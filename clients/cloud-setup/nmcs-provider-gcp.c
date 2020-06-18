// SPDX-License-Identifier: LGPL-2.1+

#include "nm-default.h"

#include "nmcs-provider-gcp.h"

#include "nm-cloud-setup-utils.h"

/*****************************************************************************/

#define HTTP_TIMEOUT_MS      3000
#define HTTP_REQ_MAX_DATA    512*1024
#define HTTP_POLL_TIMEOUT_MS 10000
#define HTTP_RATE_LIMIT_MS   1000

#define NM_GCP_HOST              "metadata.google.internal"
#define NM_GCP_BASE              "http://" NM_GCP_HOST
#define NM_GCP_API_VERSION       "/v1"
#define NM_GCP_METADATA_URL_BASE NM_GCP_BASE "/computeMetadata" NM_GCP_API_VERSION "/instance"
#define NM_GCP_METADATA_URL_NET  "/network-interfaces/"

#define NM_GCP_METADATA_HEADER   "Metadata-Flavor: Google"

#define _gcp_uri_concat(...)     nmcs_utils_uri_build_concat (NM_GCP_METADATA_URL_BASE, __VA_ARGS__)
#define _gcp_uri_interfaces(...) _gcp_uri_concat (NM_GCP_METADATA_URL_NET, ##__VA_ARGS__)

/*****************************************************************************/

struct _NMCSProviderGCP {
	NMCSProvider parent;
};

struct _NMCSProviderGCPClass {
	NMCSProviderClass parent;
};

G_DEFINE_TYPE (NMCSProviderGCP, nmcs_provider_gcp, NMCS_TYPE_PROVIDER);

/*****************************************************************************/

static void
_detect_get_meta_data_done_cb (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	gs_unref_object GTask *task = user_data;
	gs_free_error GError *get_error = NULL;
	gs_free_error GError *error = NULL;
	gboolean success;

	success = nm_http_client_poll_get_finish (NM_HTTP_CLIENT (source),
	                                          result,
	                                          NULL,
	                                          NULL,
	                                          &get_error);

	if (nm_utils_error_is_cancelled (get_error)) {
		g_task_return_error (task, g_steal_pointer (&get_error));
		return;
	}

	if (get_error) {
		nm_utils_error_set (&error,
		                    NM_UTILS_ERROR_UNKNOWN,
		                    "failure to get GCP metadata: %s",
		                    get_error->message);
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

	if (!success) {
		nm_utils_error_set (&error,
		                    NM_UTILS_ERROR_UNKNOWN,
		                    "failure to detect GCP metadata");
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

	g_task_return_boolean (task, TRUE);
}

static void
detect (NMCSProvider *provider,
        GTask *task)
{
	NMHttpClient *http_client;
	gs_free_error GError *error = NULL;
	gs_free char *uri = NULL;

	http_client = nmcs_provider_get_http_client (provider);

	if(!nm_http_client_add_header (http_client,
	                               NM_GCP_METADATA_HEADER)) {
		nm_utils_error_set (&error,
		                    NM_UTILS_ERROR_UNKNOWN,
		                    "failure in setting GCP metadata-flavor");
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

	nm_http_client_poll_get (http_client,
	                         (uri = _gcp_uri_concat ("id")),
	                         HTTP_TIMEOUT_MS,
	                         256*1024,
	                         7000,
	                         1000,
	                         g_task_get_cancellable (task),
	                         NULL,
	                         NULL,
	                         _detect_get_meta_data_done_cb,
	                         task);
}

/*****************************************************************************/

typedef struct {
	NMCSProviderGetConfigTaskData *config_data;
	guint n_ifaces_pending;
	GError *error;
	bool success:1;
} GCPData;

typedef struct {
	NMCSProviderGetConfigIfaceData *iface_get_config;
	GCPData *gcp_data;
	gint64 iface_idx;
	guint n_fips_pending;
} GCPIfaceData;

static void
_get_config_maybe_task_return (GCPData *gcp_data,
                               GError *error_take)
{
	NMCSProviderGetConfigTaskData *config_data =  gcp_data->config_data;
	gs_free_error GError *gcp_error = NULL;

	if (error_take) {
		nm_clear_error (&gcp_data->error);
		gcp_data->error = error_take;
	}

	if (gcp_data->n_ifaces_pending)
		return;

	gcp_error = gcp_data->error;

	if (!gcp_data->success) {
		nm_assert (gcp_error);

		if (nm_utils_error_is_cancelled (gcp_error))
			_LOGD ("get-config: cancelled");
		else
			_LOGD ("get-config: failed: %s", gcp_error->message);
		g_task_return_error (config_data->task, g_steal_pointer (&gcp_error));
	} else {
		_LOGD ("get-config: success");
		g_task_return_pointer (config_data->task,
		                       g_hash_table_ref (config_data->result_dict),
		                       (GDestroyNotify) g_hash_table_unref);
	}

	nm_g_slice_free (gcp_data);
	g_object_unref (config_data->task);
}

static void
_get_config_fip_cb (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GCPIfaceData *iface_data = (GCPIfaceData *) user_data;
	NMCSProviderGetConfigIfaceData *iface_get_config;
	gs_unref_variant GVariant *variant = NULL;
	GCPData *gcp_data = iface_data->gcp_data;
	gs_unref_bytes GBytes *response = NULL;
	gs_free_error GError *error = NULL;
	const char *fip_str = NULL;
	NMIPRoute **routes_arr;
	NMIPRoute *route_new;
	gboolean success;

	success = nm_http_client_poll_get_finish (NM_HTTP_CLIENT (source),
	                                          result,
	                                          NULL,
	                                          &response,
	                                          &error);

	if (   error
	    || nm_utils_error_is_cancelled (error))
		goto iface_done;

	if (!success) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "error in server response while"
		                            " fetching forwarded ip");
		goto iface_done;
	}

	fip_str = g_bytes_get_data (response, NULL);

	if (!nm_utils_ipaddr_valid (AF_INET, fip_str)) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "forwarded-ip is not a valid ip address");
		goto iface_done;
	}

	_LOGI ("GCP interface[%"G_GINT64_FORMAT"]: adding forwarded-ip %s",
	       iface_data->iface_idx,
	       fip_str);

	iface_get_config = iface_data->iface_get_config;
	iface_get_config->iface_idx = iface_data->iface_idx;
	routes_arr = iface_get_config->iproutes_arr;

	if (error)
		goto iface_done;

	variant = g_variant_new_string ("local");

	if (!nm_ip_route_attribute_validate (NM_IP_ROUTE_ATTRIBUTE_TYPE,
	                                     variant,
	                                     AF_INET,
	                                     NULL,
	                                     NULL)) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "This NetworkManager version does not support local routes");
		goto iface_done;
	}

	route_new = nm_ip_route_new (AF_INET,
	                             fip_str,
	                             32,
	                             NULL,
	                             100,
	                             &error);
	nm_ip_route_set_attribute (route_new,
	                           NM_IP_ROUTE_ATTRIBUTE_TYPE,
	                           g_steal_pointer (&variant));
	routes_arr[iface_get_config->iproutes_len] = route_new;
	++iface_get_config->iproutes_len;
	gcp_data->success = TRUE;

iface_done:
	--iface_data->n_fips_pending;
	if (iface_data->n_fips_pending == 0) {
		nm_g_slice_free (iface_data);
		--gcp_data->n_ifaces_pending;
		_get_config_maybe_task_return (gcp_data, g_steal_pointer (&error));
	}
}

static void
_g_string_free_full (GString *s)
{
	g_string_free (s, TRUE);
}

static void
_get_config_ips_list_cb (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GCPIfaceData *iface_data = (GCPIfaceData *) user_data;
	gs_unref_ptrarray GPtrArray *uri_arr = NULL;
	GCPData *gcp_data = iface_data->gcp_data;
	gs_unref_bytes GBytes *response = NULL;
	gs_free_error GError *error = NULL;
	const char *response_str = NULL;
	const char *line_start;
	const char *line_end;
	gsize response_len;
	gboolean success;
	guint i;

	success = nm_http_client_poll_get_finish (NM_HTTP_CLIENT (source),
	                                          result,
	                                          NULL,
	                                          &response,
	                                          &error);

	if (   error
	    || nm_utils_error_is_cancelled (error))
		goto fips_error;

	if (!success) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "error in server response while"
		                            " fetching forwarded ips list");
		goto fips_error;
	}

	uri_arr = g_ptr_array_new_with_free_func ((GDestroyNotify) _g_string_free_full);
	response_str = g_bytes_get_data (response, &response_len);
	line_start = response_str;

	while (   response_len > 0
	       && line_start
	       && (line_end = memchr (line_start, '\n', response_len))) {
		gint64 fip_index;
		GString *gstr;

		if (!line_end)
			continue;

		gstr = g_string_new_len (line_start, line_end - line_start);
		fip_index = _nm_utils_ascii_str_to_int64 (gstr->str, 10, 0, G_MAXINT64, -1);

		response_len -= line_end - line_start;
		line_start = line_end + 1;

		if (fip_index < 0) {
			g_string_free (gstr, TRUE);
			continue;
		}

		g_string_printf (gstr,
		                 "%"G_GINT64_FORMAT"/forwarded-ips/%"G_GINT64_FORMAT,
		                 iface_data->iface_idx,
		                 fip_index);
		g_ptr_array_add (uri_arr, gstr);
	}

	iface_data->n_fips_pending = uri_arr->len;

	_LOGI ("GCP interface[%"G_GINT64_FORMAT"]: found %u forwarded ips",
	       iface_data->iface_idx,
	       iface_data->n_fips_pending);

	if (iface_data->n_fips_pending == 0) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "found no forwarded ip");
		goto fips_error;
	}

	iface_data->iface_get_config->iproutes_arr =
	    g_new0 (NMIPRoute *, iface_data->n_fips_pending);

	for (i = 0; i < uri_arr->len; ++i) {
		GString *gstr = uri_arr->pdata[i];
		gs_free const char *uri = NULL;

		nm_http_client_poll_get (NM_HTTP_CLIENT (source),
		                         (uri = _gcp_uri_interfaces (gstr->str)),
		                         HTTP_TIMEOUT_MS,
		                         HTTP_REQ_MAX_DATA,
		                         HTTP_POLL_TIMEOUT_MS,
		                         HTTP_RATE_LIMIT_MS,
		                         g_task_get_cancellable (gcp_data->config_data->task),
		                         NULL,
		                         NULL,
		                         _get_config_fip_cb,
		                         iface_data);
	}
	return;

fips_error:
	nm_g_slice_free (iface_data);
	--gcp_data->n_ifaces_pending;
	_get_config_maybe_task_return (gcp_data, g_steal_pointer (&error));
}

static void
_get_config_iface_cb (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GCPIfaceData *iface_data = (GCPIfaceData *) user_data;
	GCPData *gcp_data = iface_data->gcp_data;
	gs_unref_bytes GBytes *response = NULL;
	gs_free_error GError *error = NULL;
	gs_free const char *hwaddr = NULL;
	gs_free const char *uri = NULL;
	gboolean success;
	GString *gstr;

	success = nm_http_client_poll_get_finish (NM_HTTP_CLIENT (source),
	                                          result,
	                                          NULL,
	                                          &response,
	                                          &error);

	if (   error
	    || nm_utils_error_is_cancelled (error))
		goto iface_error;

	if (!success) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "error in server response while"
		                            " fetching interface hwaddr");
		goto iface_error;
	}

	hwaddr = nmcs_utils_hwaddr_normalize (g_bytes_get_data (response, NULL), -1);
	iface_data->iface_get_config = g_hash_table_lookup (gcp_data->config_data->result_dict,
	                                                    hwaddr);
	if (!iface_data->iface_get_config) {
		_LOGI ("GCP interface[%"G_GINT64_FORMAT"]: did not find a matching device",
		       iface_data->iface_idx);
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "no matching hwaddr found for GCP interface");
		goto iface_error;
	}

	_LOGI ("GCP interface[%"G_GINT64_FORMAT"]: found a matching device with hwaddr %s",
	       iface_data->iface_idx,
	       hwaddr);

	gstr = g_string_new (NULL);
	g_string_printf (gstr,
	                 "%"G_GINT64_FORMAT"/forwarded-ips/",
	                 iface_data->iface_idx);

	nm_http_client_poll_get (NM_HTTP_CLIENT (source),
	                         (uri = _gcp_uri_interfaces (gstr->str)),
	                         HTTP_TIMEOUT_MS,
	                         HTTP_REQ_MAX_DATA,
	                         HTTP_POLL_TIMEOUT_MS,
	                         HTTP_RATE_LIMIT_MS,
	                         g_task_get_cancellable (gcp_data->config_data->task),
	                         NULL,
	                         NULL,
	                         _get_config_ips_list_cb,
	                        iface_data);

	g_string_free (gstr, TRUE);
	return;

iface_error:
	nm_g_slice_free (iface_data);
	--gcp_data->n_ifaces_pending;
	_get_config_maybe_task_return (gcp_data, g_steal_pointer (&error));
}

static void
_get_net_ifaces_list_cb (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	gs_unref_ptrarray GPtrArray *ifaces_arr = NULL;
	gs_unref_bytes GBytes *response = NULL;
	gs_free_error GError *error = NULL;
	GCPData *gcp_data = user_data;
	const char *response_str;
	const char *token_start;
	const char *token_end;
	const char *line_start;
	const char *line_end;
	gsize response_len;
	gboolean success;
	GString *gstr;
	guint i;

	success = nm_http_client_poll_get_finish (NM_HTTP_CLIENT (source),
	                                          result,
	                                          NULL,
	                                          &response,
	                                          &error);

	if (   error
	    || nm_utils_error_is_cancelled (error)) {
		_get_config_maybe_task_return (gcp_data, g_steal_pointer (&error));
		return;
	}

	if (!success) {
		_get_config_maybe_task_return (gcp_data, nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                                                             "error in server response"
		                                                             " while fetching interface"
		                                                             "  list"));
		return;
	}

	response_str = g_bytes_get_data (response, &response_len);
	line_start = response_str;

	ifaces_arr = g_ptr_array_new ();
	gstr = g_string_new (NULL);

	while (   response_len > 0
	       && line_start
	       && (line_end = memchr (line_start, '\n', response_len))) {
		GCPIfaceData *iface_data;
		gint64 iface_idx;

		token_start = line_start;
		token_end = memchr (token_start, '/', line_end - line_start);

		line_start = line_end + 1;
		response_len -= line_end - line_start;

		if (!token_end)
			continue;

		gstr = g_string_overwrite_len (gstr, 0, token_start, token_end - token_start);
		iface_idx = _nm_utils_ascii_str_to_int64 (gstr->str, 10, 0, G_MAXINT64, -1);

		if (iface_idx < 0)
			continue;

		iface_data = g_slice_new0 (GCPIfaceData);
		*iface_data = (GCPIfaceData) {
			.gcp_data = gcp_data,
			.iface_idx = iface_idx,
		};
		g_ptr_array_add (ifaces_arr, iface_data);
	}

	gcp_data->n_ifaces_pending = ifaces_arr->len;
	_LOGI ("found GCP interfaces: %u", ifaces_arr->len);

	for (i = 0; i < ifaces_arr->len; ++i) {
		GCPIfaceData *data = ifaces_arr->pdata[i];
		gs_free const char *uri = NULL;

		_LOGD ("GCP interface[%"G_GINT64_FORMAT"]: retrieving configuration",
		       data->iface_idx);

		g_string_printf (gstr, "%"G_GINT64_FORMAT"/mac", data->iface_idx);

		nm_http_client_poll_get (NM_HTTP_CLIENT (source),
		                         (uri = _gcp_uri_interfaces (gstr->str)),
		                         HTTP_TIMEOUT_MS,
		                         HTTP_REQ_MAX_DATA,
		                         HTTP_POLL_TIMEOUT_MS,
		                         HTTP_RATE_LIMIT_MS,
		                         g_task_get_cancellable (gcp_data->config_data->task),
		                         NULL,
		                         NULL,
		                         _get_config_iface_cb,
		                         data);

	}

	g_string_free (gstr, TRUE);

	if (ifaces_arr->len == 0) {
		error = nm_utils_error_new (NM_UTILS_ERROR_UNKNOWN,
		                            "no GCP interfaces found");
		_get_config_maybe_task_return (gcp_data, g_steal_pointer (&error));
	}
}


static void
get_config (NMCSProvider *provider,
            NMCSProviderGetConfigTaskData *get_config_data)
{
	gs_free const char *uri = NULL;
	GCPData *gcp_data;

	gcp_data = g_slice_new0 (GCPData);
	*gcp_data = (GCPData) {
		.config_data = get_config_data,
	};

	nm_http_client_poll_get (nmcs_provider_get_http_client (provider),
	                         (uri = _gcp_uri_interfaces ()),
	                         HTTP_TIMEOUT_MS,
	                         HTTP_REQ_MAX_DATA,
	                         HTTP_POLL_TIMEOUT_MS,
	                         HTTP_RATE_LIMIT_MS,
	                         g_task_get_cancellable (gcp_data->config_data->task),
	                         NULL,
	                         NULL,
	                         _get_net_ifaces_list_cb,
	                         gcp_data);
}


/*****************************************************************************/

static void
nmcs_provider_gcp_init (NMCSProviderGCP *self)
{
}

static void
nmcs_provider_gcp_class_init (NMCSProviderGCPClass *klass)
{
	NMCSProviderClass *provider_class = NMCS_PROVIDER_CLASS (klass);

	provider_class->_name                 = "GCP";
	provider_class->_env_provider_enabled = NMCS_ENV_VARIABLE ("NM_CLOUD_SETUP_GCP");
	provider_class->detect                = detect;
	provider_class->get_config            = get_config;
}

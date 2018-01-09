// SPDX-License-Identifier: LGPL-2.1+
/*
 * Copyright (C) 2017, 2018 Red Hat, Inc.
 */

#include "nm-default.h"

#include "nm-json.h"

#include <dlfcn.h>

typedef struct {
	NMJsonVt vt;
	void *dl_handle;
} JsonVt;

static JsonVt *
json_vt (void)
{
	JsonVt *vt = NULL;
	void *handle;
	int mode;

	vt = g_new (JsonVt, 1);
	*vt = (JsonVt) { };

	mode = RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE | RTLD_DEEPBIND;
#if defined (ASAN_BUILD)
	/* Address sanitizer is incompatible with RTLD_DEEPBIND. */
	mode &= ~RTLD_DEEPBIND;
#endif
	handle = dlopen (JANSSON_SONAME, mode);

	if (!handle)
		return vt;

#define TRY_BIND_SYMBOL(symbol) \
	G_STMT_START { \
		typeof (symbol) (*_sym) = dlsym (handle, #symbol); \
		\
		if (!_sym) \
			goto fail_symbol; \
		vt->vt.nm_##symbol = _sym; \
	} G_STMT_END

	TRY_BIND_SYMBOL (json_array);
	TRY_BIND_SYMBOL (json_array_append_new);
	TRY_BIND_SYMBOL (json_array_get);
	TRY_BIND_SYMBOL (json_array_size);
	TRY_BIND_SYMBOL (json_delete);
	TRY_BIND_SYMBOL (json_dumps);
	TRY_BIND_SYMBOL (json_false);
	TRY_BIND_SYMBOL (json_integer);
	TRY_BIND_SYMBOL (json_integer_value);
	TRY_BIND_SYMBOL (json_loads);
	TRY_BIND_SYMBOL (json_object);
	TRY_BIND_SYMBOL (json_object_del);
	TRY_BIND_SYMBOL (json_object_get);
	TRY_BIND_SYMBOL (json_object_iter);
	TRY_BIND_SYMBOL (json_object_iter_key);
	TRY_BIND_SYMBOL (json_object_iter_next);
	TRY_BIND_SYMBOL (json_object_iter_value);
	TRY_BIND_SYMBOL (json_object_key_to_iter);
	TRY_BIND_SYMBOL (json_object_set_new);
	TRY_BIND_SYMBOL (json_object_size);
	TRY_BIND_SYMBOL (json_string);
	TRY_BIND_SYMBOL (json_string_value);
	TRY_BIND_SYMBOL (json_true);

	vt->vt.loaded = TRUE;
	vt->dl_handle = handle;
	return vt;

fail_symbol:
	dlclose (&handle);
	*vt = (JsonVt) { };
	return vt;
}

const NMJsonVt *_nm_json_vt = NULL;

const NMJsonVt *
_nm_json_vt_init (void)
{
	NMJsonVt *vt;

again:
	vt = g_atomic_pointer_get ((gpointer *) &_nm_json_vt);
	if (G_UNLIKELY (!vt)) {
		JsonVt *v;

		v = json_vt ();
		if (!g_atomic_pointer_compare_and_exchange ((gpointer *) &_nm_json_vt, NULL, v)) {
			if (v->dl_handle)
				dlclose (v->dl_handle);
			g_free (v);
			goto again;
		}
		vt = &v->vt;
	}

	nm_assert (vt && vt == g_atomic_pointer_get (&_nm_json_vt));
	return vt;
}

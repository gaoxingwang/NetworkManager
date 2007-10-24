/* NetworkManager -- Network link manager
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * (C) Copyright 2004 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>
#include "NetworkManagerGeneric.h"
#include "NetworkManagerSystem.h"
#include "NetworkManagerUtils.h"
#include "nm-device.h"
#include "nm-device-802-3-ethernet.h"
#include "nm-device-802-11-wireless.h"
#include "NetworkManagerDialup.h"
#include "nm-utils.h"
#include "shvar.h"

/*
 * nm_system_init
 *
 * Initializes the distribution-specific system backend
 *
 */
void nm_system_init (void)
{
	nm_generic_init ();
}


/*
 * nm_system_device_flush_routes
 *
 * Flush all routes associated with a network device
 *
 */
void nm_system_device_flush_routes (NMDevice *dev)
{
	nm_generic_device_flush_routes (dev);
}


/*
 * nm_system_device_flush_routes_with_iface
 *
 * Flush all routes associated with a network device
 *
 */
void nm_system_device_flush_routes_with_iface (const char *iface)
{
	nm_generic_device_flush_routes_with_iface (iface);
}


/*
 * nm_system_device_add_default_route_via_device
 *
 * Add default route to the given device
 *
 */
void nm_system_device_add_default_route_via_device (NMDevice *dev)
{
	nm_generic_device_add_default_route_via_device (dev);
}


/*
 * nm_system_device_add_default_route_via_device_with_iface
 *
 * Add default route to the given device
 *
 */
void nm_system_device_add_default_route_via_device_with_iface (const char *iface)
{
	nm_generic_device_add_default_route_via_device_with_iface (iface);
}


/*
 * nm_system_device_add_route_via_device_with_iface
 *
 * Add route to the given device
 *
 */
void nm_system_device_add_route_via_device_with_iface (const char *iface, const char *route)
{
	nm_generic_device_add_route_via_device_with_iface (iface, route);
}


/*
 * nm_system_device_has_active_routes
 *
 * Find out whether the specified device has any routes in the routing
 * table.
 *
 */
gboolean nm_system_device_has_active_routes (NMDevice *dev)
{
	return (FALSE);
}


/*
 * nm_system_device_flush_addresses
 *
 * Flush all network addresses associated with a network device
 *
 */
void nm_system_device_flush_addresses (NMDevice *dev)
{
	nm_generic_device_flush_addresses (dev);
}


/*
 * nm_system_device_flush_addresses_with_iface
 *
 * Flush all network addresses associated with a network device
 *
 */
void nm_system_device_flush_addresses_with_iface (const char *iface)
{
	nm_generic_device_flush_addresses_with_iface (iface);
}


#if 0
	/* Alert other computers of our new address */
	temp_addr.s_addr = addr;
	buf = g_strdup_printf ("/sbin/arping -q -A -c 1 -I %s %s", iface, inet_ntoa (temp_addr));
	nm_spawn_process (buf);
	g_free (buf);
	g_usleep (G_USEC_PER_SEC * 2);
	buf = g_strdup_printf ("/sbin/arping -q -U -c 1 -I %s %s", iface, inet_ntoa (temp_addr));
	nm_spawn_process (buf);
	g_free (buf);
#endif


/*
 * nm_system_enable_loopback
 *
 * Bring up the loopback interface
 *
 */
void nm_system_enable_loopback (void)
{
	nm_generic_enable_loopback ();
}


/*
 * nm_system_flush_loopback_routes
 *
 * Flush all routes associated with the loopback device, because it
 * sometimes gets the first route for ZeroConf/Link-Local traffic.
 *
 */
void nm_system_flush_loopback_routes (void)
{
	nm_generic_flush_loopback_routes ();
}


/*
 * nm_system_delete_default_route
 *
 * Remove the old default route in preparation for a new one
 *
 */
void nm_system_delete_default_route (void)
{
	nm_generic_delete_default_route ();
}


/*
 * nm_system_flush_arp_cache
 *
 * Flush all entries in the arp cache.
 *
 */
void nm_system_flush_arp_cache (void)
{
	nm_generic_flush_arp_cache ();
}


/*
 * nm_system_kill_all_dhcp_daemons
 *
 * Kill all DHCP daemons currently running, done at startup.
 *
 */
void nm_system_kill_all_dhcp_daemons (void)
{
}


/*
 * nm_system_update_dns
 *
 * Invalidate the nscd host cache, if it exists, since
 * we changed resolv.conf.
 *
 */
void nm_system_update_dns (void)
{
	if (g_file_test ("/usr/sbin/nscd", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE | G_FILE_TEST_IS_REGULAR)) {
		nm_spawn_process ("/etc/init.d/nscd condrestart");
		nm_spawn_process ("/usr/sbin/nscd -i hosts");
	}
}


/*
 * nm_system_restart_mdns_responder
 *
 * Restart the multicast DNS responder so that it knows about new
 * network interfaces and IP addresses.
 *
 */
void nm_system_restart_mdns_responder (void)
{
	FILE 		*fp  = NULL;

	if ((fp = fopen ("/var/run/mDNSResponder.pid", "rt")))
	{
		int pid;
		int res = fscanf (fp, "%d", &pid);
		fclose (fp);
		if (res == 1)
		{
			nm_info ("Restarting mDNSResponder.");
			kill (pid, SIGUSR1);
		}
	}
}


/*
 * nm_system_device_add_ip6_link_address
 *
 * Add a default link-local IPv6 address to a device.
 *
 */
void nm_system_device_add_ip6_link_address (NMDevice *dev)
{
	nm_generic_device_add_ip6_link_address (dev);
}


typedef struct RHSystemConfigData
{
	NMIP4Config *	config;
	gboolean		use_dhcp;
	gboolean		system_disabled;
} RHSystemConfigData;


/*
 * get_current_profile_name
 *
 * Retrieve the current network profile, if any
 *
 */
static char *get_current_profile_name (void)
{
	shvarFile *	file;
	char *		buf;

	if (!(file = svNewFile (SYSCONFDIR"/sysconfig/network")))
		return NULL;

	buf = svGetValue (file, "CURRENT_PROFILE");
	if (!buf)
		buf = strdup ("default");
	svCloseFile (file);

	return buf;
}


/*
 * nm_system_device_get_system_config
 *
 * Read in the config file for a device.
 *
 */
void *nm_system_device_get_system_config (NMDevice *dev)
{
	char *				cfg_file_path = NULL;
	shvarFile *			file;
	char *				buf = NULL;
	RHSystemConfigData *	sys_data = NULL;
	gboolean				error = FALSE;

	g_return_val_if_fail (dev != NULL, NULL);

	/* Red Hat/Fedora Core systems store this information in
	 * /etc/sysconfig/network-scripts/ifcfg-* where * is the interface
	 * name.
	 */

	sys_data = g_malloc0 (sizeof (RHSystemConfigData));
	sys_data->use_dhcp = TRUE;

	cfg_file_path = g_strdup_printf (SYSCONFDIR"/sysconfig/network-scripts/ifcfg-%s", nm_device_get_iface (dev));
	if (!cfg_file_path)
		return sys_data;

	if (!(file = svNewFile (cfg_file_path)))
	{
		g_free (cfg_file_path);
		return sys_data;
	}
	g_free (cfg_file_path);

	/* Make sure this config file is for this device */
	buf = svGetValue (file, "DEVICE");
	if (!buf || strcmp (buf, nm_device_get_iface (dev)))
	{
		free (buf);
		goto out;
	}

	if ((buf = svGetValue (file, "BOOTPROTO")))
	{
		if (strcasecmp (buf, "dhcp"))
			sys_data->use_dhcp = FALSE;
		free (buf);
	}

	if ((buf = svGetValue (file, "NM_CONTROLLED")))
	{
		nm_debug ("NM_CONTROLLED=%s", buf);
		if (!strcasecmp (buf, "no"))
		{
			nm_info ("System configuration disables device %s", nm_device_get_iface (dev));
			sys_data->system_disabled = TRUE;
		}
		free (buf);
	}		

	sys_data->config = nm_ip4_config_new ();

	if (!(sys_data->use_dhcp))
	{
		if ((buf = svGetValue (file, "IPADDR")))
		{
			nm_ip4_config_set_address (sys_data->config, inet_addr (buf));
			free (buf);
		}
		else
		{
			nm_warning ("Network configuration for device '%s' was invalid (non-DHCP configuration, "
						"but no IP address specified.  Will use DHCP instead.", nm_device_get_iface (dev));
			error = TRUE;
			goto out;
		}

		if ((buf = svGetValue (file, "GATEWAY")))
		{
			nm_ip4_config_set_gateway (sys_data->config, inet_addr (buf));
			free (buf);
		}
		else
		{
			nm_warning ("Network configuration for device '%s' was invalid (non-DHCP configuration, "
						"but no gateway specified.  Will use DHCP instead.", nm_device_get_iface (dev));
			error = TRUE;
			goto out;
		}

		if ((buf = svGetValue (file, "NETMASK")))
		{
			nm_ip4_config_set_netmask (sys_data->config, inet_addr (buf));
			free (buf);
		}
		else
		{
			guint32	addr = nm_ip4_config_get_address (sys_data->config);

			/* Make a default netmask if we have an IP address */
			if (((ntohl (addr) & 0xFF000000) >> 24) <= 127)
				nm_ip4_config_set_netmask (sys_data->config, htonl (0xFF000000));
			else if (((ntohl (addr) & 0xFF000000) >> 24) <= 191)
				nm_ip4_config_set_netmask (sys_data->config, htonl (0xFFFF0000));
			else
				nm_ip4_config_set_netmask (sys_data->config, htonl (0xFFFFFF00));
		}

		if ((buf = svGetValue (file, "BROADCAST")))
		{
			nm_ip4_config_set_broadcast (sys_data->config, inet_addr (buf));
			free (buf);
		}
		else
		{
			guint32 broadcast = ((nm_ip4_config_get_address (sys_data->config) & nm_ip4_config_get_netmask (sys_data->config))
									| ~nm_ip4_config_get_netmask (sys_data->config));
			nm_ip4_config_set_broadcast (sys_data->config, broadcast);
		}
	}

	/* If we're using Static IP, grab DNS servers from the profile's config file */
	if (!sys_data->use_dhcp)
	{
		char * cur_profile_name = get_current_profile_name ();

		if (cur_profile_name)
		{
			char *filename = g_strdup_printf (SYSCONFDIR"/sysconfig/networking/profiles/%s/resolv.conf", cur_profile_name);
			
			nm_generic_set_ip4_config_from_resolv_conf (filename, sys_data->config);
			g_free (filename);
			g_free (cur_profile_name);
		}
	}	

#if 0
	nm_debug ("------ Config (%s)", nm_device_get_iface (dev));
	nm_debug ("    DHCP=%d\n", sys_data->use_dhcp);
	nm_debug ("    ADDR=%d\n", nm_ip4_config_get_address (sys_data->config));
	nm_debug ("    GW=%d\n", nm_ip4_config_get_gateway (sys_data->config));
	nm_debug ("    NM=%d\n", nm_ip4_config_get_netmask (sys_data->config));
	nm_debug ("---------------------\n");
#endif

out:
	svCloseFile (file);

	if (error)
	{
		sys_data->use_dhcp = TRUE;
		/* Clear out the config */
		g_object_unref (sys_data->config);
		sys_data->config = NULL;
	}

	return (void *)sys_data;
}


/*
 * nm_system_device_free_system_config
 *
 * Free stored system config data
 *
 */
void nm_system_device_free_system_config (NMDevice *dev, void *system_config_data)
{
	RHSystemConfigData *sys_data = (RHSystemConfigData *)system_config_data;

	g_return_if_fail (dev != NULL);

	if (!sys_data)
		return;

	if (sys_data->config)
		g_object_unref (sys_data->config);
}


/*
 * nm_system_device_get_use_dhcp
 *
 * Return whether the distro-specific system config tells us to use
 * dhcp for this device.
 *
 */
gboolean nm_system_device_get_use_dhcp (NMDevice *dev)
{
	RHSystemConfigData	*sys_data;

	g_return_val_if_fail (dev != NULL, TRUE);

	if ((sys_data = nm_device_get_system_config_data (dev)))
		return sys_data->use_dhcp;

	return TRUE;
}


/*
 * nm_system_device_get_disabled
 *
 * Return whether the distro-specific system config tells us to use
 * dhcp for this device.
 *
 */
gboolean nm_system_device_get_disabled (NMDevice *dev)
{
	RHSystemConfigData *sys_data;

	g_return_val_if_fail (dev != NULL, FALSE);

	if ((sys_data = nm_device_get_system_config_data (dev)))
		return sys_data->system_disabled;

	return FALSE;
}


NMIP4Config *nm_system_device_new_ip4_system_config (NMDevice *dev)
{
	RHSystemConfigData	*sys_data;
	NMIP4Config		*new_config = NULL;

	g_return_val_if_fail (dev != NULL, NULL);

	if ((sys_data = nm_device_get_system_config_data (dev)))
		new_config = nm_ip4_config_copy (sys_data->config);

	return new_config;
}


void nm_system_deactivate_all_dialup (GSList *list)
{
	GSList *l;
	
	for (l = list; l; l = g_slist_next(l))
	{
		NMDialUpConfig *config = (NMDialUpConfig *) l->data;
		char *cmd;
		
		cmd = g_strdup_printf ("/sbin/ifdown %s", (char *)config->data);
		nm_spawn_process (cmd);
		g_free(cmd);
	}
}


gboolean nm_system_deactivate_dialup (GSList *list, const char *dialup)
{
	GSList *l;
	gboolean ret = FALSE;
	
	for (l = list; l; l = g_slist_next (l))
	{
		NMDialUpConfig *config = (NMDialUpConfig *) l->data;
		if (strcmp (dialup, config->name) == 0)
		{
			char *cmd;
			int status;
			
			nm_info ("Dectivating dialup device %s (%s) ...", dialup, (char *) config->data);
			cmd = g_strdup_printf ("/sbin/ifdown %s", (char *) config->data);
			status = nm_spawn_process (cmd);
			g_free (cmd);
			if (status == 0) {
				ret = TRUE;
			} else {
				/* FIXME: Decode errors into something sensible */
				nm_warning ("Couldn't deactivate dialup device %s (%s) - %d", dialup, (char *) config->data, status);
				ret = FALSE;
			}
			break;
		}
	}
	return ret;
}


gboolean nm_system_activate_dialup (GSList *list, const char *dialup)
{
	GSList *l;
	gboolean ret = FALSE;
	
	for (l = list; l; l = g_slist_next (l))
	{
		NMDialUpConfig *config = (NMDialUpConfig *) l->data;
		if (strcmp (dialup, config->name) == 0)
		{
			char *cmd;
			int status;
			
			nm_info ("Activating dialup device %s (%s) ...", dialup, (char *) config->data);
			cmd = g_strdup_printf ("/sbin/ifup %s", (char *) config->data);
			status = nm_spawn_process (cmd);
			g_free (cmd);
			if (status == 0) {
				ret = TRUE;
			} else {
				/* FIXME: Decode errors into something sensible */
				nm_warning ("Couldn't activate dialup device %s (%s) - %d", dialup, (char *) config->data, status);
				ret = FALSE;
			}
			break;
		}
	}
	return ret;
}


static gboolean is_valid_cfg(const char *cfg)
{
	char *exts[] = { ".rpmsave", ".rpmorig", ".rpmnew", "~", ".bak", NULL };
	int x;
	
	if (!g_str_has_prefix (cfg, "ifcfg-"))
		return FALSE;
	for (x = 0 ; exts[x] ; x++) {
		if (g_str_has_suffix(cfg,exts[x]))
			return FALSE;
	}
	return TRUE;
}

static char *get_config_name(const char *cfg)
{
	char *ret = NULL;
	shvarFile *cfg_file;
	char *path, *tmp;
	gboolean is_dialup = FALSE;
	
	path = g_strdup_printf ("/etc/sysconfig/network-scripts/%s", cfg);
	cfg_file = svNewFile (path);
	g_free (path);
	if (!cfg_file)
		return NULL;
	
	
	/* Check for a dialup TYPE */
	tmp = svGetValue (cfg_file, "TYPE");
	if (tmp)
	{
		if ((strcmp (tmp, "Modem") == 0) ||
		    (strcmp (tmp, "ISDN") == 0))
			is_dialup = TRUE;
		free(tmp);
	}
	
	/* Check for a dialup BOOTPROTO */
	tmp = svGetValue (cfg_file, "BOOTPROTO");
	if (tmp)
	{
		if ((strcmp (tmp, "dialup") == 0))
			is_dialup = TRUE;
		free(tmp);
	}

	/* Check for a dialup device name */
	if (g_str_has_prefix(cfg+6, "ppp") || g_str_has_prefix(cfg+6, "ippp"))
		is_dialup = TRUE;
	
	if (!is_dialup)
		goto out_close;
	
	/* PROVIDER isn't actually used, but s-c-network writes it. */
	tmp = svGetValue (cfg_file, "PROVIDER");
	if (tmp)
	{
		ret = tmp;
		goto out_close;
	}
	
	/* NAME isn't used either... */
	tmp = svGetValue (cfg_file, "NAME");
	if (tmp)
	{
		ret = tmp;
		goto out_close;
	}
	
	tmp = svGetValue (cfg_file, "WVDIALSECT");
	if (tmp)
	{
		ret = tmp;
		goto out_close;
	}
	
	/* Fallback - use the config*/
	ret = strdup(cfg + 6);
	
out_close:
	svCloseFile (cfg_file);
	return ret;
	
}

GSList * nm_system_get_dialup_config (void)
{
	GSList *list = NULL;
	GDir *dir;
	GError *err;
	const char *entry;
	
	dir = g_dir_open ("/etc/sysconfig/network-scripts", 0, &err);
	if (!dir)
	{
		nm_warning ("Could not open directory /etc/sysconfig/network-scripts: %s", err->message);
		return NULL;
	}
	
	while ((entry = g_dir_read_name(dir)))
	{
		NMDialUpConfig *config;
		char *buf;
		
		if (!is_valid_cfg(entry))
			continue;
		
		buf = get_config_name(entry);
		
		if (!buf)
			continue;
		
		config = g_malloc (sizeof (NMDialUpConfig));
		config->name = g_strdup_printf ("%s via Modem", buf);
		config->data = g_strdup (entry + 6);
		list = g_slist_append (list, config);
		
		nm_info ("Found dial up configuration for %s: %s", config->name, (char *) config->data);
		
		free (buf);
			
	}
	
	g_dir_close (dir);
	
	return list;
}

/*
 * nm_system_activate_nis
 *
 * set up the nis domain and write a yp.conf
 *
 */
void nm_system_activate_nis (NMIP4Config *config)
{
}

/*
 * nm_system_shutdown_nis
 *
 * shutdown ypbind
 *
 */
void nm_system_shutdown_nis (void)
{
}

/*
 * nm_system_set_hostname
 *
 * set the hostname
 *
 */
void nm_system_set_hostname (NMIP4Config *config)
{
}


/*
 * nm_system_should_modify_resolv_conf
 *
 * Can NM update resolv.conf, or is it locked down?
 */
gboolean nm_system_should_modify_resolv_conf (void)
{
	return TRUE;
}


/*
 * nm_system_get_mtu
 *
 * Return a user-provided or system-mandated MTU for this device or zero if
 * no such MTU is provided.
 */
guint32 nm_system_get_mtu (NMDevice *dev)
{
	return 0;
}

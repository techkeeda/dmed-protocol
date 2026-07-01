/*
 * luabind.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef LUABIND_H_
#define LUABIND_H_
/* Note: not used in product -- only for testing on other platforms 
 * Cribbed from $TOPDIR/ooma/luabind/
 */

#include <glib.h>

struct ap_item_ {
	/* SSID should not be more than 31 characters */
	gchar *ssid;
	gchar *security;
	guint32 rssi;
};

typedef struct ap_item_ ap_item_t;

struct device_conf_ {
	gchar *board_name;
	gboolean inservice;
	gchar *service_type;
	gboolean wifi_scan_inprogress;
	GSList *ssid_list;
	/* g_slist_length method iterates
	 * through the whole list to find the length.
	 * we store the length of list's for faster processing.
	 */
	guint ssid_list_length;
	GSList *scan_list;
	guint scan_list_length;
	guint32 build_version;
	int rssi;
};

typedef struct device_conf_ dev_conf_t;


dev_conf_t* device_config_create(void);
void device_config_release(dev_conf_t *config);

/* every method returns boolean indicating whether
 * the operation is success or failure.
 *
 * methods fill device_conf_t structure with the return data.
 * callers should retrieve data from device_conf_t
 * only if method return indicates success.
 */

gboolean get_board_name(dev_conf_t *config);
gboolean get_device_status(dev_conf_t *config);
gboolean get_configured_wifi_list(dev_conf_t *config);
gboolean configure_wifi(const gchar *ssid, const gchar *passphrase, int index, int hidden);
gboolean delete_configured_ssid(const gchar *ssid);
gboolean start_wifi_scan_sync(void);
gboolean start_wifi_scan_async(void);
gboolean start_connection_test(const gchar *ssid);
gboolean get_signal_strength(dev_conf_t *config, const gchar *ssid);
gboolean get_wifi_scan_list(dev_conf_t *config);

#endif /* LUABIND_H_ */

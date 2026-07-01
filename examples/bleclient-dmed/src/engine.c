/*
 * engine.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include "adapter-generated.h"
#include "objectmanager-generated.h"

#define LOCKOUT_TIMER_VALUE 60

dev_conf_t *config;
btapp_state_t state;

/* removes pairing information of connected device.
 * bluez doesn't allow advertisement registration, if a BLE
 * connection is already established.
 */
void remove_device(void)
{
	GError *error = NULL;
	OrgBluezAdapter1 *proxy;

	if (!state.remote_object_path) {
		return;
	}

	proxy = org_bluez_adapter1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.adapter_path,
			NULL, &error);
	if (error != NULL) {
		btclient_error("Adapter1 proxy failed %s", error->message);
		g_error_free(error);
		return;
	}

	org_bluez_adapter1_call_remove_device_sync(proxy,
			state.remote_object_path,
			NULL, NULL);

	g_object_unref(proxy);
}

void disconnect_device(void)
{
	GError *error = NULL;
	if (!state.remote_object_path) {
		return;
	}
	OrgBluezDevice1 *proxy;

	proxy = org_bluez_device1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.remote_object_path,
			NULL, &error);
	if (error != NULL) {
		btclient_error("%s", error->message);
		return;
	}

	btclient_info("disconnecting connected device");
	/* TODO karlo: next is an interesting note: */
	/* not always disconnect succeeds. */

	org_bluez_device1_call_disconnect(proxy,
			NULL, NULL, NULL);

	g_object_unref(proxy);
}

static void flush_notification_value(void)
{
	gchar *value = "{ }";
	GVariant *variant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
			(gconstpointer)value,
			strlen(value),
			sizeof(gchar));

	/* flush previous PropertiesChanged value
	 * on both cleartext service and encryption service.
	 */
	org_bluez_gatt_characteristic1_set_value(characteristic_interface, variant);

	if (FSM_TEST_BIT(CLEARTXT_ENABLED)) {
		variant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
				(gconstpointer)value,
				strlen(value),
				sizeof(gchar));
		org_bluez_gatt_characteristic1_set_value(cleartxt_characteristic_interface, variant);
	}
}

void disconnection_cleanup(void)
{
	if (FSM_TEST_BIT(KEY_GENERATED)) {
		g_byte_array_free(state.encryption_key, TRUE);
		state.encryption_key = NULL;
	}

	FSM_CLEAR_BIT(KEY_GENERATED);
	FSM_CLEAR_BIT(DEVICE_CONNECTED);

	btclient_info("LE connection disconnected %s\n",
			state.remote_object_path);

	/* send a dummy JSON to flush
	 * the last value of PropertyChanged signal
	 */
	flush_notification_value();
	config->wifi_scan_inprogress = FALSE;
	release_ssid_scan_list();
	g_free(state.remote_object_path);
	state.remote_object_path = NULL;
}

gboolean lockout_timeout(gpointer user_data)
{
	FSM_CLEAR_BIT(RESPONSE_LOCKOUT);
	btclient_info("re-registering services\n");
	register_service();
	register_advertisement();
	return FALSE;
}

void start_lockout_timer(void)
{
	g_timeout_add_seconds(bleclient_cfg.lockout_timeout, lockout_timeout, NULL);
}

void response_lockout(void)
{
	remove_device();
	disconnection_cleanup();
	unregister_advertisement();
	unregister_service();
	start_lockout_timer();
	btclient_info("Unregistered services. Response locked out\n");
	FSM_SET_BIT(RESPONSE_LOCKOUT);
}

/* bluez doesn't persist power settings.
 * but there is a configuration parameter in bluetooth.conf
 * which can set it.
 * No harm if we try to power on again.
 */
void power_adapter(void)
{

	GError *error = NULL;
	OrgFreedesktopDBusProperties *proxy;

	proxy = org_freedesktop_dbus_properties_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.adapter_path,
			NULL, &error);
	if (proxy == NULL) {
		btclient_error("unable to get proxy %s", error->message);
		g_error_free(error);
		return;
	}

	GVariant *arg = g_variant_new_boolean(TRUE);
	GVariant *value = g_variant_new("v", arg);

	org_freedesktop_dbus_properties_call_set_sync(proxy,
			ADAPTER_INTERFACE,
			"Powered",
			value,
			NULL, &error);
	if (error != NULL) {
		btclient_error("error while turning on adapter %s : %s\n",
				state.adapter_path, error->message);
		g_error_free(error);
	}

	g_object_unref(proxy);
}

/* This method is to check if the adapter is valid or not. 
 * We are getting adapter vendor id from sysfs.
 * This will help in case of multiple adapters attached to device.
 */

gboolean check_valid_adapter(const char *ble_object_path)
{
	GString *str = g_string_new(NULL);
	gboolean ret = FALSE;

	gchar *ble_path = ble_object_path;
	gchar **path = g_strsplit(ble_path, "/", 0);

	if(path[3]){
		const gchar * ooma_board_name_env = "OOMA_BOARD_NAME";
		gchar *board_name = getenv(ooma_board_name_env);
		if (board_name == NULL) {
			btclient_error("Required environment variable not found: %s\n", ooma_board_name_env);
			return FALSE;
		}

		/* For VID=1286 */
		if((g_strcmp0(board_name, "TeloAir") == 0) || 
			(g_strcmp0(board_name, "TeloLE") == 0) || 
			(g_strcmp0(board_name, "CygnusV5") == 0)) {
				
			/* Here 1286 is vendor id for marvell chip comes inbuilt in TeloAir and TeloLE */ 
			gint rc = script_runner(&str, "cat /sys/class/bluetooth/%s/device/uevent | grep 1286", path[3]);
			if (rc == 0) {
				ret = TRUE;
			}
		} 
		
		/* For VID=13d3 */
		if (g_strcmp0(board_name, "TeloAir") == 0) {
			/* Here 13d3 is vendor id for realtek chip comes inbuilt in new TeloAir */ 
			gint rc = script_runner(&str, "cat /sys/class/bluetooth/%s/device/uevent | grep 13d3", path[3]);
			if (rc == 0) {
				ret = TRUE;
			}
		} /* This is on MrsPotts, don't bother with Puskas */
		/* For VID=226a */
		if (g_strcmp0(board_name, "Mrspotts") == 0 || g_strcmp0(board_name, "SunDial") == 0) {
			gint rc = script_runner(&str, "cat /sys/class/bluetooth/%s/device/uevent | grep 226a", path[3]);
			if (rc == 0) {
				ret = TRUE;
			}
		}
		/* This is on imx8mn, As bluetooth is UART interface, no VID */
		if ((g_strcmp0(board_name, "EVK3") == 0) || (g_strcmp0(board_name, "EVK4") == 0)  || (g_strcmp0(board_name, "JelloAir") == 0) || (g_strcmp0(board_name, "Jello") == 0)
		|| (g_strcmp0(board_name, "PuddingAir") == 0) || (g_strcmp0(board_name, "Pudding") == 0)) {
			ret = TRUE;
		}
	}
	g_string_free(str, TRUE);
	g_strfreev(path);
	return ret;
}

/* This method is to get the default adapter object path
 * The object path which has org.bluez.Adapter1 interface
 * and validated with correct VID is considered as default object path.
 */
void get_default_adapter_path(void)
{
	GError *error = NULL;
	GVariant *out_objects;
	gboolean ret = FALSE;
	OrgFreedesktopDBusObjectManager *proxy;

	proxy = org_freedesktop_dbus_object_manager_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			"/",
			NULL,
			&error);
	if (error != NULL) {
		btclient_error("Unable to Object Manager proxy %s\n", error->message);
		g_error_free(error);
		g_assert(FALSE);
		return;
	}

	ret = org_freedesktop_dbus_object_manager_call_get_managed_objects_sync(
			proxy,
			&out_objects,
			NULL,
			&error);
	if (!ret) {
		g_assert(FALSE);
		return;
	}

	const gchar* type = g_variant_get_type_string(out_objects);
	if (g_strcmp0(type, "a{oa{sa{sv}}}") != 0) {
		g_assert(FALSE);
		return;
	}

	GVariantIter iter, inner;
	GVariant *child, *inner_child, *sub_child;
	gchar *key;
	gboolean flag = TRUE;

	g_variant_iter_init(&iter, out_objects);

	/* flag is to avoid further looping once the default adapter is found */
	while ((child = g_variant_iter_next_value(&iter)) && flag) {
		g_variant_iter_init(&inner, child);
		inner_child = g_variant_iter_next_value(&inner);
		const gchar *object_path = g_variant_get_string(inner_child, NULL);

		sub_child = g_variant_iter_next_value(&inner);
		GVariantIter inner_iter;
		g_variant_iter_init(&inner_iter, sub_child);

		while (g_variant_iter_loop(&inner_iter, "{sa{sv}}", &key, NULL)) {
			if (g_strcmp0(key, ADAPTER_INTERFACE) == 0) {	
				if(check_valid_adapter(object_path)) {
					state.adapter_path = g_strdup(object_path);
					btclient_info("Default adapter path: %s", state.adapter_path);
					flag = FALSE;
					g_free(key);
					break;
				}
			}
		}
		g_variant_unref(child);
	}

	g_object_unref(proxy);
	if(state.adapter_path == NULL) {
		btclient_error("default hci adapter not found. bleclient exiting.");
		assert(state.adapter_path && "hci adapter not found");
	}
#if 0
	/* This is how I prototyped using CSR-4.0 BLE dongle instead of AW-CB375 */
	printf("karlo jam in hci name as hci1\n");
	state.adapter_path = "/org/bluez/hci1";
#endif
}

void write_to_connected_device(gchar *data)
{
	GError *error = NULL;
	char buf[256];
	OrgBluezGattCharacteristic1 *proxy;

	g_assert(state.remote_object_path != NULL);
	sprintf(buf, "%s/%s", state.remote_object_path, "service0/char0");

	proxy = org_bluez_gatt_characteristic1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			buf, NULL, NULL);
	g_assert(proxy != NULL);

	GVariantDict *child = g_variant_dict_new(NULL);
	g_variant_dict_insert_value(child, "offset", g_variant_new_string("0"));
	g_variant_dict_insert_value(child, "device", g_variant_new_object_path(buf));
	GVariant *arg_options = g_variant_dict_end(child);

	GVariant *variant_data = g_variant_new_bytestring (data);

	org_bluez_gatt_characteristic1_call_write_value_sync(proxy,
			variant_data, arg_options,
			NULL, &error);
	if (error != NULL) {
		btclient_info("failed %s : %s\n", __func__, error->message);
	}

	g_object_unref(proxy);
}

/* This method notifies response to remote device.
 * This method should only be called, when need to notify
 * response to a JSON request.
 */
void send_response(gchar* response, gboolean is_cleartext)
{
	GVariant *variant;

	g_assert(response != NULL);

	if (FSM_TEST_BIT(MSG_TRACING_ENABLED)) {
		btclient_info("response: %s\n", response);
	}

	if (is_cleartext) {
		variant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
				(gconstpointer)response,
				strlen(response),
				sizeof(guint8));
		org_bluez_gatt_characteristic1_set_value(cleartxt_characteristic_interface, variant);
	} else {
		GString *str = g_string_new(response);
		GByteArray *encrypted = encrypt_data(str);
		g_string_free(str, TRUE);
		variant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
				(gconstpointer)encrypted->data,
				encrypted->len,
				sizeof(guint8));
		org_bluez_gatt_characteristic1_set_value(characteristic_interface, variant);
		g_byte_array_free(encrypted, TRUE);
	}


#if 0
	g_dbus_interface_skeleton_flush(G_DBUS_INTERFACE_SKELETON (characteristic_properties));
#endif

}

void create_btmgmt_watcher(void)
{
	GIOChannel *io_channel = create_mgmt_channel();
	if (io_channel == NULL) {
		return;
	}

	g_io_add_watch(io_channel,
			G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
			bt_mgmt_event_handler, NULL);

	state.mgmt_channel = io_channel;
}

void engine_init(void)
{
	get_default_adapter_path();
	power_adapter();

	load_config();
	apply_configuration_settings();
	/* feature file config is for developers */
	feature_config_init();

	encryption_init();

	register_dbus_agent();
	request_default_agent();

	if (FSM_TEST_BIT(CLEARTXT_ENABLED))
		create_cleartxt_dbus_objects();

	register_service();
	register_advertisement();
	create_btmgmt_watcher();
	dmed_init();
	config = device_config_create();

	do_idle();
	g_timeout_add_seconds(1, fsm_timeout, NULL);
}

void engine_cleanup(void)
{
	disconnect_device();
	unregister_advertisement();
	unregister_service();
	if (FSM_TEST_BIT(CLEARTXT_ENABLED))
		dbus_cleartxt_object_cleanup();

	g_io_channel_unref(state.mgmt_channel);
	g_free(state.adapter_path);
	device_config_release(config);
	g_free(state.remote_object_path);

}

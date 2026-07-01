/*
 * advertisement.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

GVariant* get_advertisement_properties(void)
{
	GVariantDict dict, child_dict;
	GVariant *arg, *dict_variant;

	g_variant_dict_init(&dict, NULL);
	g_variant_dict_insert(&dict, "Type", "s", "peripheral");

	const gchar *uuid_values[] = { SERVICE_UUID, DMED_BEACON_UUID, DMED_CARD_SVC_UUID, NULL};
	arg = g_variant_new_strv(uuid_values, -1);

	g_variant_dict_insert_value(&dict, "ServiceUUIDs", arg);

	GString *tag = get_hostname_check_digit_tag();

	arg = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
			(gconstpointer)tag->str,
			tag->len,
			sizeof(guint8));
	g_string_free(tag, TRUE);

	g_variant_dict_init(&child_dict, NULL);
	/* as per bluetooth.com, manufacturer id assigned
	 * to marvel is 0x0048. manufacturer id is 2 bytes.
	 */
	g_variant_dict_insert_value(&child_dict, "0x0048", arg);
	dict_variant = g_variant_dict_end(&child_dict);

	g_variant_dict_insert_value(&dict, "ManufacturerData", dict_variant);
	dict_variant = g_variant_dict_end(&dict);

	return dict_variant;
}

void leadvertising_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{

	GError *error = NULL;
	OrgBluezLEAdvertisingManager1 *proxy = (OrgBluezLEAdvertisingManager1 *)user_data;

	org_bluez_leadvertising_manager1_call_register_advertisement_finish(proxy,
			res, &error);
	if(error != NULL) {
		btclient_error("Unable to register advertisement %s\n", error->message);
		g_error_free(error);
		assert(0);
	}

	btclient_info("LE advertisement registered");
	g_object_unref(proxy);
}

void register_advertisement(void)
{
	GError *error = NULL;
	GVariantDict dict;
	GVariant *dict_variant;
	OrgBluezLEAdvertisingManager1 *advertisement_manager;

	advertisement_manager = org_bluez_leadvertising_manager1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.adapter_path,
			NULL,
			&error);
	if(error != NULL) {
		btclient_error("%s : %s\n", __func__, error->message);
		g_error_free(error);
		return;
	}

	g_variant_dict_init(&dict, NULL);
	dict_variant = g_variant_dict_end(&dict);

	org_bluez_leadvertising_manager1_call_register_advertisement(advertisement_manager,
			ADVERTISE_PATH,
			dict_variant,
			NULL,
			leadvertising_callback,
			advertisement_manager);

}

void unregister_advertisement(void)
{
	GError *error = NULL;
	OrgBluezLEAdvertisingManager1 *advertisement_manager;

	advertisement_manager = org_bluez_leadvertising_manager1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.adapter_path,
			NULL,
			&error);
	if(error != NULL) {
		btclient_error("Failed getting proxy object for LEAdvertisingManager1 %s\n", error->message);
		return;
	}

	org_bluez_leadvertising_manager1_call_unregister_advertisement_sync (
			advertisement_manager,
			ADVERTISE_PATH,
			NULL,
			&error);
	if(error != NULL) {
		btclient_error("%s : %s\n", __func__, error->message);
	}

	g_object_unref(advertisement_manager);
	btclient_info("LE advertisement unregistered");
}

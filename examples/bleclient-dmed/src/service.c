/*
 * service.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

const gchar *characteristic_flags[] = {
		"notify",
		"write",
		NULL
};

const gchar *descriptor_flags[] = {
		"read",
		NULL
};

void change_encryption_setting(void)
{
	/* if device is in response lockout
	 * state, ignore the inotify event.
	 */
	if (FSM_TEST_BIT(RESPONSE_LOCKOUT)) {
		return;
	}

	/* If we are here, then cleartext service
	 * objects are either created or not been created.
	 */
	if (FSM_TEST_BIT(CLEARTXT_ENABLED)) {
			create_cleartxt_dbus_objects();
	} else {
		dbus_cleartxt_object_cleanup();
	}

	unregister_advertisement();
	unregister_service();

	sleep(2);

	register_service();
	register_advertisement();
}

GVariant *get_characteristic_properties(gboolean is_cleartext)
{
	GVariantDict dict;
	GVariant *uuid_arg, *obj_path_arg, *arg;
	GVariant *dict_variant;

	g_variant_dict_init(&dict, NULL);

	if (is_cleartext) {
		uuid_arg = g_variant_new_string(CLEAR_TEXT_CHARACTERISTIC_UUID);
		obj_path_arg = g_variant_new_object_path (CLEARTXT_SERVICE_OBJ_PATH);
	} else {
		uuid_arg = g_variant_new_string(CHARACTERISTIC_UUID);
		obj_path_arg = g_variant_new_object_path (SERVICE_OBJ_PATH);
	}

	g_variant_dict_insert_value(&dict, "UUID", uuid_arg);
	g_variant_dict_insert_value(&dict, "Service", obj_path_arg);

	arg = g_variant_new_strv(characteristic_flags, -1);
	g_variant_dict_insert_value(&dict, "Flags", arg);

	dict_variant = g_variant_dict_end(&dict);
	return dict_variant;
}

GVariant *get_descriptor_properties(gboolean is_cleartext)
{
	GVariantDict dict;
	GVariant *uuid_arg, *obj_path_arg, *arg;
	GVariant *dict_variant;

	g_variant_dict_init(&dict, NULL);

	if (is_cleartext) {
		uuid_arg = g_variant_new_string(CLEAR_TEXT_DESCRIPTOR_UUID);
		obj_path_arg = g_variant_new_object_path (CLEARTXT_CHARACTERISTICS_OBJ_PATH);
	} else {
		uuid_arg = g_variant_new_string(DESCRIPTOR_UUID);
		obj_path_arg = g_variant_new_object_path (CHARACTERISTICS_OBJ_PATH);
	}

	g_variant_dict_insert_value(&dict, "UUID", uuid_arg);
	g_variant_dict_insert_value(&dict, "Characteristic", obj_path_arg);

	arg = g_variant_new_strv(descriptor_flags, -1);
	g_variant_dict_insert_value(&dict, "Flags", arg);

	dict_variant = g_variant_dict_end(&dict);
	return dict_variant;
}

GVariant *get_descriptor_properties_2(gboolean is_cleartext)
{
	GVariantDict dict;
	GVariant *uuid_arg, *obj_path_arg, *arg;
	GVariant *dict_variant;

	g_variant_dict_init(&dict, NULL);

	if (is_cleartext) {
		uuid_arg = g_variant_new_string(CLEAR_TEXT_DESCRIPTOR_UUID2);
		obj_path_arg = g_variant_new_object_path (CLEARTXT_CHARACTERISTICS_OBJ_PATH);
	} else {
		uuid_arg = g_variant_new_string(DESCRIPTOR_UUID2);
		obj_path_arg = g_variant_new_object_path (CHARACTERISTICS_OBJ_PATH);
	}

	g_variant_dict_insert_value(&dict, "UUID", uuid_arg);
	g_variant_dict_insert_value(&dict, "Characteristic", obj_path_arg);

	arg = g_variant_new_strv(descriptor_flags, -1);
	g_variant_dict_insert_value(&dict, "Flags", arg);

	dict_variant = g_variant_dict_end(&dict);
	return dict_variant;
}

GVariant *get_service_properties(gboolean is_cleartext)
{

	GVariantDict dict;
	GVariant *arg, *dict_variant;

	g_variant_dict_init(&dict, NULL);

	if (is_cleartext) {
		arg = g_variant_new_string(CLEAR_TEXT_SERVICE_UUID);
	} else {
		arg = g_variant_new_string(SERVICE_UUID);
	}
	g_variant_dict_insert_value(&dict, "UUID", arg);

	g_variant_dict_insert_value(&dict, "Primary", g_variant_new_boolean(TRUE));

	dict_variant = g_variant_dict_end(&dict);

	return dict_variant;
}

void gatt_manager_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	OrgBluezGattManager1 *gatt_manager = (OrgBluezGattManager1 *)user_data;

	org_bluez_gatt_manager1_call_register_application_finish(gatt_manager, res, &error);
	if(error != NULL) {
		btclient_error("%s : %s\n", __func__, error->message);
		g_error_free(error);
		g_object_unref(gatt_manager);
		assert(0);
		return;
	}

	btclient_debug("successfully registered custom BLE service(s) with GATT manager");

	g_object_unref(gatt_manager);
}

void register_service(void)
{

	GVariantDict dict;
	GVariant *dict_variant;
	OrgBluezGattManager1 *gatt_manager;

	/* This gatt_manager object is unref'ed
	 * in its finish callback.
	 */
	gatt_manager = org_bluez_gatt_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			state.adapter_path,
			NULL,
			NULL);
	if(gatt_manager == NULL) {
		/* This condition may occur, if bluez is not running.
		 * if bluez is not running, bleclient doesn't
		 * run.
		 */
		btclient_error("Get proxy on org.bluez.GattManager1 failed\n");
		return;
	}

	g_variant_dict_init(&dict, NULL);
	dict_variant = g_variant_dict_end(&dict);

	org_bluez_gatt_manager1_call_register_application(gatt_manager,
			APPLICATION_PATH,
			dict_variant,
			NULL,
			gatt_manager_callback,
			gatt_manager);

}

void unregister_service(void)
{
	GError *error = NULL;
	OrgBluezGattManager1 *gatt_manager;

	gatt_manager = org_bluez_gatt_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
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

	org_bluez_gatt_manager1_call_unregister_application_sync(gatt_manager,
			APPLICATION_PATH,
			NULL,
			&error);
	if (error != NULL) {
		btclient_error("%s : %s\n", __func__, error->message);
		g_error_free(error);
	}

	g_object_unref(gatt_manager);
}

/*
 * dbus-support.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include <gio/gunixfdlist.h>

#define PROFILE_VERSION "version:2"

#define MIN_MSG_LEN	32  // IV length + TAG length + 2
GMainLoop *mainloop;

OrgFreedesktopDBusObjectManager *object_manager;
OrgFreedesktopDBusProperties *advertisement_properties;
OrgFreedesktopDBusProperties *service_properties;
OrgFreedesktopDBusProperties *characteristic_properties;
OrgFreedesktopDBusProperties *descriptor_properties1;
OrgFreedesktopDBusProperties *descriptor_properties2;
OrgBluezGattCharacteristic1 *characteristic_interface;
OrgBluezGattDescriptor1 *descriptor_interface1;
OrgBluezGattDescriptor1 *descriptor_interface2;
OrgOomaLteservice *lteservice_interface;

OrgFreedesktopDBusProperties *cleartxt_service_properties;
OrgFreedesktopDBusProperties *cleartxt_characteristic_properties;
OrgFreedesktopDBusProperties *cleartxt_descriptor_properties1;
OrgFreedesktopDBusProperties *cleartxt_descriptor_properties2;
OrgBluezGattCharacteristic1 *cleartxt_characteristic_interface;
OrgBluezGattDescriptor1 *cleartxt_descriptor_interface1;
OrgBluezGattDescriptor1 *cleartxt_descriptor_interface2;

/* DMED GATT interfaces */
OrgFreedesktopDBusProperties *dmed_card_svc_props;
OrgBluezGattCharacteristic1 *dmed_chr_len_iface;
OrgBluezGattCharacteristic1 *dmed_chr_data_iface;
OrgFreedesktopDBusProperties *dmed_act_svc_props;
OrgBluezGattCharacteristic1 *dmed_chr_req_iface;
OrgBluezGattCharacteristic1 *dmed_chr_rsp_iface;

static guint bluez_watch_id;
static guint wifi_watch_id;
guint bus_id;

void handle_properties_changed(GDBusConnection *connection, const gchar *sender_name,
		const gchar *object_path, const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{

	gchar *interface;
	gboolean value;

	/* parameters are of type (sa{sv}as) */
	const gchar *type = g_variant_get_type_string(parameters);
	if (g_strcmp0(type, "(sa{sv}as)") != 0) {
		/* This is highly unexpected */
		btclient_error("Unexpected parameters received in PropertiesChanged signal\n");
		assert(FALSE);
	}

	/* & indicates a borrowed string. should not be freed
	 */
	g_variant_get_child(parameters, 0, "&s", &interface);

	if(g_strcmp0(interface, DEVICE_INTERFACE) == 0) {
		/* child variant is never floating and need to free */
		GVariant *child = g_variant_get_child_value(parameters, 1);
		if(g_variant_lookup(child, "Connected", "b", &value)) {
			if (!value && g_strcmp0(object_path, state.remote_object_path) == 0) {
				disconnection_cleanup();
			} else if (value) {
				;
			}
		}
		g_variant_unref(child);
	}
}

/*
 * new interface gets added
 * TODO: May not need this listener anymore.
 */
void interface_added(GDBusConnection *connection, const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{

	const gchar *signature = g_variant_get_type_string(parameters);
	if(g_strcmp0(signature, "(oa{sa{sv}})")) {
		btclient_error("Unknown signature on InterfaceAdded signal\n");
		return;
	}

	gboolean value;
	GVariantIter iter;
	gchar *object, *interface;
	GVariant *child, *child1;

	g_variant_get_child(parameters, 0, "&o", &object);

	/* we may need to unref return value of g_variant_get_child_value */
	child1 = g_variant_get_child_value(parameters, 1);
	g_variant_iter_init(&iter, child1);

	while((child = g_variant_iter_next_value(&iter)) != NULL) {
		g_variant_get_child(child, 0, "&s", &interface);
		if (g_strcmp0(interface, DEVICE_INTERFACE) == 0) {
			GVariant *property = g_variant_get_child_value(child, 1);
			if (g_variant_lookup(property, "Connected", "b", &value)) {
				if (value) {
					g_variant_unref(property);
					break;
				}
			}
			g_variant_unref(property);
		}
		g_variant_unref(child);
	}

	g_variant_unref(child1);
}

void handle_wifi_scan_property(GDBusConnection *connection, const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{
	gchar *interface;
	gboolean value;

	/* parameters are of type (sa{sv}as) */
	const gchar *type = g_variant_get_type_string(parameters);
	if (g_strcmp0(type, "(sa{sv}as)") != 0) {
		btclient_error("Unexpected parameters received in PropertiesChanged signal\n");
		assert(0);
	}

	g_variant_get_child(parameters, 0, "&s", &interface);

	if(g_strcmp0(interface, "fi.w1.wpa_supplicant1.Interface") == 0) {
		/* child variant is never floating and need to free */
		GVariant *child = g_variant_get_child_value(parameters, 1);
		if(g_variant_lookup(child, "Scanning", "b", &value)) {
			if (!value && config->wifi_scan_inprogress) {
				btclient_info("wifi scanning done\n");
				config->wifi_scan_inprogress = FALSE;
			}

		}
		g_variant_unref(child);
	}

}

static void dbus_btclient_signal_filter(void)
{

	GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

	g_assert(conn != NULL);

	g_dbus_connection_signal_subscribe(conn,
			"org.bluez",
			"org.freedesktop.DBus.Properties",
			"PropertiesChanged", NULL,
			NULL, G_DBUS_SIGNAL_FLAGS_NONE, handle_properties_changed, NULL, NULL);

	g_dbus_connection_signal_subscribe(conn,
			"org.bluez",
			"org.freedesktop.DBus.ObjectManager",
			"InterfacesAdded", NULL,
			NULL, G_DBUS_SIGNAL_FLAGS_NONE, interface_added, NULL, NULL);

	g_dbus_connection_signal_subscribe(conn,
			"fi.w1.wpa_supplicant1",
			"org.freedesktop.DBus.Properties",
			"PropertiesChanged", NULL,
			NULL, G_DBUS_SIGNAL_FLAGS_NONE, handle_wifi_scan_property, NULL, NULL);
}

gboolean dbus_init(void)
{
	mainloop = g_main_loop_new(NULL, FALSE);

	bus_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, BTCLIENT_BUS_NAME,
			G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT|
			G_BUS_NAME_OWNER_FLAGS_REPLACE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost, mainloop, NULL);

	return TRUE;
}

static void build_managed_objects_variant(GVariantBuilder *parent, gboolean is_cleartext)
{
	GVariant *properties, *variant;
	GVariantBuilder child;

	/* @ in front of a type string,
	 * takes a non-NULL pointer to GVariant and uses
	 * its value instead of collecting args to create the value.
	 * @ - consumes floating reference count.
	 */
	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = get_service_properties(is_cleartext);
	g_variant_builder_add(&child, "{s@a{sv}}", SERVICE_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	if (is_cleartext) {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", CLEARTXT_SERVICE_OBJ_PATH, variant);
	}
	else {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", SERVICE_OBJ_PATH, variant);
	}

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = get_characteristic_properties(is_cleartext);
	g_variant_builder_add(&child, "{s@a{sv}}", CHARACTERISTICS_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	if (is_cleartext) {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", CLEARTXT_CHARACTERISTICS_OBJ_PATH, variant);
	} else {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", CHARACTERISTICS_OBJ_PATH, variant);
	}

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = get_descriptor_properties(is_cleartext);
	g_variant_builder_add(&child, "{s@a{sv}}", DESCRIPTOR_INTERFACE, properties);
	variant = g_variant_builder_end(&child);

	if (is_cleartext) {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", CLEARTXT_DESCRIPTOR_OBJ_PATH, variant);
	}
	else {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", DESCRIPTOR_OBJ_PATH, variant);
	}

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = get_descriptor_properties_2(is_cleartext);
	g_variant_builder_add(&child, "{s@a{sv}}", DESCRIPTOR_INTERFACE, properties);
	variant = g_variant_builder_end(&child);

	if (is_cleartext) {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", CLEARTXT_DESCRIPTOR_OBJ_PATH2, variant);
	}
	else {
		g_variant_builder_add(parent, "{o@a{sa{sv}}}", DESCRIPTOR_OBJ_PATH2, variant);
	}

}

/* once application registers a GATT service with bluez,
 * bluez calls GetManagedObjects on the application.
 * this should return the complete hierarchy of GATT Service.
 */
gboolean on_handle_get_managed_objects(OrgFreedesktopDBusObjectManager *object,
		GDBusMethodInvocation *invocation)
{
	GVariant *variant;
	GVariantBuilder parent;

	btclient_info("Registering all GATT services with bluez");

	g_variant_builder_init(&parent, G_VARIANT_TYPE_ARRAY);

	btclient_info("Registering encryption enabled GATT service");
	build_managed_objects_variant(&parent, FALSE);

	if (FSM_TEST_BIT(CLEARTXT_ENABLED)) {
		btclient_info("Registering cleartext enabled GATT service");
		build_managed_objects_variant(&parent, TRUE);
	}

	/* DMED Card Service */
	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_card_svc_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", SERVICE_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_SVC_CARD_PATH, variant);

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_card_len_chr_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", CHARACTERISTICS_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_CHR_LEN_PATH, variant);

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_card_data_chr_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", CHARACTERISTICS_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_CHR_DATA_PATH, variant);

	/* DMED Action Service */
	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_act_svc_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", SERVICE_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_SVC_ACT_PATH, variant);

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_act_req_chr_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", CHARACTERISTICS_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_CHR_REQ_PATH, variant);

	g_variant_builder_init(&child, G_VARIANT_TYPE_ARRAY);
	properties = dmed_get_act_rsp_chr_properties();
	g_variant_builder_add(&child, "{s@a{sv}}", CHARACTERISTICS_INTERFACE, properties);
	variant = g_variant_builder_end(&child);
	g_variant_builder_add(&parent, "{o@a{sa{sv}}}", DMED_CHR_RSP_PATH, variant);

	/* final_variant signature: a{oa{sa{sv}} */
	variant = g_variant_builder_end(&parent);
	org_freedesktop_dbus_object_manager_complete_get_managed_objects(object, invocation, variant);

	return TRUE;
}

gboolean on_handle_get_properties(OrgFreedesktopDBusProperties *object, GDBusMethodInvocation *invocation,
		const gchar *arg_interface, const gchar *arg_property)
{

	btclient_info("%s\n",__func__);
	btclient_info("get property of: %s on interface %s\n", arg_property, arg_interface);

	return TRUE;
}

gboolean on_handle_get_all_properties(OrgFreedesktopDBusProperties *object,
	    GDBusMethodInvocation *invocation, const gchar *arg_interface)
{
	GVariant *dict_variant;

	const gchar *method_obj_path = g_dbus_method_invocation_get_object_path(invocation);
	btclient_info("method of object path %s invoked", method_obj_path);

	if (g_strcmp0(arg_interface, ADVERTISING_INTERFACE) == 0) {
		dict_variant = get_advertisement_properties();
		goto done;
	} else if(g_strcmp0(arg_interface, SERVICE_INTERFACE) == 0) {
		if (g_strcmp0(method_obj_path, CLEARTXT_SERVICE_OBJ_PATH) == 0) {
			dict_variant = get_service_properties(TRUE);
			goto done;
		} else {
			dict_variant = get_service_properties(FALSE);
			goto done;
		}
	} else if(g_strcmp0(arg_interface, CHARACTERISTICS_INTERFACE) == 0) {
		if (g_strcmp0(method_obj_path, CLEARTXT_CHARACTERISTICS_OBJ_PATH) == 0) {
				dict_variant = get_characteristic_properties(TRUE);
				goto done;
			} else {
				dict_variant = get_characteristic_properties(FALSE);
				goto done;
			}
	} else if(g_strcmp0(arg_interface, DESCRIPTOR_INTERFACE) == 0) {
		if (g_strcmp0(method_obj_path, CLEARTXT_DESCRIPTOR_OBJ_PATH) == 0) {
			dict_variant = get_descriptor_properties(TRUE);
			goto done;
		} else {
			dict_variant = get_descriptor_properties(FALSE);
			goto done;
		}
	}

	return FALSE;
done:
	org_freedesktop_dbus_properties_complete_get_all(object, invocation, dict_variant);
	return TRUE;
}

/* MTU supported with bluez-5 is 517 bytes */
gboolean on_read_characteristic_value(OrgBluezGattCharacteristic1 *object, GDBusMethodInvocation *invocation,
		GVariant *arg_options)
{

	btclient_info("%s\n", __func__);
	org_bluez_gatt_characteristic1_complete_read_value(object, invocation, "ooma");
	return TRUE;
}

/* both cleartext and encryption services
 * has the same callback for GATT operations.
 * method differentiates service as per their object paths.
 */
gboolean on_write_characteristic_value(OrgBluezGattCharacteristic1 *object,
		GDBusMethodInvocation *invocation,
		GVariant *arg_data,
		GVariant *arg_options)
{

	gchar *obj_path;
	gsize no_elements;
	gboolean is_cleartext = TRUE;
	GByteArray *byte_array;

	const gchar *method_obj_path = g_dbus_method_invocation_get_object_path(invocation);
	if (g_strcmp0(method_obj_path, CLEARTXT_CHARACTERISTICS_OBJ_PATH) == 0) {
		is_cleartext = TRUE;
	} else {
		/* we expect only two known object path */
		is_cleartext = FALSE;
	}

	g_variant_lookup(arg_options, "device", "&o", &obj_path);
	gconstpointer data = g_variant_get_fixed_array(arg_data, &no_elements, sizeof(guchar));
	if (state.remote_object_path == NULL) {
		state.remote_object_path = g_strdup(obj_path);
	}
	FSM_SET_BIT(DEVICE_CONNECTED);

	org_bluez_gatt_characteristic1_complete_write_value(object, invocation);

	byte_array = g_byte_array_sized_new(no_elements);
	g_byte_array_append(byte_array, (const guint8 *)data, no_elements);

	if (is_cleartext) {
		goto no_encrypt;
	}

	if (!FSM_TEST_BIT(KEY_GENERATED)) {
		btclient_error("wrong operation sequence from device %s\n", obj_path);
		goto lockout;
	}
	/* msg length should be atleast more than 30 bytes */
	if (no_elements < MIN_MSG_LEN) {
		btclient_error("unexpected message length %d\n", no_elements);
		goto lockout;
	}

	/* This methods decrypt's data and returns plain text
	 * in a new byte array. it consumes the byte array passed
	 * to it as argument.
	 */
	byte_array = decrypt_data(byte_array);
	if (byte_array == NULL) {
		goto lockout;
	}

no_encrypt:
	if(!process_json_request(byte_array, is_cleartext)) {
		goto lockout;
	}

	g_byte_array_free(byte_array, TRUE);
	return TRUE;

lockout:
	if (byte_array != NULL)
		g_byte_array_free(byte_array, TRUE);
	response_lockout();
	return TRUE;
}

gboolean on_handle_start_notify(OrgBluezGattCharacteristic1 *object, GDBusMethodInvocation *invocation)
{

	btclient_debug("BLE start notify");
	org_bluez_gatt_characteristic1_complete_start_notify(object, invocation);
	return TRUE;
}

gboolean on_handle_stop_notify(OrgBluezGattCharacteristic1 *object, GDBusMethodInvocation *invocation)
{

	btclient_debug("BLE stop notify");
	org_bluez_gatt_characteristic1_complete_stop_notify(object, invocation);
	return TRUE;
}

gboolean on_read_descriptor2_value(OrgBluezGattDescriptor1 *object, GDBusMethodInvocation *invocation,
		GVariant *arg_flags)
{

	GVariant *rand = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
			PROFILE_VERSION,
			strlen(PROFILE_VERSION),
			sizeof(guint8));

	org_bluez_gatt_descriptor1_complete_read_value(object, invocation, rand);
	return TRUE;
}

gboolean on_read_descriptor_value(OrgBluezGattDescriptor1 *object, GDBusMethodInvocation *invocation,
		GVariant *arg_flags)
{
	GByteArray *rand_array;

	const gchar *method_obj_path = g_dbus_method_invocation_get_object_path(invocation);

	if (g_strcmp0(method_obj_path, CLEARTXT_DESCRIPTOR_OBJ_PATH2) == 0) {
		gchar *msg = g_strdup_printf("Read operation not permitted on descriptor, when encryption is disabled");
		g_dbus_method_invocation_return_error(invocation, BLUEZ_ERROR,
				BLUEZ_ERROR_NOTPERMITTED, msg,
				g_dbus_method_invocation_get_sender(invocation));
		btclient_error("%s", msg);
		g_free(msg);
		return TRUE;
	}

	rand_array = byte_array_get_random(16);

	GVariant *rand = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
			(gconstpointer)rand_array->data,
			rand_array->len,
			sizeof(guint8));

	org_bluez_gatt_descriptor1_complete_read_value(object, invocation, rand);

	if (state.encryption_key != NULL) {
		g_byte_array_free(state.encryption_key, TRUE);
		state.encryption_key = NULL;
	}

	/* memory passed to this method is consumed */
	state.encryption_key = generate_encryption_key(rand_array);

	btclient_info("Encryption key generated\n");
	FSM_SET_BIT(KEY_GENERATED);

	return TRUE;
}

gboolean on_write_descriptor_value(OrgBluezGattDescriptor1 *object,
		GDBusMethodInvocation *invocation,
		const gchar *arg_value,
		GVariant *arg_flags)
{
	btclient_info("Service Descriptor write operation should not be allowed\n");

	org_bluez_gatt_descriptor1_complete_write_value(object, invocation);
	return TRUE;
}

static void hookup_cleartxt_dbus_callbacks(void)
{
	g_signal_connect(cleartxt_service_properties, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);
	g_signal_connect(cleartxt_characteristic_properties, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);
	g_signal_connect(cleartxt_descriptor_properties1, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);

	/* Characteristic interface methods */
	g_signal_connect(cleartxt_characteristic_interface, "handle-write-value", G_CALLBACK(on_write_characteristic_value), 0);
	g_signal_connect(cleartxt_characteristic_interface, "handle-start-notify", G_CALLBACK(on_handle_start_notify), 0);
	g_signal_connect(cleartxt_characteristic_interface, "handle-stop-notify", G_CALLBACK(on_handle_stop_notify), 0);


	g_signal_connect(cleartxt_descriptor_interface1, "handle-read-value", G_CALLBACK(on_read_descriptor_value), 0);
	g_signal_connect(cleartxt_descriptor_interface2, "handle-read-value", G_CALLBACK(on_read_descriptor2_value), 0);

}

static void hookup_dbus_method_callbacks(void)
{

	/* ObjectManager methods */
	g_signal_connect(object_manager, "handle-get-managed-objects", G_CALLBACK(on_handle_get_managed_objects), 0);

	/* Properties methods */
	g_signal_connect(advertisement_properties, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);
	g_signal_connect(service_properties, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);
	g_signal_connect(characteristic_properties, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);
	g_signal_connect(descriptor_properties1, "handle-get-all", G_CALLBACK(on_handle_get_all_properties), 0);

	/* Characteristic interface methods */
	g_signal_connect(characteristic_interface, "handle-write-value", G_CALLBACK(on_write_characteristic_value), 0);
	g_signal_connect(characteristic_interface, "handle-start-notify", G_CALLBACK(on_handle_start_notify), 0);
	g_signal_connect(characteristic_interface, "handle-stop-notify", G_CALLBACK(on_handle_stop_notify), 0);


	g_signal_connect(descriptor_interface1, "handle-read-value", G_CALLBACK(on_read_descriptor_value), 0);
	g_signal_connect(descriptor_interface2, "handle-read-value", G_CALLBACK(on_read_descriptor2_value), 0);
}

void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer userdata)
{

	GMainLoop *loop =(GMainLoop *) userdata;

	btclient_info("Lost D-Bus name\n");

	if (loop) {
		g_main_loop_quit(loop);
	}
}

/* This is a handler to watch bluez on dbus
 * If bluez is not running, we can not do any thing.
 */
void bluez_vanished_handler(GDBusConnection *connection, const char *name, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;

	btclient_error("bluetoothd not found on system Bus \n");
	btclient_error("Exiting bleclient\n");

	if (loop) {
			g_main_loop_quit(loop);
	}
}

void wifi_vanished_handler(GDBusConnection *connection, const char *name, gpointer data)
{
	state.wpa_supplicant_running = FALSE;
}

void wifi_appeared_handler(GDBusConnection *connection, const char *name,
		const char *name_owner, gpointer data)
{
	state.wpa_supplicant_running = TRUE;
}

/* This method is called, only if
 * cleartext mode is enabled.
 * we create service interface objects
 * and register interface method callbacks.
 */
void create_cleartxt_dbus_objects(void)
{
	GError *error = NULL;
	GDBusConnection *connection;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
	g_assert (connection != NULL);

	cleartxt_characteristic_interface = org_bluez_gatt_characteristic1_skeleton_new();
	cleartxt_descriptor_interface1 = org_bluez_gatt_descriptor1_skeleton_new();
	cleartxt_descriptor_interface2 = org_bluez_gatt_descriptor1_skeleton_new();

	cleartxt_service_properties = org_freedesktop_dbus_properties_skeleton_new();
	cleartxt_characteristic_properties = org_freedesktop_dbus_properties_skeleton_new();
	cleartxt_descriptor_properties1 = org_freedesktop_dbus_properties_skeleton_new();
	cleartxt_descriptor_properties2 = org_freedesktop_dbus_properties_skeleton_new();

	hookup_cleartxt_dbus_callbacks();

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_service_properties), connection, CLEARTXT_SERVICE_OBJ_PATH, &error)) {
		btclient_error("Unable to export BLE service path on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_characteristic_interface), connection, CLEARTXT_CHARACTERISTICS_OBJ_PATH, &error)) {
		btclient_error("Unable to export characteristic properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_characteristic_properties), connection, CLEARTXT_CHARACTERISTICS_OBJ_PATH, &error)) {
		btclient_error("Unable to export service characteristics interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_descriptor_interface1), connection, CLEARTXT_DESCRIPTOR_OBJ_PATH, &error)) {
		btclient_error("Unable to export descriptor interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_descriptor_properties1), connection, CLEARTXT_DESCRIPTOR_OBJ_PATH, &error)) {
		btclient_error("Unable to export descriptor properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_descriptor_interface2), connection, CLEARTXT_DESCRIPTOR_OBJ_PATH2, &error)) {
		btclient_error("Unable to export descriptor interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (cleartxt_descriptor_properties2), connection, CLEARTXT_DESCRIPTOR_OBJ_PATH2, &error)) {
		btclient_error("Unable to export descriptor properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	g_object_unref(connection);
	return;

cleanup:
	g_error_free(error);
	g_assert(FALSE);
}

void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer userdata)
{
	GError *error = NULL;

	object_manager = org_freedesktop_dbus_object_manager_skeleton_new();
	characteristic_interface = org_bluez_gatt_characteristic1_skeleton_new();
	descriptor_interface1 = org_bluez_gatt_descriptor1_skeleton_new();
	descriptor_interface2 = org_bluez_gatt_descriptor1_skeleton_new();

	agent_interface = org_bluez_agent1_skeleton_new();
	lteservice_interface= org_ooma_lteservice_skeleton_new();

	advertisement_properties = org_freedesktop_dbus_properties_skeleton_new();
	service_properties = org_freedesktop_dbus_properties_skeleton_new();
	characteristic_properties = org_freedesktop_dbus_properties_skeleton_new();
	descriptor_properties1 = org_freedesktop_dbus_properties_skeleton_new();
	descriptor_properties2 = org_freedesktop_dbus_properties_skeleton_new();

	hookup_dbus_method_callbacks();
	hookup_dbus_agent_callbacks();

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (object_manager), connection, APPLICATION_PATH, &error)) {
		btclient_error("Unable to export object manager on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (advertisement_properties), connection, ADVERTISE_PATH, &error)) {
		btclient_error("Unable to export advertisement path interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (service_properties), connection, SERVICE_OBJ_PATH, &error)) {
		btclient_error("Unable to export BLE service path on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (characteristic_properties), connection, CHARACTERISTICS_OBJ_PATH, &error)) {
		btclient_error("Unable to export service characteristics interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (descriptor_properties1), connection, DESCRIPTOR_OBJ_PATH, &error)) {
		btclient_error("Unable to export descriptor properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}


	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (descriptor_properties2), connection, DESCRIPTOR_OBJ_PATH2, &error)) {
		btclient_error("Unable to export descriptor properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (characteristic_interface), connection, CHARACTERISTICS_OBJ_PATH, &error)) {
		btclient_error("Unable to export characteristic properties interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (descriptor_interface1), connection, DESCRIPTOR_OBJ_PATH, &error)) {
		btclient_error("Unable to export descriptor interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (descriptor_interface2), connection, DESCRIPTOR_OBJ_PATH2, &error)) {
		btclient_error("Unable to export descriptor interface on dbus system bus %s", error->message);
		goto cleanup;
	}

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (agent_interface), connection, AGENT_OBJECT_PATH, &error)) {
		btclient_error("Unable to export agent_interface on dbus system bus %s", error->message);
		goto cleanup;
	}
	
    if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON (lteservice_interface), connection, LTESERVICE_OBJECT_PATH, &error)) {
        btclient_error("Unable to export lteservice_interface on dbus system bus %s", error->message);
        goto cleanup;
    }

	/* DMED GATT interfaces */
	dmed_card_svc_props = org_freedesktop_dbus_properties_skeleton_new();
	dmed_chr_len_iface = org_bluez_gatt_characteristic1_skeleton_new();
	dmed_chr_data_iface = org_bluez_gatt_characteristic1_skeleton_new();
	dmed_act_svc_props = org_freedesktop_dbus_properties_skeleton_new();
	dmed_chr_req_iface = org_bluez_gatt_characteristic1_skeleton_new();
	dmed_chr_rsp_iface = org_bluez_gatt_characteristic1_skeleton_new();

	g_signal_connect(dmed_chr_len_iface, "handle-read-value", G_CALLBACK(dmed_on_card_len_read), 0);
	g_signal_connect(dmed_chr_data_iface, "handle-read-value", G_CALLBACK(dmed_on_card_data_read), 0);
	g_signal_connect(dmed_chr_req_iface, "handle-write-value", G_CALLBACK(dmed_on_act_req_write), 0);
	g_signal_connect(dmed_chr_rsp_iface, "handle-read-value", G_CALLBACK(dmed_on_act_rsp_read), 0);

	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_card_svc_props), connection, DMED_SVC_CARD_PATH, &error);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_chr_len_iface), connection, DMED_CHR_LEN_PATH, &error);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_chr_data_iface), connection, DMED_CHR_DATA_PATH, &error);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_act_svc_props), connection, DMED_SVC_ACT_PATH, &error);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_chr_req_iface), connection, DMED_CHR_REQ_PATH, &error);
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dmed_chr_rsp_iface), connection, DMED_CHR_RSP_PATH, &error);

	/* we want to find bluetoothd restarts */
	bluez_watch_id = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez",
			G_BUS_NAME_WATCHER_FLAGS_AUTO_START, NULL,
			bluez_vanished_handler,
			mainloop,
			NULL);

	wifi_watch_id = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "fi.w1.wpa_supplicant1",
				G_BUS_NAME_WATCHER_FLAGS_NONE,
				wifi_appeared_handler,
				wifi_vanished_handler,
				mainloop,
				NULL);

	dbus_btclient_signal_filter();
	engine_init();
	return ;

cleanup:
	g_error_free(error);
	g_assert(FALSE);
}

void on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
    ;
}

void dbus_cleartxt_object_cleanup(void)
{

	g_object_unref(cleartxt_service_properties);

	g_object_unref(cleartxt_characteristic_properties);
	g_object_unref(cleartxt_characteristic_interface);

	g_object_unref(cleartxt_descriptor_properties1);
	g_object_unref(cleartxt_descriptor_interface1);

	g_object_unref(cleartxt_descriptor_properties2);
	g_object_unref(cleartxt_descriptor_interface2);

}

void dbus_cleanup(void)
{
	g_bus_unwatch_name(bluez_watch_id);
	g_bus_unwatch_name(wifi_watch_id);
	g_bus_unown_name(bus_id);

	g_object_unref(object_manager);
	g_object_unref(advertisement_properties);

	g_object_unref(service_properties);

	g_object_unref(characteristic_properties);
	g_object_unref(characteristic_interface);

	g_object_unref(descriptor_properties1);
	g_object_unref(descriptor_interface1);

	g_object_unref(descriptor_properties2);
	g_object_unref(descriptor_interface2);

	g_main_loop_unref(mainloop);
}

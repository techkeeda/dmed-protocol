/*
 * dbus-support.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef DBUS_SUPPORT_H_
#define DBUS_SUPPORT_H_

#include "objectmanager-generated.h"
#include "service-generated.h"
#include "advertise-generated.h"
#include "characteristic-generated.h"
#include "descriptor-generated.h"
#include "lteservice-generated.h"

extern GMainLoop *mainloop;

/* bluez interfaces and object paths */
#define BLUEZ_BUS_NAME 						"org.bluez"
#define BLUEZ_OBJECT_PATH 					"/org/bluez"
#define ADAPTER_INTERFACE					"org.bluez.Adapter1"
#define	LEMANAGER_INTERFACE					"org.bluez.LEAdvertisingManager1"
#define OBJECT_MANAGER_INTERFACE			"org.freedesktop.DBus.Properties"
#define AGENT_OBJECT_PATH					"/org/ooma/bleclient/agent"
#define LTESERVICE_OBJECT_PATH				"/org/ooma/bleclient/lteservice"

/* implemented interfaces */
/* gdbus-codegen wraps this interfaces and we don't use this names anywhere */
#define ADVERTISING_INTERFACE				"org.bluez.LEAdvertisement1"
#define SERVICE_INTERFACE					"org.bluez.GattService1"
#define CHARACTERISTICS_INTERFACE			"org.bluez.GattCharacteristic1"
#define DESCRIPTOR_INTERFACE				"org.bluez.GattDescriptor1"
#define BTCLIENT_BUS_NAME					"org.ooma.bleclient"
#define DEVICE_INTERFACE					"org.bluez.Device1"

/* bleclient object paths */
#define APPLICATION_PATH					"/org/ooma/bleclient"
#define ADVERTISE_PATH						"/org/ooma/bleclient/advertisement"
#define SERVICE_OBJ_PATH					"/org/ooma/bleclient/service0"
#define CHARACTERISTICS_OBJ_PATH			"/org/ooma/bleclient/service0/char0"
#define DESCRIPTOR_OBJ_PATH					"/org/ooma/bleclient/service0/char0/desc0"
#define DESCRIPTOR_OBJ_PATH2				"/org/ooma/bleclient/service0/char0/desc1"

#define CLEARTXT_SERVICE_OBJ_PATH			"/org/ooma/bleclient/service1"
#define CLEARTXT_CHARACTERISTICS_OBJ_PATH	"/org/ooma/bleclient/service1/char0"
#define CLEARTXT_DESCRIPTOR_OBJ_PATH		"/org/ooma/bleclient/service1/char0/desc0"
#define CLEARTXT_DESCRIPTOR_OBJ_PATH2		"/org/ooma/bleclient/service1/char0/desc1"

#define SERVICE_UUID						"ad945229-f59f-4808-a382-10b99be092ed"
#define CHARACTERISTIC_UUID					"ad945229-f59f-4808-a382-10b99be092e3"
#define DESCRIPTOR_UUID						"ad945229-f59f-4808-a382-10b99be092e5"
#define DESCRIPTOR_UUID2					"ad945229-f59f-4808-a382-10b99be092e9"

#define CLEAR_TEXT_SERVICE_UUID				"c2921afe-f369-49fa-91cf-2760fd66e03d"
#define CLEAR_TEXT_CHARACTERISTIC_UUID		"c2921afe-f369-49fa-91cf-2760fd66e033"
#define CLEAR_TEXT_DESCRIPTOR_UUID			"c2921afe-f369-49fa-91cf-2760fd66e035"
#define CLEAR_TEXT_DESCRIPTOR_UUID2			"c2921afe-f369-49fa-91cf-2760fd66e039"

/* DMED BLE service definitions */
#define DMED_BEACON_UUID                    "d14d0000-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_SVC_UUID                  "d14d0001-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_DATA_UUID                 "d14d0002-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_LEN_UUID                  "d14d0003-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_SVC_UUID                   "d14d0004-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_REQ_UUID                   "d14d0005-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_RSP_UUID                   "d14d0006-1000-4000-8000-00805f9b34fb"

#define DMED_SVC_CARD_PATH                  "/org/ooma/bleclient/dmed_card"
#define DMED_CHR_LEN_PATH                   "/org/ooma/bleclient/dmed_card/chr_len"
#define DMED_CHR_DATA_PATH                  "/org/ooma/bleclient/dmed_card/chr_data"
#define DMED_SVC_ACT_PATH                   "/org/ooma/bleclient/dmed_act"
#define DMED_CHR_REQ_PATH                   "/org/ooma/bleclient/dmed_act/chr_req"
#define DMED_CHR_RSP_PATH                   "/org/ooma/bleclient/dmed_act/chr_rsp"

extern OrgBluezGattCharacteristic1 *characteristic_interface;
extern OrgFreedesktopDBusProperties *characteristic_properties;
extern OrgBluezGattCharacteristic1 *cleartxt_characteristic_interface;
gboolean dbus_init(void);
void dbus_cleanup(void);

/* Properties headers */
gboolean on_handle_get_properties(OrgFreedesktopDBusProperties *object,
		GDBusMethodInvocation *invocation, const gchar *arg_interface,
		const gchar *arg_property);
gboolean on_handle_get_all_properties(OrgFreedesktopDBusProperties *object,
		GDBusMethodInvocation *invocation, const gchar *arg_interface);

/* ObjectManger headers */
gboolean on_handle_get_managed_objects(OrgFreedesktopDBusObjectManager *object,
		GDBusMethodInvocation *invocation);

void create_cleartxt_dbus_objects(void);
void dbus_cleartxt_object_cleanup(void);

void on_bus_acquired(GDBusConnection *connection, const gchar *name,
		gpointer userdata);
void on_name_lost(GDBusConnection *connection, const gchar *name,
		gpointer userdata);
void on_name_acquired(GDBusConnection *conn, const gchar *name,
		gpointer user_data);

#endif /* DBUS_SUPPORT_H_ */

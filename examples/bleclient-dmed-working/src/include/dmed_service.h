/*
 * dmed_service.h — DMED BLE GATT service for bleclient (notify pattern)
 */
#ifndef DMED_SERVICE_H_
#define DMED_SERVICE_H_

#include <glib.h>
#include <gio/gio.h>
#include "characteristic-generated.h"

void dmed_init(void);

/* Property getters for GetManagedObjects */
GVariant *dmed_get_svc_properties(void);
GVariant *dmed_get_chr_properties(void);

/* GATT callbacks */
gboolean dmed_on_write(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *arg_data, GVariant *opts);
gboolean dmed_on_start_notify(OrgBluezGattCharacteristic1 *obj, GDBusMethodInvocation *inv);
gboolean dmed_on_stop_notify(OrgBluezGattCharacteristic1 *obj, GDBusMethodInvocation *inv);

#endif

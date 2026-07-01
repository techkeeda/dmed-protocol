/*
 * dmed_service.h — DMED BLE GATT service for bleclient
 */
#ifndef DMED_SERVICE_H_
#define DMED_SERVICE_H_

#include <glib.h>
#include <gio/gio.h>
#include "characteristic-generated.h"

/* Initialize DMED card JSON */
void dmed_init(void);

/* GATT property getters for GetManagedObjects */
GVariant *dmed_get_card_svc_properties(void);
GVariant *dmed_get_card_len_chr_properties(void);
GVariant *dmed_get_card_data_chr_properties(void);
GVariant *dmed_get_act_svc_properties(void);
GVariant *dmed_get_act_req_chr_properties(void);
GVariant *dmed_get_act_rsp_chr_properties(void);

/* GATT read/write callbacks */
gboolean dmed_on_card_len_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts);
gboolean dmed_on_card_data_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts);
gboolean dmed_on_act_req_write(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *arg_data, GVariant *opts);
gboolean dmed_on_act_rsp_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts);

#endif

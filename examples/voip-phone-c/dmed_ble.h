/*
 * dmed_ble.h — BLE GATT server for DMED via BlueZ D-Bus API
 */
#ifndef DMED_BLE_H
#define DMED_BLE_H

#include <stdint.h>
#include <stddef.h>

typedef int (*dmed_ble_action_handler_t)(const char *action, const char *params, char *out, size_t out_len);

/* Start BLE GATT service and advertisement via bluetoothd/D-Bus.
 * Spawns a GMainLoop thread. bluetoothd must be running.
 * card_json: capability card JSON
 * handler: action dispatch callback */
int dmed_ble_start(const char *card_json, dmed_ble_action_handler_t handler);

void dmed_ble_stop(void);

#endif

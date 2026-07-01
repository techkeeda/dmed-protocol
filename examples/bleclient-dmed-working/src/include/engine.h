/*
 * engine.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef ENGINE_H_
#define ENGINE_H_

#include "luabind.h"

typedef struct btapp_state {
	gchar *adapter_path;
	gchar *remote_object_path;
	GByteArray *encryption_key;
	gchar *response;
	guint scan_timeout_id;
	gboolean wpa_supplicant_running;
	gboolean notifying;
	GIOChannel *mgmt_channel;
}btapp_state_t;

extern btapp_state_t state;
extern dev_conf_t *config;

void remove_device(void);
void disconnection_cleanup(void);
void disconnect_device(void);
void response_lockout(void);
void power_adapter(void);
void get_default_adapter_path(void);
void create_btmgmt_watcher(void);

void engine_init(void);
void engine_cleanup(void);

void send_response(gchar* response, gboolean is_cleartext);

#endif /* ENGINE_H_ */

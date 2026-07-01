/*
 * configure.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef CONFIGURE_H_
#define CONFIGURE_H_

typedef void (*request_handler)(int tid_value, json_object *data, gboolean is_cleartext);

struct request_action_ {
	const char *action;
	request_handler handler;
};

typedef struct request_action_ request_actions_t;
extern request_actions_t request_actions[];

void release_ssid_scan_list(void);
#endif /* CONFIGURE_H_ */

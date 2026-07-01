/*
 * parser.c
 *
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include "parser.h"
#include "configure.h"
#include <string.h>

static guint32 tid = 0;

gchar *request_key[] = {
		"req",
		"tid",
		"data",
		"resp",
		"code",
		"error",
		"version",
		"myxid",
		"name",
		"inservice",
		"service_type",
		"count",
		"ssid",
		"password",
		"index",
		"security",
		"result",
		"rssi",
                "lte_status",
                "hidden",
		"timestamp",
		"line_count",
		NULL
};

gchar* create_error_response(req_key_t request, status_code_t code, int tid_value)
{
	g_assert(code < INVALID_CODE);
	json_object *val_obj;

	json_object *json = json_object_new_object();
	json_object_object_add(json, request_key[REQ], json_object_new_int(request));
	json_object_object_add(json, request_key[TID], json_object_new_int(tid_value));

	val_obj = json_object_new_object();
	json_object_object_add(val_obj, request_key[CODE], json_object_new_int(code));

	json_object_object_add(json, request_key[RESP], val_obj);
	gchar *response = g_strdup(json_object_to_json_string(json));
	json_object_put(json);

	return response;
}

gchar* create_success_response(req_key_t method, status_code_t code, int tid_value,
		json_object *data_obj) {

	g_assert(code < INVALID_CODE);

	gchar *response;

	json_object *val_obj;

	json_object *json = json_object_new_object();
	json_object_object_add(json, request_key[REQ], json_object_new_int(method));
	json_object_object_add(json, request_key[TID], json_object_new_int(tid_value));
	val_obj = json_object_new_object();
	json_object_object_add(val_obj, request_key[CODE],
			json_object_new_int(code));

	if (data_obj) {
		json_object_object_add(val_obj, request_key[DATA], data_obj);
	}

	json_object_object_add(json, request_key[RESP], val_obj);

	response = g_strdup(json_object_to_json_string(json));

	json_object_put(json);

	return response;
}

/* sample request -  { "req" : 1}
 *  Not all the requests have "data" field.
 */
gboolean process_json_request(GByteArray *buffer, gboolean is_cleartext)
{
	gchar *response;
	json_object *method_obj, *tid_obj;
	guint8 terminator = '\0';

	/* json-c library performs string operations
	 * on passed JSON.
	 */
	g_byte_array_append(buffer, &terminator, 1);

	json_object *json = json_tokener_parse((const char *)buffer->data);
	if (json == NULL) {
		/* we don't want to respond to non-JSON messages.
		 * lock out the interface.
		 */
		btclient_error("received invalid JSON\n");
		return FALSE;
	}

	if (FSM_TEST_BIT(MSG_TRACING_ENABLED)) {
		btclient_info("request: %s\n", json_object_to_json_string(json));
	}

	if (!json_object_object_get_ex(json, request_key[REQ], &method_obj)) {
		btclient_error("received invalid JSON. missing req key\n");
		goto handle_error;
	}

	if (!json_object_object_get_ex(json, request_key[TID], &tid_obj)) {
		btclient_error("received invalid JSON. missing tid key\n");
		goto handle_error;
	}

	if (json_object_is_type(method_obj, json_type_int) && json_object_is_type(tid_obj, json_type_int)) {
		gint32 method_value = json_object_get_int(method_obj);
		gint32 tid_value = json_object_get_int(tid_obj);
		if (tid_value && tid_value <= tid) {
			btclient_error("unexpected tid sequence");
		}
		tid = tid_value;
		if (method_value > INVALID_METHOD && method_value < UNSUPPORTED_REQUEST) {
			json_object *data = NULL;
			json_bool result = json_object_object_get_ex(json, request_key[DATA], &data);
			if (result) {
				request_actions[method_value].handler(tid_value, data, is_cleartext);
			} else {
				request_actions[method_value].handler(tid_value, NULL, is_cleartext);
			}
		} else {
			response = create_error_response(method_value, BAD_REQUEST, tid_value);
			send_response(response, is_cleartext);
			g_free(response);
		}
	} else {
		goto handle_error;
	}

	json_object_put(json);
	return TRUE;

handle_error:
	json_object_put(json);
	return FALSE;
}

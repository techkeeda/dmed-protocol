/*
 * parser.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef PARSER_H_
#define PARSER_H_

#include <json.h>

#define INTERFACE_VERSION 1

typedef enum {
	REQ,
	TID,
	DATA,
	RESP,
	CODE,
	ERROR,
	VERSION,
	MYXID,
	NAME,
	INSERVICE,
	SERVICE_TYPE,
	SSID_COUNT,
	WIFI_SSID,
	WIFI_PASSWORD,
	INDEX,
	WIFI_SECURITY,
	RESULT,
	RSSI,
        LTE_STATUS,
        SSID_HIDDEN,
	TIMESTAMP,
	LINE_COUNT,
	UNSUPPORTED
}request_key_t;

/* method codes in request messages
 * are mapped to this enums.
 */
typedef enum {
	INVALID_METHOD,
	GET_INTERFACE_VERSION,
	GET_MYXID,
	GET_BOARD_NAME,
	GET_BUILD_NUMBER,
	GET_STATUS,
	GET_SSID_COUNT,
	INSERT_SSID,
	DELETE_SSID,
	GET_SSID,
	GET_AP_LIST_COUNT,
	START_SCAN,
	GET_AP_ITEM,
	START_WIFI_TEST,
	GET_WIFI_TEST_RESULT,
        GET_RSSI,
        GET_LTE_STATUS,
	GET_SUMMARY,
	GET_NETWORK_LED_STATUS,
    GET_FILE_NAME_LIST,
	SETECT_FILE,
    GET_LINE,
    DELETE_FILE,
    PUT_FILE,
        UNSUPPORTED_REQUEST
}req_key_t;

typedef enum {
	CONF_SUCCESS = 200,
	CONF_FAILURE = 300,
	BAD_REQUEST = 400,
	INTERNAL_ERROR = 500,
	INVALID_CODE
}status_code_t;

extern gchar *request_key[];

void change_msg_trace_setting(gboolean enabled);

/* JSON parser methods */
gchar* create_success_response(req_key_t method, status_code_t code, int tid_value, json_object *data_obj);
gchar* create_error_response(req_key_t request, status_code_t code, int tid_value);
gboolean process_json_request(GByteArray *buffer, gboolean is_cleartext);

#endif /* PARSER_H_ */

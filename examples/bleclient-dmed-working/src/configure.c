/*
 * configure.c
 *
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <sys/inotify.h>
#include <sys/poll.h>

#include <unistd.h>

#include "btclient.h"

#ifdef NTELO
/* karlo stubs for non-Telo build */
#define NETWORK_LED_BLUE    (1 << 0)
#define NETWORK_LED_GREEN   (1 << 1)
#define NETWORK_LED_RED     (1 << 2)
uint32_t util_get_network_led(void) { return 0; };
#else
/* Usual libooma_util header */
#include "ooma/util.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>

#define WIFI_TEST_FILE "/tmp/wifi_connect_test"
#define WIFI_SCAN_TIMEOUT 30
#define SUMMARY_FILE_LINE_BUF_SIZE 100
#define SUMMARY_FILE	"/var/state/summary"

const GString *FILE_DIR = "/opt/log/";
GString *SELECTED_FILE = "ltemgrlog";
extern OrgOomaLteservice *lteservice_interface;

void handle_unknown(int tid, json_object *json, gboolean is_cleartext)
{
	;
}

/* {"req" : 6, "resp" : {"code": 200, "version" : 1 }} */
void handle_get_interface_version(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[VERSION], json_object_new_int(INTERFACE_VERSION));

	response = create_success_response(GET_INTERFACE_VERSION, CONF_SUCCESS, tid, data);

	send_response(response, is_cleartext);
	g_free(response);
}

void handle_get_myxid(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	GString *pad = g_string_new(NULL);
	int rc = script_runner(&pad, "echo -n $(hostname)");
	if (rc) {
		response = create_error_response(GET_MYXID, INTERNAL_ERROR, tid);
		goto send_data;
	}

	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[MYXID], json_object_new_string(pad->str));
	response = create_success_response(GET_MYXID, CONF_SUCCESS, tid, data);

	g_string_free(pad, TRUE);

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

static void send_line(int tid, gboolean is_cleartext, gint line_number)
{
	gchar *response;
	GString *result = g_string_new(NULL);

	char cmd[100];
	sprintf(cmd, "/bin/sed -n '%ip' %stemp_%s", line_number, FILE_DIR, SELECTED_FILE);

	int rc = script_runner(&result, cmd);
	if (rc) {
		btclient_error("send_line(): No log found");
		response = create_error_response(GET_LINE, INTERNAL_ERROR, tid);
		goto send_data;
	}

	if (result->len <= 0) {
		btclient_error("send_line(): Reached EOF");
		response = create_error_response(GET_LINE, BAD_REQUEST, tid);
		goto send_data;
	}

	gchar key[10];
	g_sprintf(key, "line%i", line_number);

	json_object *data = json_object_new_object();
	json_object_object_add(data, key, json_object_new_string(result->str));

	response = create_success_response(GET_LINE, CONF_SUCCESS, tid, data);
	g_string_free(result, TRUE);


send_data:
	send_response(response, is_cleartext);
	g_free(response);
	return;
}

/*TR-1559:
	TODO: We may expand the scope of this method and make it generic
		to extract content any compressed file, given absoulte path in the file_name
*/
static void extract_file(const gchar* file_name)
{
	GString *result = g_string_new(NULL);
	const gchar* extracted_file_name = g_strndup(file_name, strlen(file_name)-3);

	char cmd[100];
	sprintf(cmd, "gzip -c -d -f %s%s > %stemp_%s", FILE_DIR, SELECTED_FILE, FILE_DIR, extracted_file_name);

	int rc = script_runner(&result, cmd);
	if (rc) {
		btclient_error("extract_file(): Problem while extracting the file");
		g_string_free(result, TRUE);
		return;
	}
	SELECTED_FILE = g_strdup(extracted_file_name);

	return;
}

/*TR-1559:
	{ "req" : 21, "tid" : 21, "data" : { "line": 1} }
*/
void handle_get_line(gint tid, json_object *json, gboolean is_cleartext)
{
	json_object *object, *data;
	gchar* response;

	if (!json_object_object_get_ex(json, "line", &object)) {
		response = create_error_response(GET_LINE, BAD_REQUEST, tid);
	}
	gint line_number = json_object_get_int(object);

	send_line(tid, is_cleartext, line_number);

	return;
}

/*TR-1559: Send the list of files present under the FILE_DIR to the client
	{ "req" : 19, "tid" : 19 }
*/
void handle_get_file_name_list(gint tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	GString *result = g_string_new(NULL);

	char cmd[128];
	g_sprintf(cmd, "/bin/ls -lrt --full-time %s |grep -v ^d |awk '{print $9}'", FILE_DIR);

	int rc = script_runner(&result, cmd);
	if (rc) {
		response = create_error_response(GET_FILE_NAME_LIST, INTERNAL_ERROR, tid);
		goto send_data;
	}

	if (result->len <= 0) {
		btclient_error("handle_get_file_name_list(): No log files found under given directory");
		response = create_error_response(GET_FILE_NAME_LIST, INTERNAL_ERROR, tid);
		goto send_data;
	}

	json_object *data = json_object_new_object();
	gchar key[6];
	gint file_number = 1;
	gchar* log_file_name = strtok(result->str, "\n");
	while (log_file_name != NULL) {
		g_sprintf(key, "file%i", file_number++);
		json_object_object_add(data, key, json_object_new_string(log_file_name));
		log_file_name = strtok(NULL, "\n");
	}

	response = create_success_response(GET_FILE_NAME_LIST, CONF_SUCCESS, tid, data);
	g_string_free(result, TRUE);

send_data:
	send_response(response, is_cleartext);
	g_free(response);

	return;
}

static gboolean append_file_metadata(json_object *json)
{
	GString *result = g_string_new(NULL);

	char cmd[100];
	g_sprintf(cmd, "/bin/ls -lrt --full-time %s%s |awk '{print $6, $7, $8}'", FILE_DIR, SELECTED_FILE);

	script_runner(&result, cmd);
	if (result->len <= 0) {
		btclient_error("append_file_metadata(): File does not exist");
		g_string_free(result, TRUE);
		return FALSE;
	}
	gchar* file_timestamp = result->str;
	// OS gives us the timestamp in the following format.
	gchar* time_format = "%Y-%m-%d %H:%M:%S %z";
	time_t et = str_to_epoch_time(file_timestamp, time_format);
	gchar epoch_time[15];
	g_sprintf(epoch_time, "%lu", et);
	json_object_object_add(json, request_key[TIMESTAMP], json_object_new_string(epoch_time));

	if (is_compressed_file(SELECTED_FILE))
		g_sprintf(cmd, "zcat %s%s |wc -l", FILE_DIR, SELECTED_FILE);
	else
		g_sprintf(cmd, "/bin/wc %s%s |awk '{print $1}'", FILE_DIR, SELECTED_FILE);

	script_runner(&result, cmd);
	if (result->len <= 0) {
		btclient_error("append_file_metadata(): Empty file");
		g_string_free(result, TRUE);
	}
	gint line_count = atoi(result->str);
	json_object_object_add(json, request_key[LINE_COUNT], json_object_new_int(line_count));

	g_string_free(result, TRUE);
	return TRUE;
}

/*TR-1559:
	{ "req" : 20, "tid" : 20, "data" : { "selected_file" : "locallog.3" } }
*/
void handle_select_file(gint tid, json_object *json, gboolean is_cleartext)
{
	json_object *object, *data;
	gchar* response;

	if (!json_object_object_get_ex(json, "selected_file", &object)) {
		response = create_error_response(SETECT_FILE, BAD_REQUEST, tid);
		goto send_data;
	}
	const gchar* file_name = json_object_get_string(object);
	SELECTED_FILE = g_strdup(file_name);
	data = json_object_new_object();

	if (!append_file_metadata(data)) {
		response = create_error_response(SETECT_FILE, BAD_REQUEST, tid);
		goto send_data;
	}

	if (is_compressed_file(SELECTED_FILE)) {
		extract_file(SELECTED_FILE);
	}
	else {
		GString *result = g_string_new(NULL);
		gchar cmd[128];
		g_sprintf(cmd, "/bin/cp %s%s %stemp_%s", FILE_DIR, SELECTED_FILE, FILE_DIR, SELECTED_FILE);
		script_runner(&result, cmd);
	}

	response = create_success_response(SETECT_FILE, CONF_SUCCESS, tid, data);

send_data:
	send_response(response, is_cleartext);
	g_free(response);

	return;
}

/*TR-1559:
	{ "req" : 22, "tid" : 22, "data" : { "selected_file" : "locallog.3" } }
*/
void handle_delete_file(gint tid, json_object *json, gboolean is_cleartext)
{
	json_object *object, *data;
	gchar* response;

	if (!json_object_object_get_ex(json, "selected_file", &object)) {
		response = create_error_response(DELETE_FILE, BAD_REQUEST, tid);
		goto send_data;
	}
	const gchar* file_name = json_object_get_string(object);

	GString *result = g_string_new(NULL);
	char cmd[128];
	g_sprintf(cmd, "/bin/rm %s%s", FILE_DIR, file_name);

	int rc = script_runner(&result, cmd);
	if (rc) {
		btclient_error("handle_delete_file(): Failed to delete the file");
		response = create_error_response(DELETE_FILE, BAD_REQUEST, tid);
		goto send_data;
	}

	response = create_success_response(DELETE_FILE, CONF_SUCCESS, tid, data);

send_data:
	send_response(response, is_cleartext);
	g_free(response);
	g_string_free(result, TRUE);

	return;
}

/*TR-1559:
	TODO: Leaving it for future implementation
*/
void handle_put_file(gint tid, json_object *json, gboolean is_cleartext)
{
	GString *response = create_error_response(PUT_FILE, BAD_REQUEST, tid);
	send_response(response, is_cleartext);
	g_free(response);

	return;
}

void handle_get_board_name(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	gboolean status = get_board_name(config);
	if (status) {
		json_object *data = json_object_new_object();
		json_object_object_add(data, request_key[NAME], json_object_new_string(config->board_name));
		response = create_success_response(GET_BOARD_NAME, CONF_SUCCESS, tid, data);
		g_free(config->board_name);
	} else {
		response = create_error_response(GET_BOARD_NAME, INTERNAL_ERROR, tid);
	}

	send_response(response, is_cleartext);
	g_free(response);
}

void handle_get_build_number(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	/* TODO: we can use C string library
	 * to avoid guing64 usage.
	 */
	guint64 build_version = 999999;
	GString *pad = g_string_new(NULL);
	int rc = script_runner(&pad, "cat /version");

	if (rc == 0) {
		build_version = g_ascii_strtoull(pad->str, NULL, 10);
	}
	g_string_free(pad, TRUE);

	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[VERSION], json_object_new_int(build_version));
	response = create_success_response(GET_BUILD_NUMBER, CONF_SUCCESS, tid, data);

	send_response(response, is_cleartext);
	g_free(response);
}

void handle_get_status(int tid, json_object *json, gboolean is_cleartext)
{
	json_object *data;
	gchar *response;

	gboolean status = get_device_status(config);
	if (status) {
		data = json_object_new_object();
		json_object_object_add(data, request_key[INSERVICE],
				json_object_new_int(config->inservice));

		json_object_object_add(data, request_key[SERVICE_TYPE],
				json_object_new_string(config->service_type));

		response = create_success_response(GET_STATUS, CONF_SUCCESS, tid, data);

		g_free(config->service_type);
	} else {
		response = create_error_response(GET_STATUS, INTERNAL_ERROR, tid);
	}

	send_response(response, is_cleartext);
	g_free(response);
}

static gboolean rebuild_configured_wifi_list(void)
{
	gboolean rc;
	g_slist_free_full(config->ssid_list, (GDestroyNotify) g_free);
	config->ssid_list = NULL;

	gboolean status = get_configured_wifi_list(config);
	if (status) {
		rc = TRUE;
	} else {
		btclient_error("failed building configured ssid list");
		rc = FALSE;
	}

	return rc;
}

void handle_get_ssid_count(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;

	gboolean status = rebuild_configured_wifi_list();
	if (status) {
		json_object *data = json_object_new_object();
		json_object_object_add(data, request_key[SSID_COUNT], json_object_new_int(config->ssid_list_length));
		response = create_success_response(GET_SSID_COUNT, CONF_SUCCESS, tid, data);
	} else {
		response = create_error_response(GET_SSID_COUNT, INTERNAL_ERROR, tid);
	}

	send_response(response, is_cleartext);
	g_free(response);
}

void handle_insert_ssid(int tid, json_object *json, gboolean is_cleartext)
{
	gchar* response;
	json_object *object;
	const gchar *password = NULL;

	int index = 1;
	int hidden = 0;

	if (!json_object_object_get_ex(json, request_key[WIFI_SSID], &object)) {
		response = create_error_response(INSERT_SSID, BAD_REQUEST, tid);
		goto send_data;
	}
	const gchar *ssid = json_object_get_string(object);

	if (json_object_object_get_ex(json, request_key[WIFI_PASSWORD], &object)) {
		password = json_object_get_string(object);
	}

	if (json_object_object_get_ex(json, request_key[INDEX], &object)) {
		index = json_object_get_int(object);
	}

	if (config->ssid_list) {
		if (index < 0  || index > g_slist_length(config->ssid_list)) {
			response = create_error_response(INSERT_SSID, BAD_REQUEST, tid);
			goto send_data;
		}
	}

	if (json_object_object_get_ex(json, request_key[SSID_HIDDEN], &object)) {
		btclient_info("configuring hidden wifi network");
		hidden = json_object_get_int(object);
	}

	/* Lua array index starts with 1. Lua don't understand 0 index */
	gboolean status = configure_wifi(ssid, password, index + 1, hidden);
	if (status) {
		rebuild_configured_wifi_list();
		response = create_success_response(INSERT_SSID, CONF_SUCCESS, tid, NULL);
	} else {
		response = create_error_response(INSERT_SSID, INTERNAL_ERROR, tid);
	}

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

void handle_delete_ssid(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	int index = 0;
	json_object *index_object;

	if (json_object_object_get_ex(json, request_key[INDEX], &index_object)) {
		index = json_object_get_int(index_object);
	} else {
		response = create_error_response(DELETE_SSID, INTERNAL_ERROR, tid);
		goto send_data;
	}

	const gchar *ssid = (gchar *) g_slist_nth_data(config->ssid_list, index);
	if (!ssid) {
		response = create_error_response(DELETE_SSID, BAD_REQUEST, tid);
		goto send_data;
	}

	gboolean status = delete_configured_ssid(ssid);
	if (status) {
		rebuild_configured_wifi_list();
		response = create_success_response(DELETE_SSID, CONF_SUCCESS, tid, NULL);
	} else {
		response = create_error_response(DELETE_SSID, INTERNAL_ERROR, tid);
	}

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

void handle_get_configured_ssid(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	int index = 0;
	json_object *index_object;

	if (config->ssid_list == NULL) {
		btclient_error("No ssid list found. Incorrect message sequence");
		response = create_error_response(GET_SSID, BAD_REQUEST, tid);
		goto send_data;
	}

	if (json_object_object_get_ex(json, request_key[INDEX], &index_object)) {
		index = json_object_get_int(index_object);
	} else {
		response = create_error_response(GET_SSID, BAD_REQUEST, tid);
		goto send_data;
	}

	if (index < 0 || index >= config->ssid_list_length) {
		btclient_error("configured ssid list out of bound request");
		response = create_error_response(GET_SSID, BAD_REQUEST, tid);
		goto send_data;
	}

	gchar *ssid = (gchar *)g_slist_nth_data(config->ssid_list, index);

	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[WIFI_SSID], json_object_new_string(ssid));
	response = create_success_response(GET_SSID, CONF_SUCCESS, tid, data);

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

static void free_scan_list(GSList *list)
{
	int i;

	if (list == NULL) {
		return;
	}

	guint len = g_slist_length(list);

	for (i = 0; i < len; i++) {
		ap_item_t *ap_item = (ap_item_t *)g_slist_nth_data(config->scan_list, i);
		g_free(ap_item->security);
		g_free(ap_item->ssid);
		g_free(ap_item);
	}

	g_slist_free(list);
	list = NULL;
}

/* get_scan_list_count performs GetServices on connman and
 * returns the result.
 */
void handle_get_scan_list_count(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;

	json_object *data = json_object_new_object();

	if (config->wifi_scan_inprogress) {
		json_object_object_add(data, request_key[SSID_COUNT], json_object_new_int(-1));
		response = create_success_response(GET_AP_LIST_COUNT, CONF_SUCCESS, tid, data);
		goto send_resp;
	}

	free_scan_list(config->scan_list);
	gboolean status = get_wifi_scan_list(config);
	if (status) {
		json_object_object_add(data, request_key[SSID_COUNT], json_object_new_int(config->scan_list_length));

		response = create_success_response(GET_AP_LIST_COUNT, CONF_SUCCESS, tid, data);
	} else {
		response = create_error_response(GET_AP_LIST_COUNT, INTERNAL_ERROR, tid);
	}

send_resp:
	send_response(response, is_cleartext);
	g_free(response);
}

gboolean wifi_scan_timeout(gpointer user_data)
{
	config->wifi_scan_inprogress = FALSE;
	state.scan_timeout_id = 0;
	return FALSE;
}

void handle_start_scan(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;

	if (state.scan_timeout_id) {
		g_source_remove(state.scan_timeout_id);
		state.scan_timeout_id = 0;
	}

	if (config->wifi_scan_inprogress) {
		btclient_info("wifi scan is already in progress");
		response = create_success_response(START_SCAN, CONF_SUCCESS, tid, NULL);
		goto end;
	}

	if (!state.wpa_supplicant_running) {
		btclient_info("wpa_supplicant not running\n");
		response = create_error_response(START_SCAN, INTERNAL_ERROR, tid);
		goto end;
	}

	gboolean status = start_wifi_scan_async();
	if (status) {
		state.scan_timeout_id = g_timeout_add_seconds(WIFI_SCAN_TIMEOUT, wifi_scan_timeout, NULL);
		config->wifi_scan_inprogress = TRUE;
		response = create_success_response(START_SCAN, CONF_SUCCESS, tid, NULL);
	} else {
		response = create_error_response(START_SCAN, INTERNAL_ERROR, tid);
	}

end:
	send_response(response, is_cleartext);
	g_free(response);
}

void handle_get_scan_list_item(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	int index = 0;
	json_object *index_object;

	if (json_object_object_get_ex(json, request_key[INDEX], &index_object)) {
		index = json_object_get_int(index_object);
	} else {
		response = create_error_response(GET_AP_ITEM, BAD_REQUEST, tid);
		goto send_data;
	}

	if (index < 0 || index >= config->scan_list_length) {
		response = create_error_response(GET_AP_ITEM, BAD_REQUEST, tid);
		goto send_data;
	}

	ap_item_t *ap_item = (ap_item_t *)g_slist_nth_data(config->scan_list, index);
	if (!ap_item || !ap_item->ssid || *(ap_item->ssid) == '\0') {
		response = create_error_response(GET_AP_ITEM, INTERNAL_ERROR, tid);
		goto send_data;
	}

	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[WIFI_SSID], json_object_new_string(ap_item->ssid));
	json_object_object_add(data, request_key[WIFI_SECURITY], json_object_new_string(ap_item->security));
	json_object_object_add(data, request_key[RSSI], json_object_new_int(ap_item->rssi));
	response = create_success_response(GET_AP_ITEM, CONF_SUCCESS, tid, data);

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

void handle_start_wifi_test(int tid, json_object *json, gboolean is_cleartext)
{
	gchar* response;
	json_object *index_object;

	if (!json_object_object_get_ex(json, request_key[INDEX], &index_object)) {
		response = create_error_response(START_WIFI_TEST, BAD_REQUEST, tid);
		goto send_data;
	}

	/* json_object_get_int sets errno */
	int index = json_object_get_int(index_object);

	if (config->ssid_list == NULL || config->ssid_list_length <= index) {
		btclient_error("Incorrect message sequence");
		response = create_error_response(START_WIFI_TEST, BAD_REQUEST, tid);
		goto send_data;
	}

	gchar *ssid = (gchar *)g_slist_nth_data(config->ssid_list, index);

	script_runner(NULL, "rm -f " WIFI_TEST_FILE);
	gboolean status = start_connection_test(ssid);
	if (status) {
		response = create_success_response(START_WIFI_TEST, CONF_SUCCESS, tid, NULL);
	} else {
		response = create_error_response(START_WIFI_TEST, INTERNAL_ERROR, tid);
	}

send_data:
	send_response(response, is_cleartext);
	g_free(response);
}

/*
 * TODO: this method logic need to be moved to Lua scripts.
 */
void handle_get_wifi_test_result(int tid, json_object *json, gboolean is_cleartext)
{
	GString *pad = g_string_new(NULL);
	int result = -1;
	gchar *response;
	int rc = script_runner(&pad, "cat "WIFI_TEST_FILE);
	if (rc == 0) {
		if (g_strcmp0(pad->str, "Busy") == 0) {
			result = -1;
		} else if(g_strcmp0(pad->str, "Connected") == 0) {
			result = 1;
		} else if(g_strcmp0(pad->str, "Failed") == 0) {
			result = 0;
		}
		g_string_free(pad, TRUE);
	}

	json_object *data = json_object_new_object();
	json_object_object_add(data, request_key[RESULT], json_object_new_int(result));
	response = create_success_response(GET_WIFI_TEST_RESULT, CONF_SUCCESS, tid, data);
	send_response(response, is_cleartext);
	g_free(response);

}

void handle_get_rssi(int tid, json_object *json, gboolean is_cleartext)
{
        gchar *response;

        json_object *ssid_object;

        json_bool result = json_object_object_get_ex(json, request_key[WIFI_SSID], &ssid_object);
        if (!result) {
                response = create_error_response(GET_RSSI, BAD_REQUEST, tid);
                goto send_data;
        }

        const gchar *ssid = json_object_get_string(ssid_object);

        gboolean status = get_signal_strength(config, ssid);
        if (status) {
                json_object *data = json_object_new_object();
                json_object_object_add(data, request_key[RSSI], json_object_new_int(config->rssi));
                response = create_success_response(GET_RSSI, CONF_SUCCESS, tid, data);
        } else {
                response = create_error_response(GET_RSSI, INTERNAL_ERROR, tid);
        }

send_data:
        send_response(response, is_cleartext);
        g_free(response);
}

static time_t now()
{
   // timespec t;
   // clock_gettime(CLOCK_MONOTONIC, &t);
  //  return (double)t.tv_sec + t.tv_nsec/1.E+09;
    time_t t; time(&t);
    return t;
}

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

static gboolean waitForStatusFile(int notifyDesc, const char *pszFileName)
{
    //sleep(1); // TODO: wait until real file created/updated, 1 sec is too long, just as POC

    char events[BUF_LEN];
    if (notifyDesc < 0)
        return FALSE;

    struct pollfd fds;
    memset(&fds, 0, sizeof(fds));
    fds.fd = notifyDesc;
    fds.events = POLLIN;

    gboolean bSuccess = FALSE;

    int ec = poll(&fds, 1, 1000); // 0 means "timeout expired" -> do nothing
    if (ec > 0)
    {
        if (fds.revents == POLLIN)
        {
            long length = read(notifyDesc, events, BUF_LEN);
            if (length < 0)
            {
                fprintf(stderr, "Error reading inotify file descriptor (%s)\n", strerror(errno));
                return FALSE;
            }

            long i = 0;
            while (!bSuccess && (i < length))
            {
                struct inotify_event *pEvent = (struct inotify_event *)(events+i);
                if (pEvent->len)
                {
                    if (pEvent->mask & IN_CLOSE_WRITE)
                    {
                        // pEvent->name is newly created/closed file name
                        // check it, it can be just stranger
                        if (strcasecmp(pEvent->name, pszFileName) == 0)
                        {
                            bSuccess = TRUE;
                        }
                    }
                }
                i += EVENT_SIZE + pEvent->len;
            }
            fds.revents = 0;
        }
    }

    return bSuccess;
}

void handle_get_lte_status(int tid, json_object *json, gboolean is_cleartext)
{
    const char *PSZ_CURRENT_STATE_DIR = "/var/state/lte-manager/";
    const char *PSZ_CURRENT_STATE_FILE = "lteparam";

    static gboolean bInitialized = FALSE;
    static GHashTable *pLteValues = NULL;
    static char *pszLteStateFullPath = NULL;
    static int notifyDesc = -1;
    if (!bInitialized)
    {
        // init everything

        // 1. init inotify stuff
        notifyDesc = inotify_init();
        if (notifyDesc < 0)
        {
            fprintf(stderr, "Error initializing inotify (%s)", strerror(errno));
        }

        int watchDesc = inotify_add_watch(notifyDesc, PSZ_CURRENT_STATE_DIR, IN_CLOSE_WRITE);
        if (watchDesc < 0)
        {
            fprintf(stderr, "Cannot monitor %s (%s)", PSZ_CURRENT_STATE_DIR, strerror(errno));
            close(notifyDesc);
            notifyDesc = -1;
        }

        // 2. compose full file path
        pszLteStateFullPath = (char *)calloc(sizeof(char), strlen(PSZ_CURRENT_STATE_DIR)+strlen(PSZ_CURRENT_STATE_FILE)+2);
        strcpy(pszLteStateFullPath, PSZ_CURRENT_STATE_DIR);
        strcat(pszLteStateFullPath, PSZ_CURRENT_STATE_FILE);

        // 3. create hash-table "param name" vs "value"
        pLteValues = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
        bInitialized = TRUE;
    }

    gboolean bSignalNeeded = TRUE;
    struct stat fileInfo;
    int ec = stat(pszLteStateFullPath, &fileInfo);
    if (ec == 0)
    {
        if (difftime(now(), fileInfo.st_mtime) < 10)
            bSignalNeeded = FALSE; // can be skipped, not too old
    }

    if (bSignalNeeded)
    {
        //ec = system("kill -SIGUSR1 `pidof lte-manager`"); // TODO: GET RID OF system() call
        org_ooma_lteservice_emit_get_lte_param(lteservice_interface, "RSRP");
        gboolean bSuccess = waitForStatusFile(notifyDesc, PSZ_CURRENT_STATE_FILE);
        if (bSuccess)
        {
        }
        else
            fprintf(stderr, "Cannot wait for file %s\n", PSZ_CURRENT_STATE_FILE);
    }

    FILE *pFile = fopen(pszLteStateFullPath, "r");
    if (pFile)
    {
        char pLteValue[128] = {0};
        fgets(pLteValue, sizeof(pLteValue), pFile);
        char *pszOldKey = NULL;
        char *pszOldValue = NULL;
        if (g_hash_table_lookup_extended(pLteValues, "RSRP", (gpointer *)&pszOldKey, (gpointer *)&pszOldValue))
        {
            if (strcmp(pLteValue, pszOldValue) != 0)
            {
                //printf("Replace: (%s) = (%s)\n", pszKey, pszValue);
                g_hash_table_replace(pLteValues, g_strdup("RSRP"), g_strdup(pLteValue));
            }
        }
        else
        {
            //printf("Insert: (%s) = (%s)\n", pszKey, pszValue);
            g_hash_table_insert(pLteValues, g_strdup("RSRP"), g_strdup(pLteValue));
        }
        fclose(pFile);
        //printf("Map updated, %d values\n", g_hash_table_size(pLteValues));
    }

    gchar *pszResponse = NULL;
    json_object *pParamObject = NULL;
    if (!json_object_object_get_ex(json, "prm", &pParamObject))
    {
        pszResponse = create_error_response(GET_LTE_STATUS, BAD_REQUEST, tid);
    }
    else
    {
        json_object *pReplyData = json_object_new_object();

        int nParams = json_object_array_length(pParamObject);
        //printf("%d params requested\n", nParams);
        for (int i = 0; i < nParams; i++)
        {
            json_object *pParam = json_object_array_get_idx(pParamObject, i);
            const gchar *pParamName = json_object_get_string(pParam);
            gchar *pValue = (gchar *)g_hash_table_lookup(pLteValues, pParamName);
            //printf("Key = (%s), Value = (%s)\n", pParamName, pValue);
            if (pValue != NULL)
                json_object_object_add(pReplyData, pParamName, json_object_new_string(pValue));
        }

        pszResponse = create_success_response(GET_LTE_STATUS, CONF_SUCCESS, tid, pReplyData);
    }

    printf("Response:%d bytes\n%s\n", strlen(pszResponse), pszResponse);
    send_response(pszResponse, is_cleartext);

    g_free(pszResponse);
}

void handle_get_summary(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response = NULL;
	json_object *data = json_object_new_object();
	FILE *fp = NULL;
	char *line = NULL;
	size_t len = SUMMARY_FILE_LINE_BUF_SIZE;
	fp = g_fopen(SUMMARY_FILE, "r");
	if(fp == NULL){
		response = create_error_response(GET_SUMMARY, INTERNAL_ERROR, tid);
		goto send_data;
	}
	line = g_try_malloc(sizeof(char) * SUMMARY_FILE_LINE_BUF_SIZE);
	if(line == NULL){
		response = create_error_response(GET_SUMMARY, INTERNAL_ERROR, tid);
		goto send_data;
	}
	while(!feof(fp)){
		memset(line, 0, SUMMARY_FILE_LINE_BUF_SIZE * sizeof(char));
		size_t rc = getline(&line, &len, fp);
		if(rc == -1){
			break;
		}
		char * token_key = NULL;
		char *token_val = NULL;
		token_key = strtok(line, ":");
		token_val = strtok(NULL, "\n");
		if(token_key == NULL || token_val == NULL){
			response = create_error_response(GET_SUMMARY, INTERNAL_ERROR, tid);
			goto send_data;
		}else{
			json_object_object_add(data, token_key, json_object_new_int(atoi(token_val)));
		}
	}
	response = create_success_response(GET_SUMMARY, CONF_SUCCESS, tid, data);
send_data:	
	send_response(response, is_cleartext);
	g_free(response);
	if(fp)
		fclose(fp);
	g_free(line);
}

void handle_get_network_led_status(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	json_object *data = json_object_new_object();
	uint32_t value = util_get_network_led();
	json_object_object_add(data, "red", json_object_new_int((value & NETWORK_LED_RED)?1:0));
	json_object_object_add(data, "green", json_object_new_int((value & NETWORK_LED_GREEN)?1:0));	
	json_object_object_add(data, "blue", json_object_new_int((value & NETWORK_LED_BLUE)?1:0));	
	response = create_success_response(GET_NETWORK_LED_STATUS, CONF_SUCCESS, tid, data);
	send_response(response, is_cleartext);
	g_free(response);
}

void handle_unsupported_request(int tid, json_object *json, gboolean is_cleartext)
{
	gchar *response;
	response = create_error_response(UNSUPPORTED_REQUEST, BAD_REQUEST, tid);
	send_response(response, is_cleartext);
	g_free(response);
}

void release_ssid_scan_list(void)
{
	free_scan_list(config->scan_list);

	if (config->ssid_list != NULL)
		g_slist_free_full(config->ssid_list, (GDestroyNotify) g_free);

	config->scan_list = NULL;
	config->ssid_list_length = 0;
	config->ssid_list = NULL;
	config->scan_list_length = 0;
}

/* req names as strings are just for reference, need to replace them with enum's. */
request_actions_t request_actions[] = {
                { "error",				handle_unknown },
		{ "get_interface_version",		handle_get_interface_version },
                { "get_myxid",				handle_get_myxid },
                { "get_board_name",			handle_get_board_name },
		{ "get_build_number",			handle_get_build_number },
                { "get_status",				handle_get_status },
                { "get_ssid_count",			handle_get_ssid_count },
                { "insert_ssid",			handle_insert_ssid },
                { "delete_ssid",			handle_delete_ssid },
                { "get_ssid",				handle_get_configured_ssid },
		{ "get_ap_list_count",			handle_get_scan_list_count },
                { "start_scan",				handle_start_scan },
                { "get_ap_item",			handle_get_scan_list_item },
		{ "start_wifi_test",			handle_start_wifi_test },
		{ "get_wifi_test_result",		handle_get_wifi_test_result },
                { "get_rssi",				handle_get_rssi },
                { "get_lte_status",			handle_get_lte_status },
		{"get_summary", handle_get_summary},
		{"get_network_led_status", handle_get_network_led_status},
		{ "get_file_name_list",			handle_get_file_name_list },
		{ "select_file",				handle_select_file},
		{ "get_line", 					handle_get_line },
		{ "delete_file",				handle_delete_file },
		{ "put_file",					handle_put_file },
                { "",					handle_unsupported_request }
};

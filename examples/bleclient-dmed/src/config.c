/*
 * config.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

bleclient_cfg_t bleclient_cfg;

#define FEATURE_DIR					"/opt/etc/features"
#define MSG_TRACING_FILE			"wireless_wifi_config_tracing"
#define CLEARTXT_FILE				"ble-cleartext"

#define CONF_FILE					"/etc/bleclient.conf"
#define KEYFILE_GROUP_NAME			"General"

#define CONF_ENABLE_ENCRYPTION		"EnableCleartextService"
#define CONF_LOCKOUT_TIMEOUT		"ResponseLockOutTimeOut"

static void default_config(void)
{
	bleclient_cfg_t *cfg = &bleclient_cfg;

	cfg->cleartext_enabled = FALSE;
	cfg->lockout_timeout = 60;

}

void load_config(void)
{
	GKeyFile *key_file = NULL;
	GError *error = NULL;
	bleclient_cfg_t *cfg = &bleclient_cfg;

	key_file = g_key_file_new();

	if (!g_key_file_load_from_file (key_file, CONF_FILE, G_KEY_FILE_NONE, &error)) {
		btclient_error("%s file not found: %s\n", CONF_FILE, error->message);
		/* no configuration file found
		 * set default configuration values
		 */
		g_error_free(error);
		default_config();
		return;
	}

	gboolean cleartext_enabled = g_key_file_get_boolean(key_file, KEYFILE_GROUP_NAME, CONF_ENABLE_ENCRYPTION, &error);
	if (error != NULL) {
		btclient_error("%s key not found in %s\n", CONF_ENABLE_ENCRYPTION, CONF_FILE);
		g_clear_error(&error);
		cfg->cleartext_enabled = FALSE;
	} else {
		cfg->cleartext_enabled = cleartext_enabled;
	}

	gint lockout_timeout = g_key_file_get_integer(key_file, KEYFILE_GROUP_NAME, CONF_LOCKOUT_TIMEOUT, &error);
	if (error != NULL) {
		btclient_error("%s key not found in %s\n", CONF_LOCKOUT_TIMEOUT, CONF_FILE);
		cfg->lockout_timeout = 60;
		g_error_free(error);
	} else {
		btclient_info("setting response lockout timeout to %d", lockout_timeout);
		cfg->lockout_timeout = lockout_timeout;
	}

	g_key_file_unref(key_file);
}

void apply_configuration_settings(void)
{
	if (bleclient_cfg.cleartext_enabled) {
		btclient_info("Cleartext service is enabled");
		FSM_SET_BIT(CLEARTXT_ENABLED);
	}
}

inline static void set_feature_file_settings(void)
{

	if (g_file_test(FEATURE_DIR "/" CLEARTXT_FILE, G_FILE_TEST_EXISTS)) {
		btclient_info("Found cleartext feature file. cleartext service will be enabled");
		FSM_SET_BIT(CLEARTXT_ENABLED);
	}

	if (g_file_test(FEATURE_DIR "/" MSG_TRACING_FILE, G_FILE_TEST_EXISTS)) {
		btclient_info("Message tracing enabled");
		FSM_SET_BIT(MSG_TRACING_ENABLED);
	} else {
		FSM_CLEAR_BIT(MSG_TRACING_ENABLED);
	}
}

void feature_file_inotify_callback(struct inotify_event *event, const char *ident)
{
	if (g_strcmp0(ident, CLEARTXT_FILE) == 0) {
		if (g_file_test(FEATURE_DIR "/" CLEARTXT_FILE, G_FILE_TEST_EXISTS) && !FSM_TEST_BIT(CLEARTXT_ENABLED)) {
			FSM_SET_BIT(CLEARTXT_ENABLED);
			change_encryption_setting();
		} else if (!g_file_test(FEATURE_DIR "/" CLEARTXT_FILE, G_FILE_TEST_EXISTS) && FSM_TEST_BIT(CLEARTXT_ENABLED)) {
			btclient_info("cleartext feature file removed");
			FSM_CLEAR_BIT(CLEARTXT_ENABLED);
			change_encryption_setting();
		}
	} else if (g_strcmp0(ident, MSG_TRACING_FILE) == 0) {
		if (g_file_test(FEATURE_DIR "/" MSG_TRACING_FILE, G_FILE_TEST_EXISTS)) {
			FSM_SET_BIT(MSG_TRACING_ENABLED);
		} else {
			FSM_CLEAR_BIT(MSG_TRACING_ENABLED);
		}
	}

}

void feature_config_init(void)
{
	set_feature_file_settings();
	bleclient_inotify_init();
	bleclient_inotify_register(FEATURE_DIR, feature_file_inotify_callback);
}

void config_cleanup(void)
{
	bleclient_inotify_register(FEATURE_DIR, feature_file_inotify_callback);
	bleclient_inotify_cleanup();
}

/*
 * luabind.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 * 
 * Note: not used in product -- only for testing on other platforms 
 * Cribbed from $TOPDIR/ooma/luabind/
 */


#ifdef NTELO
#include "private.h"
#endif

#include "luabind.h"

#define luabind_debug(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define luabind_error(...) syslog(LOG_ERR, __VA_ARGS__)
#define luabind_info(...) syslog(LOG_INFO, __VA_ARGS__)

#define LIBDIR "/usr/share/lua"
#define WIRELESS_PAGE "wireless.lua"

/* Lua method names */
static char *lua_methods[] = {
		"get_board_name",
		"is_device_inservice",
		"get_service_type",
		"ws_configured_ssid_list",
		"ws_get_wifi_service_list",
		"ws_configure_wifi_network",
		"ws_delete_wifi_network",
		"ws_scan_wifi",
		"ws_connect_to_service",
		"ws_get_rssi_of_service"
};

/* data that is retrieved from Lua scripts
 * is stored in a structure. This library
 * users need to call this method to get an instance
 * of the structure.
 */
dev_conf_t *device_config_create(void)
{

	dev_conf_t *config = (dev_conf_t *) malloc(sizeof(dev_conf_t));
	if (config == NULL) {
		luabind_error("%s malloc failed", __func__);
	}

	/* we may need to free scan and configured ssid list
	 * in multiple places. For safe free attempts, need
	 * to initialize with NULL after freeing.
	 */
	config->ssid_list = NULL;
	config->ssid_list_length = 0;
	config->scan_list = NULL;
	config->scan_list_length = 0;
	config->wifi_scan_inprogress = FALSE;

	return config;
}

void device_config_release(dev_conf_t *config)
{
	/* dev_conf_t elements may contain dynamic data.
	 * caller should free them before calling this method.
	 */
	if (config != NULL) {
		free(config);
	}
}

static lua_State* create_lua_state(void)
{
	lua_State *L;
	L = luaL_newstate();
	if (L == NULL) {
		luabind_debug("luaL_newstate failed\n");
		return NULL;
	}

	luaL_openlibs(L);

	 int ret = luaL_dofile(L, LIBDIR "/" WIRELESS_PAGE);
	 if (ret != LUA_OK) {
		 luabind_error("failed loading %s, error code %d", WIRELESS_PAGE, ret);
		 goto cleanup;
	 }

	return L;

cleanup:
	lua_close(L);
	return NULL;
}

/*
 * get board name of the device.
 */
gboolean get_board_name(dev_conf_t *config)
{
	lua_State *L;
	int return_value;
	gboolean rc = FALSE;

	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[BOARD_NAME]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	return_value = lua_pcall(L, 0, 1, 0);
	if (return_value) {
		luabind_error("%s : failed calling Lua. Return value :%d\n" ,
				__func__, return_value);
		goto cleanup;
	}

	const char *board_name = lua_tolstring(L, -1, NULL);
	if (board_name != NULL) {
		config->board_name = g_strdup(board_name);
	} else {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[BOARD_NAME]);
		config->board_name = g_strdup("unknown");
	}

	rc = TRUE;

cleanup:
	lua_close(L);
	return rc;
}

gboolean get_device_status(dev_conf_t *config)
{

	lua_State *L;
	int return_value;
	gboolean rc = FALSE;
	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[IS_INSERVICE]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	return_value = lua_pcall(L, 0, 1, 0);
	if (return_value) {
		luabind_error("%s : failed calling Lua, return value :%d\n", __func__,
				return_value);
		goto cleanup;
	}

	if (!lua_isboolean(L, -1)) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[IS_INSERVICE]);
		goto cleanup;
	}

	config->inservice = lua_toboolean(L, -1);

	/* remove all stack elements */
	lua_settop(L, 0);
	lua_getglobal(L, lua_methods[GET_SERVICE_TYPE]);
	if (lua_type(L, -1) != LUA_TFUNCTION) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[GET_SERVICE_TYPE]);
		goto cleanup;
	}

	return_value = lua_pcall(L, 0, 1, 0);
	if (return_value) {
		luabind_error("%s : failed calling Lua, return value :%d\n", __func__,
				return_value);
		goto cleanup;
	}

	const char *service_type = lua_tolstring(L, -1, NULL);
	if (service_type != NULL) {
		config->service_type = g_strdup(service_type);
	} else {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[GET_SERVICE_TYPE]);
		config->service_type = g_strdup("none");
	}

	rc = TRUE;

cleanup:
	lua_close(L);
	return rc;
}

static GSList* lua_table_to_list(lua_State *L)
{
	GSList *list = NULL;

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_isstring(L, -1) == 1) {
			/* we only care about values in the table
			 * as we are expecting an array.
			 */
			gchar *str = g_strdup(lua_tostring(L, -1));
			list = g_slist_prepend(list, str);
		}
		lua_pop(L, 1);
	}

	list = g_slist_reverse(list);
	return list;
}

static GSList* wifi_scan_table_to_list(lua_State *L)
{
	GSList *list = NULL;
	ap_item_t *ap_item = NULL;

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_istable(L, -1) == 1) {
			ap_item = malloc(sizeof(ap_item_t));
			if (ap_item == NULL) {
				luabind_error("malloc failed %s", __func__);
				return NULL;
			}
			memset(ap_item, 0, sizeof(*ap_item));
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				if (lua_isstring(L, -2) == 1) {
					if (g_strcmp0(lua_tostring(L, -2), "Name") == 0) {
						ap_item->ssid = g_strdup(lua_tostring(L, -1));
					} else if (g_strcmp0(lua_tostring(L, -2), "Security") == 0) {
						ap_item->security = g_strdup(lua_tostring(L, -1));
					} else if(g_strcmp0(lua_tostring(L, -2), "Strength") == 0) {
						ap_item->rssi = lua_tointeger(L, -1);
					}
				}
				lua_pop(L, 1);
			}
			list = g_slist_prepend(list, ap_item);
		}
		lua_pop(L, 1);
	}

	list = g_slist_reverse(list);
	return list;
}

gboolean get_wifi_scan_list(dev_conf_t *config)
{
	lua_State *L;
	gboolean rc = FALSE;

	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[SCAN_LIST]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[SCAN_LIST]);
		goto cleanup;
	}

	int return_value = lua_pcall(L, 0, 1, 0);
	if (return_value) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				return_value);
		goto cleanup;
	}

	if (!lua_istable(L, -1)) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[SCAN_LIST]);
		goto cleanup;
	}

	config->scan_list = wifi_scan_table_to_list(L);
	config->scan_list_length = g_slist_length(config->scan_list);

	rc = TRUE;

cleanup:
	lua_close(L);
	return rc;
}

gboolean get_configured_wifi_list(dev_conf_t *config)
{

	lua_State *L;
	gboolean rc = FALSE;

	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[CONFIGURED_SSID_LIST]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[CONFIGURED_SSID_LIST]);
		goto cleanup;
	}
	int return_value = lua_pcall(L, 0, 1, 0);
	if (return_value) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				return_value);
		goto cleanup;
	}

	if (!lua_istable(L, -1)) {
		luabind_error("%s: incompatible Lua api: %s", __func__,
				lua_methods[CONFIGURED_SSID_LIST]);
		goto cleanup;
	}

	config->ssid_list = lua_table_to_list(L);
	config->ssid_list_length = g_slist_length(config->ssid_list);
	rc = TRUE;

cleanup:
	lua_close(L);
	return rc;
}

/* use lua_tointegerx - to get integer from stack
 * use lua_toboolean - to get boolean from stack
 */
gboolean configure_wifi(const gchar *ssid, const gchar *passphrase, int index, int hidden)
{
	int ret;
	lua_State *L;
	gboolean rc = FALSE;

	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[CONFIGURE_WIFI_NETWORK]);

	/* check whether expected methods are defined in Lua or not */
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	/* no need to support embedded NULL in the strings. then,
	 * why lua_pushlstring ?
	 */
	lua_pushlstring(L, ssid, strlen(ssid));
	if (passphrase != NULL) {
		lua_pushlstring(L, passphrase, strlen(passphrase));
	} else {
		lua_pushnil(L);
	}
	/* push nil's on to the stack for compatibility with
	 * Lua API.
	 */
	lua_pushinteger(L, index);
	lua_pushboolean(L, hidden);

	ret = lua_pcall(L, 4, 1, 0);
	if (ret) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				ret);
		goto cleanup;
	}
	/* lua_pcall consumes all the arguments we pushed on to the stack */
	rc = lua_toboolean(L, -1);

	cleanup: lua_close(L);
	return rc;
}

gboolean delete_configured_ssid(const gchar *ssid)
{
	int ret;
	lua_State *L;
	gboolean rc = FALSE;

	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[DELETE_CONFIGURED_SSID]);
	/* check whether expected methods are defined in Lua or not */
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	lua_pushlstring(L, ssid, strlen(ssid));
	ret = lua_pcall(L, 1, 1, 0);
	if (ret) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
					ret);
		goto cleanup;
	}

	rc = lua_toboolean(L, -1);

cleanup:
	lua_close(L);
	return rc;
}

/* This method may not return immediately
 * if Lua uses synchronous dbus method to start scan.
 */
gboolean start_wifi_scan_sync(void)
{

	gboolean rc = FALSE;
	lua_State *L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[SCAN_WIFI]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	int ret = lua_pcall(L, 0, 1, 0);
	if (ret != LUA_OK) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				ret);
		return FALSE;
	}

	rc = lua_toboolean(L, -1);

cleanup:
	lua_close(L);
	return rc;

}

gboolean start_wifi_scan_async(void)
{
	GDBusConnection *connection;
	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
	if (connection == NULL) {
		return FALSE;
	}

	g_dbus_connection_call(connection,
			"net.connman",
			"/net/connman/technology/wifi",
			"net.connman.Technology",
			"Scan",
			NULL, NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

	return TRUE;
}

/* this method performs SetService on wanswitch
 * caller need to create signal listener for connection status.
 * SOC has a signal listener and writes a tmp file with status
 */
gboolean start_connection_test(const gchar *ssid)
{

	lua_State *L;
	gboolean rc = FALSE;
	L = create_lua_state();
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[CONNECT_SERVICE]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	lua_pushlstring(L, ssid, strlen(ssid));
	lua_pushnil(L);
	int ret = lua_pcall(L, 2, 1, 0);
	if (ret != LUA_OK) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				ret);
		return FALSE;
	}

	rc = lua_toboolean(L, -1);

cleanup:
	lua_close(L);
	return rc;
}

/* Lua uses dynamic data types, this makes it tough when passing
 * arguments between C and Lua. Checks need to be carefully
 * done before pushing or popping args to or from Lua stack.
 */
gboolean get_signal_strength(dev_conf_t *config, const gchar *ssid)
{

	lua_State *L;
	L = create_lua_state();
	gboolean rc = FALSE;
	if (L == NULL) {
		return FALSE;
	}

	lua_getglobal(L, lua_methods[GET_WIFI_RSSI]);
	if (lua_isfunction(L, -1) != 1) {
		luabind_error("%s undefined Lua function\n", __func__);
		goto cleanup;
	}

	lua_pushlstring(L, ssid, strlen(ssid));

	int ret = lua_pcall(L, 1, 1, 0);
	if (ret != LUA_OK) {
		luabind_error("failed calling Lua in %s error code :%d\n", __func__,
				ret);
		goto cleanup;
	}
	#pragma warning "karlo hacked out lua_tointeger"
/*
	if (lua_isinteger(L, -1)) {
		config->rssi = lua_tointeger(L, -1);
		rc = TRUE;
	} else {
		rc = FALSE;
	}
	rc = TRUE;
*/
cleanup:
	lua_close(L);
	return rc;
}

#ifdef DEVELOPMENT_STAND_ALONE

int main()
{
	gboolean result = configure_wifi("ooma_extreme", "12345678", 1);
	if (result) {
		luabind_debug("wifi configuration successful\n");
	}
	return 0;

}

#endif

#ifndef PRIVATE_H_
#define PRIVATE_H_
/* This file is not used in product.  It is here for convenience.  
 * Cribbed from $TOPDIR/ooma/luabind/private.h to allow native build on x86 and ARM64
 * Recently the shared luabind library changed to C++ application and the headers are not 
 * compatible with Telo code.  It is just easier to ship the libbind shared library use 
 * for other platforms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <glib.h>
#include <gio/gio.h>
#include <errno.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

enum enum_lua_methods {
	BOARD_NAME,
	IS_INSERVICE,
	GET_SERVICE_TYPE,
	CONFIGURED_SSID_LIST,
	SCAN_LIST,
	CONFIGURE_WIFI_NETWORK,
	DELETE_CONFIGURED_SSID,
	SCAN_WIFI,
	CONNECT_SERVICE,
	GET_WIFI_RSSI,
};

typedef enum enum_lua_methods lua_methods_t;

#endif

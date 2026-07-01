/*
 * dbus-error.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

static const GDBusErrorEntry bluez_error_entries[] = {
		{BLUEZ_ERROR_FAILED, "org.bluez.Error.Failed"},
		{BLUEZ_ERROR_INPROGRESS, "org.bluez.Error.InProgress"},
		{BLUEZ_ERROR_NOTAUTHORIZED, "org.bluez.Error.NotAuthorized"},
		{BLUEZ_ERROR_REJECTED, "org.bluez.Error.Rejected"},
		{BLUEZ_ERROR_NOTSUPPORTED, "org.bluez.Error.NotSupported"},
		{BLUEZ_ERROR_NOTPERMITTED, "org.bluez.Error.NotPermitted"},
		{BLUEZ_ERROR_INVALIDVALUELENGTH, "org.bluez.Error.InvalidValueLength"}
};

// Ensure that every error code has an associated D-Bus error name
G_STATIC_ASSERT (G_N_ELEMENTS (bluez_error_entries) == BLUEZ_ERROR_N_ERRORS);

GQuark bluez_error_quark(void) {
	static volatile gsize quark_volatile = 0;
	 g_dbus_error_register_error_domain ("bluez-error-quark",
	                                      &quark_volatile,
	                                      bluez_error_entries,
	                                      G_N_ELEMENTS (bluez_error_entries));
	  return (GQuark) quark_volatile;
}


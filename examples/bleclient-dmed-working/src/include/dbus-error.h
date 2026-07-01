/*
 * dbus-error.h
 *
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef _DBUS_ERROR_H_
#define _DBUS_ERROR_H_

#define BLUEZ_ERROR (bluez_error_quark())

GQuark bluez_error_quark(void);

typedef enum {
	BLUEZ_ERROR_FAILED,
	BLUEZ_ERROR_INPROGRESS,
	BLUEZ_ERROR_NOTAUTHORIZED,
	BLUEZ_ERROR_REJECTED,
	BLUEZ_ERROR_NOTSUPPORTED,
	BLUEZ_ERROR_NOTPERMITTED,
	BLUEZ_ERROR_INVALIDVALUELENGTH,
	BLUEZ_ERROR_N_ERRORS
}BluezError_t;

#endif

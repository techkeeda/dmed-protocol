/*
 * btclient.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef BTCLIENT_H_
#define BTCLIENT_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <gio/gio.h>

#include "log.h"
#include "dbus-error.h"
#include "encrypt.h"
#include "inotify.h"
#include "config.h"
#include "engine.h"
#include "dbus-support.h"
#include "advertisement.h"
#include "service.h"
#include "agent.h"
#include "fsm.h"
#include "parser.h"
#include "configure.h"
#include "luabind.h"
#include "utils.h"
#include "dmed_service.h"

#define APP_VERSION "2.0"
extern volatile sig_atomic_t exit_request_pending;

#endif /* BTCLIENT_H_ */

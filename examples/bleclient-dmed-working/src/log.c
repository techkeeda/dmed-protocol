/*
 * log.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include <stdarg.h>
#include <sys/syslog.h>

void btclient_info(const char* format, ...) {
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_INFO, format, ap);
	va_end(ap);
}

void btclient_debug(const char* format, ...) {
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_DEBUG, format, ap);
	va_end(ap);
}


void btclient_error(const char* format, ...) {
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_ERR, format, ap);
	va_end(ap);
}

void logger_init(const char *program_name, gboolean no_detach, gboolean debug) {

	int option = LOG_PID;

	if (no_detach) {
		option = LOG_PERROR;
	}

	openlog(program_name, option, LOG_DAEMON);

	syslog(LOG_INFO, "%s version %s started", program_name, APP_VERSION);
}

void logger_exit(void) {

	closelog();
}

/*
 * log.h
 *
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#ifndef LOG_H_
#define LOG_H_

void btclient_info(const char* format, ...)
	__attribute__ ((format(printf, 1, 2)));
void btclient_debug(const char* format, ...)
	__attribute__ ((format(printf, 1, 2)));
void btclient_error(const char* format, ...)
	__attribute__ ((format(printf, 1, 2)));

void logger_init(const char *program_name, gboolean no_detach, gboolean debug);
void logger_exit(void);
#endif /* LOG_H_ */

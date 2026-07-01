/*
 * utils.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef UTILS_H_
#define UTILS_H

GIOChannel* create_mgmt_channel(void);
gboolean bt_mgmt_event_handler(GIOChannel *source, GIOCondition condition, gpointer data);
int script_runner(GString ** ppad, const char *format, ...);
guint8 todigit(guchar c);
guchar hex_to_char(int c);
guint8 calculate_check_digit(GString *str);
GString* get_hostname_check_digit_tag(void);
gboolean is_compressed_file(const char*);

#endif /* UTILS_H_ */

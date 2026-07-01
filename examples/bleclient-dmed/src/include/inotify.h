/*
 * inotify.h
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#ifndef INOTIFY_H_
#define INOTIFY_H_

struct inotify_event;

typedef void (*inotify_event_cb) (struct inotify_event * event, const char *ident);

int bleclient_inotify_register(const char *path, inotify_event_cb callback);
void bleclient_inotify_unregister(const char *path, inotify_event_cb callback);

int bleclient_inotify_init(void);
void bleclient_inotify_cleanup(void);

#endif /* INOTIFY_H_ */

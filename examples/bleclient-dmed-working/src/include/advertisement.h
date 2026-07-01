/*
 * advertisement.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef ADVERTISEMENT_H_
#define ADVERTISEMENT_H_

GVariant* get_advertisement_properties(void);
void register_advertisement(void);
void leadvertising_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
void unregister_advertisement(void);

#endif /* ADVERTISEMENT_H_ */

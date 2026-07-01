/*
 * service.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef SERVICE_H_
#define SERVICE_H_

void change_encryption_setting(void);
void set_encryption(gboolean disabled);

GVariant *get_characteristic_properties(gboolean is_cleartext);
GVariant *get_descriptor_properties(gboolean is_cleartext);
GVariant *get_descriptor_properties_2(gboolean is_cleartext);
GVariant *get_service_properties(gboolean is_cleartext);

void gatt_manager_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
void register_service(void);
void unregister_service(void);

#endif /* SERVICE_H_ */

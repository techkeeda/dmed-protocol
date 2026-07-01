/*
 * agent.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef AGENT_H_
#define AGENT_H_

#include "agent-generated.h"

#define ENABLE_BLE_AUTHENTICATION

void hookup_dbus_agent_callbacks(void);
gboolean register_dbus_agent(void);
void unregister_dbus_agent(void);
gboolean request_default_agent(void);

#ifdef ENABLE_BLE_AUTHENTICATION
/* Agent headers */
gboolean on_handle_request_passkey(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation, const gchar *arg_object_path);
gboolean on_handle_request_pin_code(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation, const gchar *arg_object_path);
gboolean on_handle_request_confirmation(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation, const gchar *object_name, guint32 passkey);
gboolean on_handle_request_authorization(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation, const gchar *object_path);
gboolean on_handle_authorize_service(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation, const gchar *object_path, const gchar *uuid);

extern OrgBluezAgent1 *agent_interface;
#endif

#endif /* AGENT_H_ */

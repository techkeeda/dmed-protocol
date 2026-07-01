/*
 * agent.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

#define PASSKEY_MASK 0x7FFFF
#define GET_PASSKEY "hostname | cut -c10-16 | sha256sum | cut -b -5"

static guint64 get_passkey_from_hostname(void)
{
	guint64 pass_key = 954267;
	GString *pad = g_string_new(NULL);
	int rc = script_runner(&pad, GET_PASSKEY);

	if (rc == 0) {
		pass_key = g_ascii_strtoull(pad->str, NULL, 16);
		pass_key = pass_key & PASSKEY_MASK;
		g_string_free(pad, TRUE);
	}

	return pass_key;
}

OrgBluezAgent1 *agent_interface;

/* handler methods always returns true
 * even there is an error while handling
 */
gboolean on_handle_request_passkey(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation,
		const gchar *arg_object_path)
{

	guint pin = (guint) get_passkey_from_hostname();
	org_bluez_agent1_complete_request_passkey(object, invocation, pin);

	return TRUE;
}

gboolean on_handle_request_pin_code(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation,
		const gchar *arg_object_path)
{
	const gchar *pincode = "000000";

	btclient_debug("request from %s\n", arg_object_path);
	org_bluez_agent1_complete_request_pin_code(object, invocation, pincode);

	return TRUE;
}

gboolean on_handle_request_confirmation(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation,
		const gchar *object_name,
		guint32 passkey)
{

	btclient_info("%s\n", __func__);

	if (passkey) {
		btclient_info("Received passkey for authentication %u\n", passkey);
		/* Empty reply to confirm authentication */
		org_bluez_agent1_complete_request_confirmation(object, invocation);
		return TRUE;
	}

	g_dbus_method_invocation_return_error(invocation, BLUEZ_ERROR, BLUEZ_ERROR_REJECTED, "%s", "Rejected");
	return FALSE;
}

gboolean on_handle_request_authorization(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation,
		const gchar *object_path)
{

	btclient_info("%s\n", __func__);

	org_bluez_agent1_complete_request_authorization(object, invocation);
	return TRUE;
}

gboolean on_handle_authorize_service(OrgBluezAgent1 *object, GDBusMethodInvocation *invocation,
		const gchar *object_path,
		const gchar *uuid)
{
	btclient_info("%s: uuid of request is %s\n", __func__, uuid);

	org_bluez_agent1_complete_authorize_service(object, invocation);
	return TRUE;
}

void hookup_dbus_agent_callbacks(void)
{
	/* Agent1 methods */
	g_signal_connect(agent_interface, "handle-request-confirmation", G_CALLBACK(on_handle_request_confirmation), 0);

	g_signal_connect(agent_interface, "handle-request-authorization", G_CALLBACK(on_handle_request_authorization), 0);

	g_signal_connect(agent_interface, "handle-authorize-service", G_CALLBACK(on_handle_authorize_service), 0);

	g_signal_connect(agent_interface, "handle-request-pin-code", G_CALLBACK(on_handle_request_pin_code), 0);

	g_signal_connect(agent_interface, "handle-request-passkey", G_CALLBACK(on_handle_request_passkey), 0);

}

/* register an agent object.
 * bluez invokes methods on the registered agent path
 * when required.
 */
gboolean register_dbus_agent(void)
{
	OrgBluezAgentManager1 *agent_manager;

	agent_manager = org_bluez_agent_manager1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			BLUEZ_OBJECT_PATH,
			NULL, NULL);
	if (!agent_manager) {
		btclient_error("get proxy failed %s", __func__);
		return FALSE;
	}

	gboolean ret = org_bluez_agent_manager1_call_register_agent_sync(agent_manager,
			AGENT_OBJECT_PATH, "NoInputNoOutput",
			NULL, NULL);
	if(!ret) {
		btclient_error("Unable to register dbus agent\n");
		return FALSE;
	}

	g_object_unref(agent_manager);

	return TRUE;
}

gboolean request_default_agent(void)
{
	GError *error = NULL;

	OrgBluezAgentManager1 *agent_manager;

	agent_manager = org_bluez_agent_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_BUS_NAME,
			BLUEZ_OBJECT_PATH,
			NULL, NULL);
	if (!agent_manager) {
		btclient_error("Unable to get AgentManager1 proxy\n");
	}

	org_bluez_agent_manager1_call_request_default_agent_sync(agent_manager,
			AGENT_OBJECT_PATH, NULL, &error);
	if (error) {
		btclient_error("%s: %s\n", __func__, error->message);
		g_error_free(error);
	}

	g_object_unref(agent_manager);

	return TRUE;
}

void unregister_dbus_agent(void)
{
	OrgBluezAgentManager1 *agent_manager;

	agent_manager = org_bluez_agent_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE, BLUEZ_BUS_NAME,
			BLUEZ_OBJECT_PATH,
			NULL, NULL);
	if (!agent_manager) {
		btclient_error("failed get proxy method, %s\n", __func__);
		return;
	}

	org_bluez_agent_manager1_call_unregister_agent_sync(agent_manager,
			AGENT_OBJECT_PATH, NULL, NULL);

	g_object_unref(agent_manager);
}

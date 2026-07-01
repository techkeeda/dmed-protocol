/*
 * fsm.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"

static char *fsm_state_string[] = {
		"fsm_idle",
		"fsm_disabled",
		"fsm_connect_wait",
		"fsm_connected",
		"fsm_teardown",
		"fsm_response_lockout"
};

guint32 fsm_vector;

static fsm_state_t current_state, next_state;

void do_idle(void) {

	FSM_CLEAR_BIT(DEVICE_CONNECTED);
	FSM_CLEAR_BIT(SERVICE_ENABLED);

	FSM_SET_BIT(SERVICE_ENABLED);

	/* other parts of code should not
	 * use state variables. Instead, should only set fsm_vector
	 */
	current_state = next_state = FSM_IDLE;
}

gboolean fsm_timeout(gpointer userdata) {

	/* Linux signal handler sets this variable to true if received a signal */
	if(exit_request_pending) {
		disconnect_device();
		g_main_loop_quit(mainloop);
		return FALSE;
	}

	if(current_state != next_state) {
		btclient_info("state changed %s -> %s\n",
				fsm_state_string[current_state], fsm_state_string[next_state]);
		current_state = next_state;
	}

	finite_state_machine();

	return TRUE;
}

void finite_state_machine(void) {

	switch(current_state) {
	case FSM_IDLE:
		if (FSM_TEST_BIT(SERVICE_ENABLED)) {
			next_state = FSM_CONNECT_WAIT;
		} else {
			next_state = FSM_DISABLED;
		}
		break;
	case FSM_DISABLED:
		if (FSM_TEST_BIT(SERVICE_ENABLED)) {
			next_state = FSM_IDLE;
		}
		break;
	case FSM_CONNECT_WAIT:
		if (FSM_TEST_BIT(DEVICE_CONNECTED)) {
			next_state = FSM_CONNECTED;
		}
		break;
	case FSM_CONNECTED:
		if (FSM_TEST_BIT(RESPONSE_LOCKOUT)) {
			next_state = FSM_LOCKOUT;
		} else if (!FSM_TEST_BIT(DEVICE_CONNECTED)) {
			next_state = FSM_TEARDOWN;
		}
		break;
	case FSM_TEARDOWN:
		do_idle();
		next_state = FSM_IDLE;
		break;
	case FSM_LOCKOUT:
		if (!FSM_TEST_BIT(RESPONSE_LOCKOUT)) {
			next_state = FSM_IDLE;
		}
		break;
	default:
		break;
	}
}

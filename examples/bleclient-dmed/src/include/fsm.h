/*
 * fsm.h
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef FSM_H_
#define FSM_H_

#include <glib.h>

enum fsm_state {
	FSM_IDLE,
	FSM_DISABLED,
	FSM_CONNECT_WAIT,
	FSM_CONNECTED,
	FSM_TEARDOWN,
	FSM_LOCKOUT,
	FSM_INVALID
};

typedef enum fsm_state fsm_state_t;

extern guint32 fsm_vector;

#define SERVICE_DISABLED	(1<<1)
#define SERVICE_ENABLED		(1<<2)
#define DEVICE_CONNECTED	(1<<3)
#define KEY_GENERATED		(1<<4)
#define RESPONSE_LOCKOUT	(1<<5)
#define CLEARTXT_ENABLED	(1<<6)
#define MSG_TRACING_ENABLED	(1<<7)

#define FSM_SET_BIT(x) (fsm_vector |= x)

#define FSM_CLEAR_BIT(x) (fsm_vector &= ~x)

#define FSM_TEST_BIT(x) ((fsm_vector & x) == x)


void do_idle(void);
gboolean fsm_timeout(gpointer userdata);
void finite_state_machine(void);

#endif /* FSM_H_ */

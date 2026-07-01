/*
 * main.c
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include <signal.h>

volatile sig_atomic_t exit_request_pending = FALSE;

gboolean option_debug = FALSE;
gboolean option_detach = FALSE;
gboolean option_version = FALSE;

static GOptionEntry entries[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version, "print version number"},
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, "log debug messages"},
		{ "nodetach", 'n', 0, G_OPTION_ARG_NONE, &option_detach, "run in foreground"},
		{ NULL }
};

void sig_handler(int sig, siginfo_t * siginfo, void *ignore)
{
	exit_request_pending = TRUE;
}

void install_signal_handler(void)
{
	struct sigaction act;
	sigset_t block_mask;

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGTERM);
	sigaddset(&block_mask, SIGINT);
	sigaddset(&block_mask, SIGQUIT);
	act.sa_sigaction = sig_handler;
	act.sa_mask = block_mask;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGTERM, &act, NULL) < 0) {
		perror("Failed to install signal handler\n");
		exit(-1);
	}
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
}

void get_options(int argc, char *argv[])
{

	GError *error = NULL;

	GOptionContext *context = g_option_context_new(NULL);

	g_option_context_add_main_entries(context, entries, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
		exit(1);
	}

	g_option_context_free(context);

	if (option_version) {
                printf("%s\n", APP_VERSION);
		exit(1);
	}

        if(!option_detach) {
		if(daemon(0,0)) {
			printf("daemon() failed\n");
			exit(1);
		}
        }

}

int main(int argc, char *argv[])
{

	get_options(argc, argv);

	install_signal_handler();
	logger_init("bleclient", option_detach, option_debug);
	dbus_init();

	/* name_lost_handler quits mainloop.
	 * mainloop run will return if unable to acquire dbus name.
	 */
	g_main_loop_run(mainloop);

	config_cleanup();
	engine_cleanup();
	dbus_cleanup();

	btclient_error("Exited\n");
	logger_exit();

	return 0;
}

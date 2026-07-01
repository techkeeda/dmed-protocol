/*
 * inotify.c
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include <sys/inotify.h>

#ifdef STANDALONE
GMainLoop *main_loop;
#endif

struct bleclient_inotify {
	GIOChannel *channel;
	uint watch;
	int wd;

	GSList *list;
};

static GHashTable *inotify_hash;

static gboolean inotify_data(GIOChannel * channel, GIOCondition cond, gpointer user_data)
{
	struct bleclient_inotify *inotify = user_data;
	char buffer[256];
	char *next_event;
	gsize bytes_read;
	GIOStatus status;
	GSList *list;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
		inotify->watch = 0;
		return FALSE;
	}

	status = g_io_channel_read_chars(channel, buffer, sizeof(buffer) - 1, &bytes_read, NULL);

	switch (status) {
	case G_IO_STATUS_NORMAL:
		break;
	case G_IO_STATUS_AGAIN:
		return TRUE;
	default:
		btclient_error("Reading from inotify channel failed");
		inotify->watch = 0;
		return FALSE;
	}

	next_event = buffer;

	while (bytes_read > 0) {
		struct inotify_event *event;
		gchar *ident;
		gsize len;

		event = (struct inotify_event *) next_event;
		if (event->len) {
			ident = next_event + sizeof(struct inotify_event);
		} else {
			ident = NULL;
		}

		len = sizeof(struct inotify_event) + event->len;

		/* check if inotify_event block fit */
		if (len > bytes_read) {
			break;
		}

		next_event += len;
		bytes_read -= len;

		for (list = inotify->list; list; list = list->next) {
			inotify_event_cb callback = list->data;

			(*callback) (event, ident);
		}
	}

	return TRUE;
}

static int create_watch(const char *path, struct bleclient_inotify *inotify)
{
	int fd;

	fd = inotify_init();
	if (fd < 0) {
		return -EIO;
	}

	inotify->wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
	if (inotify->wd < 0) {
		btclient_error("Creation of %s watch failed", path);
		close(fd);
		return -EIO;
	}

	inotify->channel = g_io_channel_unix_new(fd);
	if (!inotify->channel) {
		btclient_error("Creation of inotify channel failed");
		inotify_rm_watch(fd, inotify->wd);
		inotify->wd = 0;

		close(fd);
		return -EIO;
	}

	g_io_channel_set_close_on_unref(inotify->channel, TRUE);
	g_io_channel_set_encoding(inotify->channel, NULL, NULL);
	g_io_channel_set_buffered(inotify->channel, FALSE);

	inotify->watch = g_io_add_watch(inotify->channel,
					G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR, inotify_data, inotify);

	return 0;
}

static void remove_watch(struct bleclient_inotify *inotify)
{
	int fd;

	if (!inotify->channel) {
		return;
	}

	if (inotify->watch > 0) {
		g_source_remove(inotify->watch);
	}

	fd = g_io_channel_unix_get_fd(inotify->channel);

	if (inotify->wd >= 0) {
		inotify_rm_watch(fd, inotify->wd);
	}

	g_io_channel_unref(inotify->channel);
}

int bleclient_inotify_register(const char *path, inotify_event_cb callback)
{
	struct bleclient_inotify *inotify;
	int err;

	if (!callback) {
		return -EINVAL;
	}

	inotify = g_hash_table_lookup(inotify_hash, path);
	if (inotify) {
		goto update;
	}

	inotify = g_try_new0(struct bleclient_inotify, 1);
	if (!inotify) {
		return -ENOMEM;
	}

	inotify->wd = -1;

	err = create_watch(path, inotify);
	if (err < 0) {
		g_free(inotify);
		return err;
	}

	g_hash_table_replace(inotify_hash, g_strdup(path), inotify);

update:
	inotify->list = g_slist_prepend(inotify->list, callback);

	return 0;
}

static void cleanup_inotify(gpointer user_data)
{
	struct bleclient_inotify *inotify = user_data;

	g_slist_free(inotify->list);

	remove_watch(inotify);
	g_free(inotify);
}

void bleclient_inotify_unregister(const char *path, inotify_event_cb callback)
{
	struct bleclient_inotify *inotify;

	inotify = g_hash_table_lookup(inotify_hash, path);
	if (!inotify) {
		return;
	}

	inotify->list = g_slist_remove(inotify->list, callback);
	if (inotify->list) {
		return;
	}

	g_hash_table_remove(inotify_hash, path);
}

int bleclient_inotify_init(void)
{
	inotify_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, cleanup_inotify);
	return 0;
}

void bleclient_inotify_cleanup(void)
{
	g_hash_table_destroy(inotify_hash);
}

#ifdef STANDALONE

int main(void)
{
	main_loop = g_main_loop_new(NULL, FALSE);

	bleclient_inotify_init();
	bleclient_inotify_register(FEATURE_DIR, feature_file_inotify_callback);

	g_main_loop_run(main_loop);
}

#endif

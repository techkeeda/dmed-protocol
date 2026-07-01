/*
 * utils.c
 *
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#include "btclient.h"
#include <wait.h>
#include <glib/gprintf.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

/* TODO karlo: where were these copied from? */
/* Looks similar to bluez.../src/shared/mgmt.c */

#define DEVICE_CONNECTED_EVENT			0x000B
#define DEVICE_DISCONNECTED_EVENT		0x000C

#define MGMT_HDR_SIZE					6

struct mgmt_event {
	uint16_t opcode;
	uint16_t index;
	uint16_t len;
} __attribute__((packed));

struct mgmt_addr_info {
	bdaddr_t bdaddr;
	uint8_t type;
} __attribute__((packed));

struct mgmt_ev_device_connected {
	struct mgmt_addr_info addr;
	uint32_t flags;
	uint16_t eir_len;
	uint8_t eir[0];
} __attribute__((packed));

struct mgmt_ev_device_disconnected {
	struct mgmt_addr_info addr;
	uint8_t reason;
} __attribute__((packed));

GIOChannel* create_mgmt_channel(void)
{
	int fd;
	GIOChannel *io_channel;
	struct sockaddr_hci addr;

	fd = socket(PF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, BTPROTO_HCI);
	if (fd < 0) {
		btclient_error("unable to open PF_BLUETOOTH socket");
		return NULL;
	}

	memset(&addr, 0, sizeof(addr));
	addr.hci_family = AF_BLUETOOTH;
	addr.hci_dev = HCI_DEV_NONE;
	addr.hci_channel = HCI_CHANNEL_CONTROL;

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		btclient_error("socket bind failed");
		close(fd);
		return NULL;
	}

	io_channel = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(io_channel, NULL, NULL);
	g_io_channel_set_buffered(io_channel, FALSE); /* TODO karlo: unbuffered? */
	g_io_channel_set_close_on_unref(io_channel, TRUE);

	return io_channel;
}

static void handle_device_disconnected_event(struct mgmt_event *event, const void *param)
{
	btclient_info("karlo.2 handle_device_disconnected_event");

	const struct mgmt_ev_device_disconnected *ev1 = param;

	if (event->len < sizeof(*ev1)) {
		btclient_error("Unexpected device disconnected event length");
		return;
	}

	if (ev1->addr.type != 0x00 && state.remote_object_path == NULL) {
		btclient_info("LE connection disconnected");
		FSM_CLEAR_BIT(DEVICE_CONNECTED);
	}

}

static void handle_device_connected_event(struct mgmt_event *event, const void *param)
{
	const struct mgmt_ev_device_connected *ev = param;
	if (event->len < sizeof(*ev)) {
		btclient_error("Unexpected device connected event length");
		return;
	}

	/* the BDDR address is not always public.
	 * so, we just check whether the connection type
	 * is LE or not.
	 */
	if (ev->addr.type != 0x00) {
		btclient_info("LE device connected");
		FSM_SET_BIT(DEVICE_CONNECTED);
	}

}

static void process_event(struct mgmt_event *event, const void *param)
{

	switch(event->opcode) {
	case DEVICE_CONNECTED_EVENT:
		handle_device_connected_event(event, param);
		break;
	case DEVICE_DISCONNECTED_EVENT:
		handle_device_disconnected_event(event, param);
		break;
	default:
		;
	}
}

gboolean bt_mgmt_event_handler(GIOChannel *source, GIOCondition condition, gpointer data)
{

	gchar buf[512];
	gsize bytes_read = 0;
	GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(buf), &bytes_read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		btclient_error("read operations on mgmt socket failed");
		return FALSE;
	}

	struct mgmt_event *event = (struct mgmt_event *) buf;

	process_event(event, buf + MGMT_HDR_SIZE);

	return TRUE;
}

/* This method is adapted from wanswitch */
int script_runner(GString ** ppad, const char *format, ...)
{
	char linebuf[BUFSIZ];
	ssize_t len;
	int rc = 1;

	if (format == NULL || *format == '\0') {
		btclient_error("Coding error: null parameter in %s line %d", __FILE__, __LINE__);
		assert(0);
		return rc;
	}

	/* Clear any existing data in the pad */
	if (ppad && *ppad) {
		g_string_truncate(*ppad, 0);
	}

	/* Format the variable argument list */
	gchar *cmd;
	va_list ap;
	va_start(ap, format);
	int ret = g_vasprintf(&cmd, format, ap);
	va_end(ap);

	if (ret < 1) {
		btclient_error("Coding error: empty required parameter in %s line %d", __FILE__, __LINE__);
		assert(0);
		return rc;
	}
	/* This is the command we are going to do */
	/* wanswitch_debug("%s", cmd); */
	FILE *fp = popen(cmd, "r");
	if (fp) {
		len = fread(linebuf, sizeof(char), BUFSIZ - 1, fp);
		/*This condition is to handle scripts which output single character. mdm_active file outputs single character */
		if (len == 1 && ppad) {
				linebuf[len] = '\0';
				g_string_append(*ppad, linebuf);
		}
		else {
			while (len > 0) {
				linebuf[len] = '\0';
				if (ppad) {
					g_string_append(*ppad, linebuf);
				}
				len = fread(linebuf, sizeof(char), BUFSIZ - 1, fp);
			}
		}
		rc = WEXITSTATUS(pclose(fp));
		/* wanswitch_debug("Child exit code: %i\n", rc); */
	}
	g_free(cmd);
	return rc;
}

/*TR-1559: 
	TODO: In future, we can expand the scope of this method.
		We may also return the compression type, if required.
*/
gboolean is_compressed_file(const char* file_name)
{
	if (g_str_has_suffix(file_name, ".gz"))
		return TRUE;

	return FALSE;
}

/*TR-1559
*/
time_t str_to_epoch_time(const char *time_string, const char *time_format)
{
	const struct tm good_time;

	strptime(time_string, time_format, &good_time);
	time_t epoch_time = mktime(&good_time);

	return epoch_time;
}

guint8 todigit(guchar c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}

	return 0;
}

guchar hex_to_char(int c)
{
	if (c >= 0 && c <= 9) {
		return c + '0';
	} else if (c >= 0x0A && c <= 0x0F) {
		return c + 'A' - 10;
	}

	return 0;
}

guchar calculate_check_digit(GString *str)
{
    int factor = 2;
    int sum = 0;
    int n = str->len;
    int i;

    for (i = str->len - 1; i >= 0; i--) {
            int code_point = (int)todigit(str->str[i]);
            int addend = factor * code_point;

            factor = (factor == 2) ? 1 : 2;

            addend = (addend / n) + (addend % n);
            sum += addend;
    }

    int remainder = sum % n;
    int check_code_point = n - remainder;
    check_code_point %= n;

    return hex_to_char(check_code_point);
}

GString* get_hostname_check_digit_tag(void)
{
	guchar check_digit = 'X';
	GString *str = g_string_new(NULL);

	int rc = script_runner(&str, "echo -n $(hostname | cut -b5-)");
	if (rc == 0) {
		check_digit = calculate_check_digit(str);
	}

	/* previous content is automatically destroyed */
	g_string_printf(str, "Tag-%c", check_digit);

	return str;
}

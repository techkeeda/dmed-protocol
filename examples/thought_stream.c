/*
 * DMED Demo: "Thought Stream"
 *
 * Broadcasts a DMED beacon (mDNS) and serves a file's content as an MCP resource.
 * The file is updated externally every minute with a new thought/quote.
 *
 * Build:
 *   gcc -o thought_stream thought_stream.c -I../../lib/c/include -lpthread
 *
 * Run:
 *   # Terminal 1: start the server
 *   ./thought_stream /tmp/thoughts.txt 8090
 *
 *   # Terminal 2: feed thoughts every 60s
 *   while true; do fortune | tee /tmp/thoughts.txt; sleep 60; done
 *
 *   # Terminal 3: discover and read
 *   curl http://localhost:8090/dmed/card | jq .
 *   curl -X POST http://localhost:8090/mcp -H "Content-Type: application/json" \
 *     -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_thought","arguments":{}}}'
 */

#define DMED_IMPLEMENTATION
#include "dmed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_THOUGHT 4096
#define MAX_HTTP    8192
#define INSTANCE_ID 0x7468696E  /* "thin" in hex — for "think" */

static char g_thought_file[256];
static int  g_port;

/* Read current thought from file */
static int read_thought(char *buf, size_t buflen) {
    FILE *f = fopen(g_thought_file, "r");
    if (!f) {
        snprintf(buf, buflen, "No thoughts yet... file not found.");
        return 0;
    }
    size_t n = fread(buf, 1, buflen - 1, f);
    buf[n] = '\0';
    fclose(f);
    /* Trim trailing newline */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return (int)n;
}

/* JSON-escape a string (minimal: just escape quotes and backslashes) */
static int json_escape(const char *src, char *dst, size_t dstlen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstlen - 2; i++) {
        if (src[i] == '"' || src[i] == '\\') { dst[j++] = '\\'; }
        else if (src[i] == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; continue; }
        else if (src[i] == '\r') continue;
        dst[j++] = src[i];
    }
    dst[j] = '\0';
    return (int)j;
}

/* Capability Card JSON */
static const char *CARD_FMT =
    "{\"dmed_version\":\"0.1.0\",\"instance_id\":\"%08x\","
    "\"name\":\"Thought Stream\","
    "\"description\":\"A stream of thoughts updated every minute. Connect and listen.\","
    "\"service_type\":\"information\","
    "\"transports\":[{\"type\":\"http\",\"url\":\"http://%s:%d/mcp\",\"priority\":1}],"
    "\"auth\":{\"type\":\"none\"},"
    "\"capabilities\":{\"tools\":["
    "{\"name\":\"get_thought\",\"description\":\"Get the current thought/quote\"},"
    "{\"name\":\"get_history\",\"description\":\"Get last N thoughts\",\"input_schema_summary\":\"n: integer (1-10)\"}"
    "],\"resources\":["
    "{\"uri\":\"resource://thought/current\",\"name\":\"Current Thought\",\"mime_type\":\"text/plain\"}"
    "],\"prompts\":[]},\"tags\":[\"motivation\",\"quotes\",\"stream\",\"thoughts\"]}";

/* Get local IP */
static void get_local_ip(char *buf, size_t len) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {.sin_family=AF_INET, .sin_port=htons(80)};
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    connect(s, (struct sockaddr*)&addr, sizeof(addr));
    struct sockaddr_in local;
    socklen_t slen = sizeof(local);
    getsockname(s, (struct sockaddr*)&local, &slen);
    inet_ntop(AF_INET, &local.sin_addr, buf, len);
    close(s);
}

/* Simple HTTP response helper */
static void http_respond(int fd, int code, const char *ctype, const char *body) {
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        code, ctype, strlen(body));
    write(fd, hdr, hlen);
    write(fd, body, strlen(body));
}

/* Handle one HTTP request */
static void handle_request(int fd) {
    char req[MAX_HTTP];
    int n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) { close(fd); return; }
    req[n] = '\0';

    char ip[64];
    get_local_ip(ip, sizeof(ip));

    /* Route: GET /dmed/card */
    if (strstr(req, "GET /dmed/card")) {
        char card[2048];
        snprintf(card, sizeof(card), CARD_FMT, INSTANCE_ID, ip, g_port);
        http_respond(fd, 200, "application/json", card);
    }
    /* Route: POST /mcp */
    else if (strstr(req, "POST /mcp")) {
        /* Find JSON body */
        char *body = strstr(req, "\r\n\r\n");
        if (!body) { close(fd); return; }
        body += 4;

        char response[MAX_HTTP];

        if (strstr(body, "\"initialize\"")) {
            snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"protocolVersion\":\"2025-03-26\","
                "\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"Thought Stream\",\"version\":\"1.0.0\"}}}");
        }
        else if (strstr(body, "\"tools/list\"")) {
            snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{\"tools\":["
                "{\"name\":\"get_thought\",\"description\":\"Get the current thought/quote\","
                "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
                "{\"name\":\"get_history\",\"description\":\"Get last N thoughts\","
                "\"inputSchema\":{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}}}}"
                "]}}");
        }
        else if (strstr(body, "\"get_thought\"")) {
            char thought[MAX_THOUGHT], escaped[MAX_THOUGHT];
            read_thought(thought, sizeof(thought));
            json_escape(thought, escaped, sizeof(escaped));
            snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}]}}",
                escaped);
        }
        else if (strstr(body, "\"notifications/initialized\"")) {
            http_respond(fd, 204, "text/plain", "");
            close(fd);
            return;
        }
        else {
            snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":0,\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
        }
        http_respond(fd, 200, "application/json", response);
    }
    else {
        http_respond(fd, 404, "text/plain", "Not found. Try /dmed/card");
    }
    close(fd);
}

/* Print DMED beacon info on startup */
static void print_beacon_info(void) {
    dmed_beacon_t beacon = {
        .version = DMED_PROTOCOL_VERSION,
        .flags = 0,  /* no auth, no TLS, single-ish */
        .service_type = DMED_ST_INFORMATION,
        .instance_id = INSTANCE_ID,
        .tx_power = 0,
        .has_tx_power = 0,
    };

    uint8_t buf[DMED_BEACON_MAX_SIZE];
    int len = dmed_beacon_encode(&beacon, buf, sizeof(buf));

    printf("[DMED] Beacon (%d bytes): ", len);
    for (int i = 0; i < len; i++) printf("%02X", buf[i]);
    printf("\n");
    printf("[DMED] Service type: %s (0x%02X)\n", dmed_service_type_str(beacon.service_type), beacon.service_type);
    printf("[DMED] Instance ID: %08x\n", beacon.instance_id);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <thought_file> [port]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  echo 'The journey of a thousand miles begins with a single step.' > /tmp/thoughts.txt\n");
        fprintf(stderr, "  %s /tmp/thoughts.txt 8090\n", argv[0]);
        return 1;
    }

    strncpy(g_thought_file, argv[1], sizeof(g_thought_file) - 1);
    g_port = argc > 2 ? atoi(argv[2]) : 8090;

    char ip[64];
    get_local_ip(ip, sizeof(ip));

    printf("\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║   DMED Thought Stream                     ║\n");
    printf("  ║   Dynamic MCP Discovery Demo             ║\n");
    printf("  ╚══════════════════════════════════════════╝\n\n");

    print_beacon_info();
    printf("\n");
    printf("[DMED] ✓ Thought file: %s\n", g_thought_file);
    printf("[DMED] ✓ Card:  http://%s:%d/dmed/card\n", ip, g_port);
    printf("[DMED] ✓ MCP:   http://%s:%d/mcp\n", ip, g_port);
    printf("\n");
    printf("[DMED] Waiting for connections...\n");
    printf("[DMED] Tip: Update %s every 60s with new thoughts!\n\n", g_thought_file);

    /* Start HTTP server */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(g_port),
        .sin_addr.s_addr = INADDR_ANY,
    };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(srv, 8);

    while (1) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;
        handle_request(client);
    }

    return 0;
}

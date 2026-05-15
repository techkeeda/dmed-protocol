/*
 * DMED Demo: "Thought Stream Client"
 *
 * Simulates a mobile device that:
 *   1. Scans the network for DMED servers (via HTTP for simplicity)
 *   2. Fetches the Capability Card
 *   3. Connects via MCP and polls for new thoughts every few seconds
 *
 * Build:
 *   gcc -o thought_client thought_client.c -I../lib/c/include -lpthread
 *
 * Run:
 *   ./thought_client <server_ip> <port>
 *   ./thought_client 192.168.1.42 8090
 */

#define DMED_IMPLEMENTATION
#include "dmed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define BUF_SIZE 8192

/* Simple HTTP request — returns response body length, body written to out */
static int http_request(const char *host, int port, const char *method,
                        const char *path, const char *body, char *out, size_t outlen) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }

    char req[BUF_SIZE];
    int reqlen;
    if (body) {
        reqlen = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
            method, path, host, port, strlen(body), body);
    } else {
        reqlen = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
            method, path, host, port);
    }
    write(s, req, reqlen);

    /* Read response */
    int total = 0, n;
    while ((n = read(s, out + total, outlen - total - 1)) > 0) total += n;
    out[total] = '\0';
    close(s);

    /* Skip headers, find body */
    char *bstart = strstr(out, "\r\n\r\n");
    if (bstart) {
        bstart += 4;
        int blen = total - (bstart - out);
        memmove(out, bstart, blen + 1);
        return blen;
    }
    return total;
}

/* Extract a JSON string value (very minimal parser) */
static int json_get_string(const char *json, const char *key, char *val, size_t vallen) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char *p = strstr(json, pattern);
    if (!p) return -1;
    p = strchr(p, ':');
    while (*++p == ' ');
    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i < vallen - 1) {
            if (*p == '\\' && *(p+1)) {
                p++;
                if (*p == 'n') val[i++] = '\n';
                else if (*p == 't') val[i++] = '\t';
                else val[i++] = *p;
            } else {
                val[i++] = *p;
            }
            p++;
        }
        val[i] = '\0';
        return (int)i;
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n\n", argv[0]);
        fprintf(stderr, "Simulates a mobile device discovering and connecting to a DMED Thought Stream.\n");
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    char buf[BUF_SIZE];

    printf("\n");
    printf("  ┌──────────────────────────────────────────┐\n");
    printf("  │  📱 DMED Client — Thought Stream Listener  │\n");
    printf("  └──────────────────────────────────────────┘\n\n");

    /* ─── Phase 1: SCAN (simulated — in real app this would be mDNS/BLE) ─── */
    printf("[SCAN] Looking for DMED server at %s:%d ...\n", host, port);

    /* ─── Phase 2: RESOLVE — Fetch Capability Card ─── */
    printf("[RESOLVE] Fetching capability card...\n");
    int n = http_request(host, port, "GET", "/dmed/card", NULL, buf, sizeof(buf));
    if (n <= 0) { fprintf(stderr, "[!] Failed to fetch card. Is the server running?\n"); return 1; }

    /* Parse card basics */
    char name[128], stype[64], instance_id[16], desc[256];
    json_get_string(buf, "name", name, sizeof(name));
    json_get_string(buf, "service_type", stype, sizeof(stype));
    json_get_string(buf, "instance_id", instance_id, sizeof(instance_id));
    json_get_string(buf, "description", desc, sizeof(desc));

    /* Decode instance_id and verify beacon would match */
    uint32_t iid = (uint32_t)strtoul(instance_id, NULL, 16);
    uint8_t beacon_type = dmed_service_type_from_str(stype);

    printf("\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║  DISCOVERED: %-27s  ║\n", name);
    printf("  ╠══════════════════════════════════════════╣\n");
    printf("  ║  ID:   %s                         ║\n", instance_id);
    printf("  ║  Type: %s (%s)            ║\n", dmed_service_type_str(beacon_type),
           (beacon_type == DMED_ST_INFORMATION) ? "📰" : "❓");
    printf("  ║  Desc: %-33s║\n", desc);
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("\n");

    /* ─── Phase 3: CONNECT — Initialize MCP session ─── */
    printf("[CONNECT] Establishing MCP session...\n");
    const char *init_req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
        "\"params\":{\"protocolVersion\":\"2025-03-26\",\"capabilities\":{},"
        "\"clientInfo\":{\"name\":\"DMED Mobile Client\",\"version\":\"0.1.0\"}}}";
    n = http_request(host, port, "POST", "/mcp", init_req, buf, sizeof(buf));
    if (n <= 0 || !strstr(buf, "protocolVersion")) {
        fprintf(stderr, "[!] MCP initialize failed\n"); return 1;
    }
    printf("[CONNECT] ✓ MCP session established\n\n");

    /* ─── Phase 4: LISTEN — Poll for thoughts ─── */
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Listening for thoughts... (Ctrl+C to stop)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    char last_thought[4096] = "";
    int thought_num = 0;

    while (1) {
        const char *call_req = "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"tools/call\","
            "\"params\":{\"name\":\"get_thought\",\"arguments\":{}}}";
        n = http_request(host, port, "POST", "/mcp", call_req, buf, sizeof(buf));

        if (n > 0) {
            char thought[4096];
            if (json_get_string(buf, "text", thought, sizeof(thought)) > 0) {
                /* Only print if thought changed */
                if (strcmp(thought, last_thought) != 0) {
                    thought_num++;
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    printf("  💭 #%d [%02d:%02d:%02d]\n", thought_num, t->tm_hour, t->tm_min, t->tm_sec);
                    printf("  %s\n\n", thought);
                    strncpy(last_thought, thought, sizeof(last_thought) - 1);
                }
            }
        }
        sleep(5);  /* Poll every 5 seconds */
    }

    return 0;
}

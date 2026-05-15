/*
 * DMED C Library — Example / Test
 *
 * Build: gcc -o dmed_test dmed_test.c -I../include
 */
#define DMED_IMPLEMENTATION
#include "dmed.h"
#include <stdio.h>

int main(void) {
    /* Encode a beacon */
    dmed_beacon_t beacon = {
        .version = DMED_PROTOCOL_VERSION,
        .flags = DMED_FLAG_MULTI,
        .service_type = DMED_ST_IOT_DEVICE,
        .instance_id = 0xA1B2C3D4,
        .tx_power = -20,
        .has_tx_power = 1,
        .name_hash = dmed_crc16((const uint8_t*)"Kitchen Light", 13),
        .has_name_hash = 1,
    };

    uint8_t buf[DMED_BEACON_MAX_SIZE];
    int len = dmed_beacon_encode(&beacon, buf, sizeof(buf));
    printf("Encoded beacon (%d bytes): ", len);
    for (int i = 0; i < len; i++) printf("%02X ", buf[i]);
    printf("\n");

    /* Decode it back */
    dmed_beacon_t decoded;
    int rc = dmed_beacon_decode(buf, len, &decoded);
    printf("Decode result: %d\n", rc);
    printf("  version=%d flags=0x%X type=%s(0x%02X) id=0x%08X tx=%d\n",
        decoded.version, decoded.flags,
        dmed_service_type_str(decoded.service_type), decoded.service_type,
        decoded.instance_id, decoded.tx_power);

    /* Instance ID from string */
    uint32_t id = dmed_instance_id_from_string("my-device-serial-123");
    printf("Instance ID from 'my-device-serial-123': 0x%08X\n", id);

    /* mDNS TXT */
    char txt[256];
    int txt_len = dmed_mdns_txt_encode(&beacon, "Kitchen Light", "/mcp", "/dmed/card", txt, sizeof(txt));
    printf("mDNS TXT (%d bytes):\n", txt_len);
    for (int i = 0; i < txt_len; i++) {
        if (txt[i] == '\0') printf("\n  ");
        else putchar(txt[i]);
    }
    printf("\n");

    return 0;
}

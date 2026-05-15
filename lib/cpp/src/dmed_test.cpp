/*
 * DMED C++ Library — Example / Test
 * Build: g++ -std=c++17 -o dmed_test dmed_test.cpp -I../include
 */
#include "dmed.hpp"
#include <cstdio>

int main() {
    using namespace dmed;

    // Encode
    Beacon b{.version=1, .flags=MULTI, .service_type=ServiceType::IotDevice,
             .instance_id=0xA1B2C3D4, .tx_power=-20, .name_hash=crc16("Kitchen Light")};
    uint8_t buf[BEACON_MAX_SIZE];
    size_t len = b.encode(buf, sizeof(buf));
    printf("Encoded (%zu bytes): ", len);
    for (size_t i=0;i<len;i++) printf("%02X ",buf[i]);
    printf("\n");

    // Decode
    auto dec = Beacon::decode(buf, len);
    if (dec) printf("Decoded: v=%d fl=%X st=%s id=%08X tx=%d\n",
        dec->version, dec->flags, std::string(service_type_str(dec->service_type)).c_str(),
        dec->instance_id, dec->tx_power.value_or(0));

    // Card
    Card card{.instance_id="a1b2c3d4", .name="Kitchen Light", .service_type="iot_device",
              .transports={{"http","http://192.168.1.42:8080/mcp",1}},
              .auth_type="none", .tools={{"toggle","Toggle light"},{"set_brightness","Set brightness 0-100"}}};
    printf("\nCard JSON:\n%s\n", card.to_json().c_str());

    // Instance ID
    printf("\nID from serial: %08X\n", instance_id_from("my-device-serial-123"));
    return 0;
}

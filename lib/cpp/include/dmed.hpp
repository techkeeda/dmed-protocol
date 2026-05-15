/*
 * DMED — Dynamic MCP Endpoint Discovery Protocol
 * C++ Library Header (dmed.hpp) — v0.2.0
 *
 * Header-only, modern C++17. Type-safe beacon encode/decode + Card builder + Action client.
 * Zero heap allocation for beacon ops.
 *
 * v0.2: Added ActionRequest, ActionResponse, and Client class for interaction.
 *
 * Usage:
 *   #include "dmed.hpp"
 */
#ifndef DMED_HPP
#define DMED_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <array>
#include <map>

namespace dmed {

constexpr const char* VERSION = "0.2.0";
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr size_t BEACON_MIN_SIZE = 6;
constexpr size_t BEACON_MAX_SIZE = 9;

enum Flag : uint8_t { AUTH=1, TLS=2, MULTI=4, CLOUD=8 };

enum class ServiceType : uint8_t {
    Unknown=0x00, IotDevice=0x01, Media=0x02, Appliance=0x03, Vehicle=0x04,
    RetailKiosk=0x05, Infrastructure=0x06, Computing=0x07, AiService=0x08,
    DataSource=0x09, ToolUtility=0x0A, Communication=0x0B, HealthMedical=0x0C,
    Industrial=0x0D, Environmental=0x0E, Security=0x0F, Information=0x10,
    Energy=0x11, Custom=0xFF,
};

struct Beacon {
    uint8_t version = PROTOCOL_VERSION;
    uint8_t flags = 0;
    ServiceType service_type = ServiceType::Unknown;
    uint32_t instance_id = 0;
    std::optional<int8_t> tx_power;
    std::optional<uint16_t> name_hash;

    size_t encode(uint8_t* buf, size_t len) const {
        size_t need = BEACON_MIN_SIZE + (tx_power?1:0) + (name_hash?2:0);
        if (len < need) return 0;
        buf[0] = ((version&0xF)<<4)|(flags&0xF);
        buf[1] = uint8_t(service_type);
        buf[2]=(instance_id>>24)&0xFF; buf[3]=(instance_id>>16)&0xFF;
        buf[4]=(instance_id>>8)&0xFF;  buf[5]=instance_id&0xFF;
        size_t p=6;
        if (tx_power) buf[p++]=uint8_t(*tx_power);
        if (name_hash) { buf[p++]=(*name_hash>>8)&0xFF; buf[p++]=*name_hash&0xFF; }
        return p;
    }

    static std::optional<Beacon> decode(const uint8_t* buf, size_t len) {
        if (len < BEACON_MIN_SIZE) return std::nullopt;
        Beacon b;
        b.version=(buf[0]>>4)&0xF; b.flags=buf[0]&0xF;
        b.service_type=ServiceType(buf[1]);
        b.instance_id=(uint32_t(buf[2])<<24)|(uint32_t(buf[3])<<16)|(uint32_t(buf[4])<<8)|buf[5];
        if (!b.version) return std::nullopt;
        if (len>=7) b.tx_power=int8_t(buf[6]);
        if (len>=9) b.name_hash=uint16_t(buf[7])<<8|buf[8];
        return b;
    }
};

struct Tool { std::string name, description; };
struct Transport { std::string type, url; int priority=10; };

struct Card {
    std::string instance_id, name, description, service_type;
    std::vector<Transport> transports;
    std::string auth_type = "none";
    std::vector<Tool> tools;

    std::string to_json() const {
        std::string j="{\"dmed_version\":\""+std::string(VERSION)+"\",\"instance_id\":\""+instance_id+"\",\"name\":\""+name+"\",\"service_type\":\""+service_type+"\",\"transports\":[";
        for (size_t i=0;i<transports.size();i++){
            if(i)j+=",";
            j+="{\"type\":\""+transports[i].type+"\",\"url\":\""+transports[i].url+"\",\"priority\":"+std::to_string(transports[i].priority)+"}";
        }
        j+="],\"auth\":{\"type\":\""+auth_type+"\"},\"capabilities\":{\"tools\":[";
        for (size_t i=0;i<tools.size();i++){
            if(i)j+=",";
            j+="{\"name\":\""+tools[i].name+"\",\"description\":\""+tools[i].description+"\"}";
        }
        j+="],\"resources\":[],\"prompts\":[]}}";
        return j;
    }
};

/* ─── v0.2: Action Interaction ─── */

struct ActionRequest {
    std::string action;
    std::string params_json; /* JSON object as string, e.g. {"size":"large"} */

    std::string to_json() const {
        if (params_json.empty())
            return "{\"action\":\""+action+"\",\"params\":{}}";
        return "{\"action\":\""+action+"\",\"params\":"+params_json+"}";
    }
};

struct ActionResponse {
    bool ok = false;
    std::string action;
    std::string result_text;
    std::string error_message;
};

/* ─── Utilities ─── */

inline uint16_t crc16(const void* data, size_t len) {
    uint16_t crc=0xFFFF;
    auto p=static_cast<const uint8_t*>(data);
    for (size_t i=0;i<len;i++){crc^=uint16_t(p[i])<<8;for(int j=0;j<8;j++)crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1;}
    return crc;
}
inline uint16_t crc16(std::string_view s){return crc16(s.data(),s.size());}

inline uint32_t instance_id_from(std::string_view s) {
    uint32_t c=0xFFFFFFFF;
    for(char ch:s){c^=uint8_t(ch);for(int i=0;i<8;i++)c=(c>>1)^(0xEDB88320&-(c&1));}
    return ~c;
}

inline std::string_view service_type_str(ServiceType st) {
    switch(st){
    case ServiceType::IotDevice:return "iot_device";case ServiceType::Media:return "media";
    case ServiceType::Appliance:return "appliance";case ServiceType::Vehicle:return "vehicle";
    case ServiceType::RetailKiosk:return "retail_kiosk";case ServiceType::Infrastructure:return "infrastructure";
    case ServiceType::Computing:return "computing";case ServiceType::AiService:return "ai_service";
    case ServiceType::DataSource:return "data_source";case ServiceType::ToolUtility:return "tool_utility";
    case ServiceType::Communication:return "communication";case ServiceType::HealthMedical:return "health_medical";
    case ServiceType::Industrial:return "industrial";case ServiceType::Environmental:return "environmental";
    case ServiceType::Security:return "security";case ServiceType::Information:return "information";
    case ServiceType::Energy:return "energy";case ServiceType::Custom:return "custom";
    default:return "unknown";}
}

} // namespace dmed
#endif

# Appendix A: JSON Schema — Capability Card

This is the normative JSON Schema for DMED Capability Cards. Implementations MUST validate cards against this schema.

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://dmed-protocol.org/schema/capability-card/v0.2.0",
  "title": "DMED Capability Card",
  "description": "Describes a DMED server's identity, transports, authentication, and MCP capabilities.",
  "type": "object",
  "required": ["dmed_version", "instance_id", "name", "service_type", "transports", "capabilities"],
  "additionalProperties": true,
  "properties": {
    "dmed_version": {
      "type": "string",
      "pattern": "^[0-9]+\\.[0-9]+\\.[0-9]+$",
      "description": "Semantic version of the DMED protocol this card conforms to."
    },
    "instance_id": {
      "type": "string",
      "pattern": "^[0-9a-f]{8}$",
      "description": "8-character lowercase hex string matching the beacon instance_id."
    },
    "name": {
      "type": "string",
      "minLength": 1,
      "maxLength": 128,
      "description": "Human-readable name of the server."
    },
    "description": {
      "type": "string",
      "maxLength": 512,
      "description": "Longer description of what this server does."
    },
    "service_type": {
      "type": "string",
      "enum": [
        "iot_device", "media", "appliance", "vehicle", "retail_kiosk",
        "infrastructure", "computing", "ai_service", "data_source",
        "tool_utility", "communication", "health_medical", "industrial",
        "environmental", "security", "information", "energy",
        "agriculture", "education", "entertainment", "logistics", "custom"
      ],
      "description": "Category of service. See Appendix E for full registry."
    },
    "vendor": {
      "type": "object",
      "properties": {
        "name": { "type": "string", "maxLength": 128 },
        "url": { "type": "string", "format": "uri" },
        "contact": { "type": "string", "maxLength": 256 }
      },
      "required": ["name"]
    },
    "transports": {
      "type": "array",
      "minItems": 1,
      "items": { "$ref": "#/$defs/transport" },
      "description": "Available transports for MCP session establishment."
    },
    "auth": { "$ref": "#/$defs/auth" },
    "capabilities": { "$ref": "#/$defs/capabilities" },
    "metadata": {
      "type": "object",
      "additionalProperties": true,
      "description": "Arbitrary key-value metadata."
    },
    "icon_url": {
      "type": "string",
      "format": "uri",
      "description": "URL to a square icon (min 64x64, PNG or SVG)."
    },
    "expires": {
      "type": "integer",
      "minimum": 0,
      "description": "Unix timestamp (seconds) when this card should be re-fetched."
    },
    "location": {
      "type": "object",
      "properties": {
        "latitude": { "type": "number", "minimum": -90, "maximum": 90 },
        "longitude": { "type": "number", "minimum": -180, "maximum": 180 },
        "description": { "type": "string", "maxLength": 256 }
      }
    },
    "tags": {
      "type": "array",
      "items": { "type": "string", "maxLength": 64 },
      "maxItems": 20,
      "description": "Searchable keywords."
    },
    "languages": {
      "type": "array",
      "items": { "type": "string", "pattern": "^[a-z]{2}(-[A-Z]{2})?$" },
      "description": "BCP 47 language tags supported by this server."
    },
    "_signature": { "$ref": "#/$defs/signature" }
  },
  "$defs": {
    "transport": {
      "type": "object",
      "required": ["type"],
      "properties": {
        "type": {
          "type": "string",
          "enum": ["http", "https", "ws", "wss", "ble_gatt", "thread_matter", "usb_serial"],
          "description": "Transport type. http/https/ws/wss for IP-based; ble_gatt for Bluetooth; thread_matter for Thread/Matter mesh; usb_serial for wired adapter. Additional types may be added in future versions."
        },
        "url": {
          "type": "string",
          "format": "uri",
          "description": "Full URL to MCP endpoint. Required for http/https/ws/wss."
        },
        "address": {
          "type": "string",
          "pattern": "^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$",
          "description": "BLE MAC address. Required for ble_gatt."
        },
        "service_uuid": {
          "type": "string",
          "description": "GATT service UUID for MCP. Required for ble_gatt."
        },
        "priority": {
          "type": "integer",
          "minimum": 0,
          "maximum": 255,
          "default": 10,
          "description": "Lower value = higher priority."
        },
        "bandwidth": {
          "type": "string",
          "enum": ["high", "medium", "low"]
        },
        "latency_ms": {
          "type": "integer",
          "minimum": 0,
          "description": "Expected round-trip latency in milliseconds."
        }
      },
      "allOf": [
        {
          "if": { "properties": { "type": { "enum": ["http", "https", "ws", "wss"] } } },
          "then": { "required": ["url"] }
        },
        {
          "if": { "properties": { "type": { "const": "ble_gatt" } } },
          "then": { "required": ["address", "service_uuid"] }
        }
      ]
    },
    "auth": {
      "type": "object",
      "required": ["type"],
      "properties": {
        "type": {
          "type": "string",
          "enum": ["none", "pin", "api_key", "oauth2", "mtls"]
        },
        "pin_mechanism": {
          "type": "string",
          "enum": ["device_screen", "sms", "email", "companion_app"],
          "description": "How the PIN is communicated to the user."
        },
        "oauth2": {
          "type": "object",
          "properties": {
            "authorization_url": { "type": "string", "format": "uri" },
            "token_url": { "type": "string", "format": "uri" },
            "scopes": { "type": "array", "items": { "type": "string" } },
            "client_id": { "type": "string" }
          },
          "required": ["authorization_url", "token_url"]
        },
        "mtls": {
          "type": "object",
          "properties": {
            "ca_cert_url": { "type": "string", "format": "uri" },
            "client_cert_required": { "type": "boolean" }
          },
          "required": ["ca_cert_url", "client_cert_required"]
        },
        "api_key": {
          "type": "object",
          "properties": {
            "header": { "type": "string", "default": "X-API-Key" },
            "query_param": { "type": "string" },
            "registration_url": { "type": "string", "format": "uri" }
          }
        }
      }
    },
    "capabilities": {
      "type": "object",
      "properties": {
        "tools": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["name"],
            "properties": {
              "name": { "type": "string", "pattern": "^[a-z][a-z0-9_]*$" },
              "description": { "type": "string", "maxLength": 256 },
              "input_schema_summary": { "type": "string", "maxLength": 512 }
            }
          }
        },
        "resources": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["uri", "name"],
            "properties": {
              "uri": { "type": "string" },
              "name": { "type": "string" },
              "description": { "type": "string", "maxLength": 256 },
              "mime_type": { "type": "string" }
            }
          }
        },
        "prompts": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["name"],
            "properties": {
              "name": { "type": "string" },
              "description": { "type": "string", "maxLength": 256 }
            }
          }
        }
      }
    },
    "signature": {
      "type": "object",
      "required": ["algorithm", "public_key", "signature", "signed_fields"],
      "properties": {
        "algorithm": { "type": "string", "enum": ["Ed25519"] },
        "public_key": { "type": "string", "description": "Base64url-encoded public key." },
        "signature": { "type": "string", "description": "Base64url-encoded signature." },
        "signed_fields": {
          "type": "array",
          "items": { "type": "string" },
          "description": "List of top-level field names included in signature computation."
        }
      }
    }
  }
}
```

## Validation Rules (Beyond Schema)

1. If `auth.type` is `"oauth2"`, the `auth.oauth2` object MUST be present.
2. If `auth.type` is `"mtls"`, the `auth.mtls` object MUST be present.
3. If `auth.type` is `"api_key"`, the `auth.api_key` object MUST be present.
4. If `auth.type` is `"pin"`, the `auth.pin_mechanism` field MUST be present.
5. At least one transport MUST have a reachable endpoint (not just BLE discovery-only).
6. The `instance_id` MUST be exactly 8 lowercase hex characters.
7. Tool names MUST match the pattern `^[a-z][a-z0-9_]*$` (lowercase, underscores, starts with letter).

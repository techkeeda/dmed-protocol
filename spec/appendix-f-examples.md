# Appendix F: Examples — Complete Flows

This appendix provides complete, copy-paste-ready examples for implementers.

---

## F.1 Example: Smart Light (BLE + WiFi)

### Server Configuration

A Philips Hue-style smart light bulb running on an ESP32.

**Beacon (BLE advertisement payload, hex):**
```
10 01 A1B2C3D4 EC 7F3A
│  │  │         │  └── name_hash: CRC-16 of "Kitchen Light" = 0x7F3A
│  │  │         └── tx_power: -20 dBm (0xEC signed)
│  │  └── instance_id: 0xA1B2C3D4
│  └── service_type: 0x01 (iot_device)
└── version=1, flags=0x0 (no auth, no TLS, single tool=false, not cloud-backed)
```

Wait — flags should be 0x4 (MULTI=1, multiple tools). Corrected:
```
14 01 A1B2C3D4 EC 7F3A
```

**mDNS advertisement (simultaneous):**
```
Kitchen Light._mcp-dmed._tcp.local. 120 IN SRV 0 0 8080 kitchen-light.local.
Kitchen Light._mcp-dmed._tcp.local. 4500 IN TXT "v=1" "id=a1b2c3d4" "st=01" "fl=4" "nm=Kitchen Light" "path=/mcp" "card=/dmed/card"
kitchen-light.local. 120 IN A 192.168.1.42
```

**Capability Card (served at http://192.168.1.42:8080/dmed/card):**
```json
{
  "dmed_version": "0.2.0",
  "instance_id": "a1b2c3d4",
  "name": "Kitchen Light",
  "description": "RGBW smart bulb with dimming and color temperature control",
  "service_type": "iot_device",
  "vendor": {
    "name": "SmartBulb Inc",
    "url": "https://smartbulb.example.com"
  },
  "transports": [
    {
      "type": "http",
      "url": "http://192.168.1.42:8080/mcp",
      "priority": 1,
      "bandwidth": "high",
      "latency_ms": 5
    },
    {
      "type": "ble_gatt",
      "address": "AA:BB:CC:DD:EE:FF",
      "service_uuid": "D14D0001-1000-4000-8000-00805F9B34FB",
      "priority": 10,
      "bandwidth": "low",
      "latency_ms": 50
    }
  ],
  "auth": {
    "type": "none"
  },
  "capabilities": {
    "tools": [
      {
        "name": "set_brightness",
        "description": "Set brightness level",
        "input_schema_summary": "level: integer (0-100)"
      },
      {
        "name": "set_color",
        "description": "Set light color",
        "input_schema_summary": "color: string (hex '#RRGGBB' or name like 'warm_white')"
      },
      {
        "name": "set_color_temperature",
        "description": "Set color temperature in Kelvin",
        "input_schema_summary": "kelvin: integer (2700-6500)"
      },
      {
        "name": "get_status",
        "description": "Get current light state (on/off, brightness, color)"
      },
      {
        "name": "toggle",
        "description": "Toggle light on/off"
      }
    ],
    "resources": [
      {
        "uri": "resource://light/status",
        "name": "Light Status",
        "description": "Real-time light state as JSON",
        "mime_type": "application/json"
      }
    ],
    "prompts": []
  },
  "metadata": {
    "firmware": "2.1.0",
    "model": "SB-RGBW-E27",
    "location": "Kitchen"
  },
  "tags": ["lighting", "smart-home", "color", "dimmable"],
  "icon_url": "http://192.168.1.42:8080/icon.png",
  "expires": 1747360000
}
```

**MCP Session Example (HTTP POST to http://192.168.1.42:8080/mcp):**

Request — Initialize:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-03-26",
    "capabilities": {},
    "clientInfo": {
      "name": "DMED Mobile App",
      "version": "1.0.0"
    }
  }
}
```

Response:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2025-03-26",
    "capabilities": {
      "tools": {}
    },
    "serverInfo": {
      "name": "Kitchen Light",
      "version": "2.1.0"
    }
  }
}
```

Request — Call tool:
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "set_brightness",
    "arguments": {
      "level": 75
    }
  }
}
```

Response:
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Brightness set to 75%"
      }
    ]
  }
}
```

---

## F.2 Example: Coffee Shop Kiosk (WiFi only)

### Server Configuration

A Raspberry Pi running behind the counter at a coffee shop.

**mDNS advertisement:**
```
Joes Coffee._mcp-dmed._tcp.local. 120 IN SRV 0 0 9000 joes-kiosk.local.
Joes Coffee._mcp-dmed._tcp.local. 4500 IN TXT "v=1" "id=cafe0001" "st=05" "fl=4" "nm=Joe's Coffee" "path=/mcp" "card=/dmed/card"
joes-kiosk.local. 120 IN A 10.0.0.5
```

**Capability Card:**
```json
{
  "dmed_version": "0.2.0",
  "instance_id": "cafe0001",
  "name": "Joe's Coffee — Order & Menu",
  "description": "Browse our menu, place orders, and check order status",
  "service_type": "retail_kiosk",
  "vendor": {
    "name": "Joe's Coffee Shop",
    "url": "https://joescoffee.example.com"
  },
  "transports": [
    {
      "type": "http",
      "url": "http://10.0.0.5:9000/mcp",
      "priority": 1
    }
  ],
  "auth": {
    "type": "none"
  },
  "capabilities": {
    "tools": [
      {
        "name": "get_menu",
        "description": "Get the full menu with prices and availability"
      },
      {
        "name": "place_order",
        "description": "Place an order for pickup",
        "input_schema_summary": "items: array of {item_id, quantity, customizations}"
      },
      {
        "name": "get_order_status",
        "description": "Check status of an existing order",
        "input_schema_summary": "order_id: string"
      },
      {
        "name": "get_wait_time",
        "description": "Get estimated wait time for new orders"
      }
    ],
    "resources": [
      {
        "uri": "resource://menu/today",
        "name": "Today's Menu",
        "description": "Current menu with daily specials",
        "mime_type": "application/json"
      }
    ],
    "prompts": [
      {
        "name": "recommend",
        "description": "Get a drink recommendation based on preferences"
      }
    ]
  },
  "location": {
    "latitude": 37.7749,
    "longitude": -122.4194,
    "description": "123 Main St, San Francisco"
  },
  "tags": ["coffee", "food", "ordering", "cafe"],
  "languages": ["en", "es"],
  "metadata": {
    "hours": "Mon-Fri 6am-6pm, Sat-Sun 7am-4pm",
    "payment_methods": ["apple_pay", "google_pay", "credit_card"]
  }
}
```

---

## F.3 Example: Cloud AI Service (Internet)

### DNS Record

```
_dmed.api.smartllm.example.com. 3600 IN TXT "v=1;id=llm00001;st=08;fl=3;url=https://api.smartllm.example.com/mcp;name=SmartLLM API"
```

### Capability Card (at https://api.smartllm.example.com/.well-known/dmed/card)

```json
{
  "dmed_version": "0.2.0",
  "instance_id": "llm00001",
  "name": "SmartLLM API",
  "description": "General-purpose language model with text generation, summarization, and translation",
  "service_type": "ai_service",
  "vendor": {
    "name": "SmartLLM Corp",
    "url": "https://smartllm.example.com",
    "contact": "support@smartllm.example.com"
  },
  "transports": [
    {
      "type": "https",
      "url": "https://api.smartllm.example.com/mcp",
      "priority": 1,
      "bandwidth": "high",
      "latency_ms": 200
    },
    {
      "type": "wss",
      "url": "wss://api.smartllm.example.com/mcp/ws",
      "priority": 2,
      "bandwidth": "high",
      "latency_ms": 50
    }
  ],
  "auth": {
    "type": "oauth2",
    "oauth2": {
      "authorization_url": "https://auth.smartllm.example.com/authorize",
      "token_url": "https://auth.smartllm.example.com/token",
      "scopes": ["generate", "summarize", "translate"],
      "client_id": "dmed_public_client_001"
    }
  },
  "capabilities": {
    "tools": [
      {
        "name": "generate_text",
        "description": "Generate text from a prompt",
        "input_schema_summary": "prompt: string, max_tokens?: integer, temperature?: number"
      },
      {
        "name": "summarize",
        "description": "Summarize a document or text",
        "input_schema_summary": "text: string, max_length?: integer, style?: 'bullet' | 'paragraph'"
      },
      {
        "name": "translate",
        "description": "Translate text between languages",
        "input_schema_summary": "text: string, source_lang?: string, target_lang: string"
      }
    ],
    "resources": [],
    "prompts": [
      {
        "name": "creative_writing",
        "description": "Template for creative writing tasks"
      },
      {
        "name": "code_review",
        "description": "Template for code review feedback"
      }
    ]
  },
  "tags": ["llm", "text-generation", "translation", "summarization"],
  "languages": ["en", "es", "fr", "de", "ja", "zh"],
  "metadata": {
    "model": "smartllm-v3",
    "context_window": 128000,
    "pricing": "See https://smartllm.example.com/pricing"
  },
  "expires": 1747446400
}
```

---

## F.4 Example: Industrial Sensor (Ethernet + mTLS)

### mDNS Advertisement

```
CNC-7-Telemetry._mcp-dmed._tcp.local. 120 IN SRV 0 0 8443 cnc7.factory.local.
CNC-7-Telemetry._mcp-dmed._tcp.local. 4500 IN TXT "v=1" "id=cnc70001" "st=0d" "fl=3" "nm=CNC Machine 7" "path=/mcp" "card=/dmed/card"
cnc7.factory.local. 120 IN A 172.16.0.107
```

### Capability Card

```json
{
  "dmed_version": "0.2.0",
  "instance_id": "cnc70001",
  "name": "CNC Machine 7 — Telemetry",
  "description": "Real-time telemetry and diagnostics for CNC milling machine #7",
  "service_type": "industrial",
  "vendor": {
    "name": "FactoryTech GmbH",
    "url": "https://factorytech.example.com"
  },
  "transports": [
    {
      "type": "https",
      "url": "https://172.16.0.107:8443/mcp",
      "priority": 1,
      "bandwidth": "high",
      "latency_ms": 2
    }
  ],
  "auth": {
    "type": "mtls",
    "mtls": {
      "ca_cert_url": "https://pki.factory.local/ca.pem",
      "client_cert_required": true
    }
  },
  "capabilities": {
    "tools": [
      {
        "name": "get_temperature",
        "description": "Get spindle/bearing temperature readings",
        "input_schema_summary": "sensor?: 'spindle' | 'bearing_a' | 'bearing_b' | 'coolant'"
      },
      {
        "name": "get_vibration",
        "description": "Get vibration analysis data",
        "input_schema_summary": "axis?: 'x' | 'y' | 'z', range?: '1m' | '1h' | '24h'"
      },
      {
        "name": "get_tool_wear",
        "description": "Get current tool wear percentage and remaining life estimate"
      },
      {
        "name": "get_production_stats",
        "description": "Get parts produced, cycle time, and efficiency metrics",
        "input_schema_summary": "period?: 'shift' | 'day' | 'week'"
      },
      {
        "name": "get_alarms",
        "description": "Get active and recent alarms/faults"
      }
    ],
    "resources": [
      {
        "uri": "resource://telemetry/realtime",
        "name": "Real-time Telemetry Stream",
        "description": "Live sensor data (subscribable)",
        "mime_type": "application/json"
      }
    ],
    "prompts": []
  },
  "tags": ["cnc", "manufacturing", "telemetry", "predictive-maintenance"],
  "metadata": {
    "machine_model": "DMG MORI NHX 5000",
    "serial": "NHX5K-2024-0742",
    "install_date": "2024-03-15",
    "location": "Building A, Bay 7"
  }
}
```

---

## F.5 Test Vectors

### Beacon Binary Encoding

| Input | Expected Hex Output |
|-------|-------------------|
| version=1, flags=0, st=0x01, id=0xA1B2C3D4 | `10 01 A1B2C3D4` |
| version=1, flags=0xF, st=0x08, id=0x00000001 | `1F 08 00000001` |
| version=1, flags=0x3, st=0x0D, id=0xCNC70001, tx=-10 | `13 0D CNC70001 F6` |
| version=1, flags=0x4, st=0x01, id=0xA1B2C3D4, tx=-20, nh=0x7F3A | `14 01 A1B2C3D4 EC 7F3A` |

### Instance ID Generation

| Input | Method | Expected instance_id |
|-------|--------|---------------------|
| MAC `AA:BB:CC:DD:EE:FF` | CRC-32 | `3e4f5a6b` (example) |
| Serial `SN-2024-0742` | Lower 32 bits of SHA-256 | `a7c3e1f9` (example) |
| Random | Random uint32 | Any 8-char hex |

### Name Hash (CRC-16/CCITT)

| Input String | Expected name_hash (hex) |
|-------------|-------------------------|
| "Kitchen Light" | Compute CRC-16/CCITT with poly=0x1021, init=0xFFFF |
| "CNC Machine 7" | Compute CRC-16/CCITT with poly=0x1021, init=0xFFFF |

CRC-16/CCITT parameters:
- Polynomial: 0x1021
- Initial value: 0xFFFF
- Input reflected: false
- Output reflected: false
- Final XOR: 0x0000

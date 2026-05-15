# Appendix E: Service Type Registry

## E.1 Service Type Codes

The `service_type` field classifies what kind of service a DMED server provides. This helps clients filter and display appropriate icons/categories.

### Beacon Format (8-bit hex)

| Code | String Identifier | Category | Description | Examples |
|------|------------------|----------|-------------|----------|
| `0x00` | `unknown` | Unknown | Unclassified service | — |
| `0x01` | `iot_device` | IoT Device | Connected sensor or actuator | Smart light, thermostat, motion sensor, smart plug |
| `0x02` | `media` | Media | Audio/video playback or capture | TV, speaker, projector, camera, streaming box |
| `0x03` | `appliance` | Appliance | Large household device | Fridge, washer, dryer, oven, dishwasher |
| `0x04` | `vehicle` | Vehicle | Transportation device | Car, e-bike, scooter, EV charger, fleet tracker |
| `0x05` | `retail_kiosk` | Retail/Kiosk | Point of sale or service | Ordering kiosk, vending machine, ticket machine |
| `0x06` | `infrastructure` | Infrastructure | Network/building systems | Router, access point, gateway, elevator, HVAC |
| `0x07` | `computing` | Computing | General-purpose computer | Server, NAS, desktop, Raspberry Pi, cluster node |
| `0x08` | `ai_service` | AI Service | AI/ML inference endpoint | LLM, image gen, speech-to-text, embeddings |
| `0x09` | `data_source` | Data Source | Queryable data provider | Database, API, time-series store, log aggregator |
| `0x0A` | `tool_utility` | Tool/Utility | Single-purpose tool | Calculator, unit converter, QR generator, formatter |
| `0x0B` | `communication` | Communication | Messaging/calling system | PBX, intercom, radio, paging system |
| `0x0C` | `health_medical` | Health/Medical | Health monitoring device | Heart rate monitor, scale, blood pressure, glucose |
| `0x0D` | `industrial` | Industrial | Manufacturing/process control | PLC, SCADA, CNC, robot arm, conveyor |
| `0x0E` | `environmental` | Environmental | Environmental monitoring | Weather station, air quality, soil moisture, UV |
| `0x0F` | `security` | Security | Physical security device | Camera, smart lock, alarm panel, access control |
| `0x10` | `information` | Information | Information/directory service | Digital signage, knowledge base, directory, map |
| `0x11` | `energy` | Energy | Power/energy management | Solar inverter, battery, smart meter, grid tie |
| `0x12` | `agriculture` | Agriculture | Farming/growing systems | Irrigation controller, greenhouse, livestock tracker |
| `0x13` | `education` | Education | Learning/training tools | Interactive whiteboard, lab equipment, LMS |
| `0x14` | `entertainment` | Entertainment | Games/leisure | Arcade machine, escape room, interactive exhibit |
| `0x15` | `logistics` | Logistics | Shipping/tracking | Package tracker, warehouse robot, dock sensor |
| `0x16`-`0xEF` | — | Reserved | Future standard categories | — |
| `0xF0`-`0xFE` | — | Vendor Range | Vendor-specific categories | Must include vendor ID in metadata |
| `0xFF` | `custom` | Custom | Fully custom category | Defined in Capability Card metadata |

## E.2 String Identifiers

In the Capability Card JSON, service types are represented as strings (the "String Identifier" column above). In beacons, they are the 8-bit hex code.

Mapping between the two is defined by this registry. Clients MUST support both representations.

## E.3 Extending the Registry

New service types MAY be proposed via:
1. Opening an issue on the DMED specification repository
2. Providing: proposed code, string identifier, category name, description, and 3+ examples
3. Community review period of 14 days
4. Merge into next minor version of this appendix

Vendor-range codes (`0xF0`-`0xFE`) may be used without registration but:
- MUST include `x-vendor-service-type` in the Capability Card metadata
- MUST NOT conflict with other vendors on the same network (use instance_id for disambiguation)

## E.4 Icon Recommendations

Clients SHOULD display category-appropriate icons. Recommended icon mappings:

| Category | Suggested Icon | Unicode |
|----------|---------------|---------|
| iot_device | 💡 | U+1F4A1 |
| media | 🎵 | U+1F3B5 |
| appliance | 🏠 | U+1F3E0 |
| vehicle | 🚗 | U+1F697 |
| retail_kiosk | 🛒 | U+1F6D2 |
| infrastructure | 🌐 | U+1F310 |
| computing | 💻 | U+1F4BB |
| ai_service | 🤖 | U+1F916 |
| data_source | 📊 | U+1F4CA |
| tool_utility | 🔧 | U+1F527 |
| communication | 📞 | U+1F4DE |
| health_medical | ❤️ | U+2764 |
| industrial | ⚙️ | U+2699 |
| environmental | 🌡️ | U+1F321 |
| security | 🔒 | U+1F512 |
| information | ℹ️ | U+2139 |
| energy | ⚡ | U+26A1 |
| agriculture | 🌱 | U+1F331 |
| education | 📚 | U+1F4DA |
| entertainment | 🎮 | U+1F3AE |
| logistics | 📦 | U+1F4E6 |
| unknown/custom | ❓ | U+2753 |

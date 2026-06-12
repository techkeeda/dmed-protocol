# DMED Things — Reference Capability Cards

Ready-to-use capability card templates for common devices and services. Manufacturers and hobbyists can copy these as a starting point.

## Categories

| Directory | What's Inside |
|-----------|--------------|
| `lighting/` | Bulbs, strips, panels, dimmers |
| `fans/` | Ceiling fans, exhaust, portable |
| `appliances/` | Vacuum, washer, fridge, oven |
| `security/` | Cameras, locks, alarms, doorbells |
| `media/` | TVs, speakers, projectors |
| `climate/` | AC, thermostat, heater, humidifier |
| `energy/` | Smart plugs, meters, solar inverters |
| `health/` | Scales, BP monitors, air purifiers |

## How to Use

1. Pick the template closest to your device
2. Copy it as your `/dmed/card` response
3. Update `instance_id`, `name`, `metadata.location`
4. Implement the tools listed

Each file is a valid DMED Capability Card JSON — serve it directly from your device.

import { describe, expect, it } from 'vitest'
import { createRegistry, registerDevice, slugify, toolsFromCard, unregisterDevice } from '../src/tool-mapper.js'
import type { DmedDevice } from '@dmed/discovery'

function makeDevice(name: string, tools: { name: string; description: string; inputSchema?: Record<string, unknown> }[]): DmedDevice {
  return {
    instanceId: name,
    name,
    discoveredVia: 'mdns',
    address: '192.168.1.1:3100',
    httpHost: '192.168.1.1',
    httpPort: 3100,
    card: {
      dmed_version: '0.2.0',
      instance_id: name,
      name,
      description: '',
      service_type: 'iot_device',
      capabilities: { tools }
    }
  }
}

describe('slugify', () => {
  it('lowercases and replaces spaces with underscores', () => {
    expect(slugify('Smart Coffee Machine')).toBe('smart_coffee_machine')
  })

  it('replaces special characters with underscores and trims', () => {
    expect(slugify('Device #1 (test)')).toBe('device_1_test')
  })

  it('leaves an already-lowercase slug with underscores unchanged', () => {
    expect(slugify('already_lowercase')).toBe('already_lowercase')
  })

  it('truncates names longer than 32 characters', () => {
    const long = 'This Is A Really Long Device Name That Exceeds The Limit'
    const result = slugify(long)
    expect(result.length).toBeLessThanOrEqual(32)
  })
})

describe('toolsFromCard', () => {
  it('produces one MCP tool per device action', () => {
    const device = makeDevice('Smart Coffee Machine', [
      { name: 'brew_coffee', description: 'Brew coffee' },
      { name: 'get_status', description: 'Get status' },
      { name: 'set_temperature', description: 'Set temperature' }
    ])
    expect(toolsFromCard(device)).toHaveLength(3)
  })

  it('names tools as {device_slug}__{action_name}', () => {
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    expect(toolsFromCard(device)[0].name).toBe('smart_coffee_machine__brew_coffee')
  })

  it('formats description as "{action.description} ({device.name})"', () => {
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    expect(toolsFromCard(device)[0].description).toBe('Brew coffee (Smart Coffee Machine)')
  })

  it('copies inputSchema verbatim from the card', () => {
    const schema = { type: 'object', properties: { size: { type: 'string' } } }
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee', inputSchema: schema }])
    expect(toolsFromCard(device)[0].inputSchema).toEqual(schema)
  })

  it('returns an empty array for a device with no tools', () => {
    const device = makeDevice('Empty Device', [])
    expect(toolsFromCard(device)).toEqual([])
  })

  it('returns an empty array for a device with no card', () => {
    const device: DmedDevice = {
      instanceId: 'x',
      name: 'Uninitialized',
      discoveredVia: 'mdns',
      address: '192.168.1.1:3100'
    }
    expect(toolsFromCard(device)).toEqual([])
  })

  it('two devices with the same action name get different prefixes, no collision', () => {
    const kitchen = makeDevice('Kitchen Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    const office = makeDevice('Office Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    const names = [...toolsFromCard(kitchen), ...toolsFromCard(office)].map(t => t.name)
    expect(new Set(names).size).toBe(2)
  })
})

describe('registry', () => {
  it('registerDevice adds both tools and the device', () => {
    const registry = createRegistry()
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    registerDevice(registry, device)
    expect(registry.tools.has('smart_coffee_machine__brew_coffee')).toBe(true)
    expect(registry.devices.has('smart_coffee_machine')).toBe(true)
  })

  it('unregisterDevice removes the device and its tools', () => {
    const registry = createRegistry()
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    registerDevice(registry, device)
    unregisterDevice(registry, device)
    expect(registry.tools.size).toBe(0)
    expect(registry.devices.size).toBe(0)
  })

  it('unregisterDevice does not remove tools belonging to other devices', () => {
    const registry = createRegistry()
    const coffee = makeDevice('Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    const lamp = makeDevice('Smart Lamp', [{ name: 'turn_on', description: 'Turn on' }])
    registerDevice(registry, coffee)
    registerDevice(registry, lamp)
    unregisterDevice(registry, coffee)
    expect(registry.tools.has('smart_lamp__turn_on')).toBe(true)
    expect(registry.devices.has('smart_lamp')).toBe(true)
  })

  it('registerDevice excludes a device not present in the allowlist', () => {
    const registry = createRegistry()
    const device = makeDevice('Smart Coffee Machine', [{ name: 'brew_coffee', description: 'Brew coffee' }])
    registerDevice(registry, device, { devices: ['smart_lamp'] })
    expect(registry.devices.has('smart_coffee_machine')).toBe(false)
    expect(registry.tools.size).toBe(0)
  })

  it('registerDevice includes only allowlisted actions for an allowlisted device', () => {
    const registry = createRegistry()
    const device = makeDevice('Smart Coffee Machine', [
      { name: 'brew_coffee', description: 'Brew coffee' },
      { name: 'get_status', description: 'Get status' }
    ])
    registerDevice(registry, device, {
      devices: ['smart_coffee_machine'],
      actions: { smart_coffee_machine: ['get_status'] }
    })
    expect(registry.tools.has('smart_coffee_machine__get_status')).toBe(true)
    expect(registry.tools.has('smart_coffee_machine__brew_coffee')).toBe(false)
  })
})

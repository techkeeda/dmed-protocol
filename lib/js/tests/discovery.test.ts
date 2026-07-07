import { describe, it, expect } from 'vitest'
import { DmedDiscovery } from '../src/discovery.js'
import type { DmedDevice } from '../src/types.js'

class TestableDiscovery extends DmedDiscovery {
  expose_addDevice(d: DmedDevice) { this.addDevice(d) }
  expose_removeDevice(key: string) { this.removeDevice(key) }
}

function makeDevice(instanceId: string, via: 'ble' | 'mdns' = 'mdns'): DmedDevice {
  return {
    instanceId,
    name: `Device ${instanceId}`,
    discoveredVia: via,
    address: via === 'mdns' ? '192.168.1.1:3100' : 'AA:BB:CC:DD:EE:FF',
    httpHost: via === 'mdns' ? '192.168.1.1' : undefined,
    httpPort: via === 'mdns' ? 3100 : undefined
  }
}

describe('DmedDiscovery', () => {
  it('fires device event when device added', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    expect(seen).toHaveLength(1)
    expect(seen[0].instanceId).toBe('abc')
  })

  it('does not fire duplicate device event for same instanceId', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    discovery.expose_addDevice(makeDevice('abc'))
    expect(seen).toHaveLength(1)
  })

  it('fires lost event when device removed', () => {
    const discovery = new TestableDiscovery()
    const lost: DmedDevice[] = []
    discovery.on('lost', d => lost.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    discovery.expose_removeDevice('abc')
    expect(lost).toHaveLength(1)
  })

  it('getDevices returns all current devices', () => {
    const discovery = new TestableDiscovery()
    discovery.expose_addDevice(makeDevice('a'))
    discovery.expose_addDevice(makeDevice('b'))
    expect(discovery.getDevices()).toHaveLength(2)
  })

  it('getDevices excludes removed devices', () => {
    const discovery = new TestableDiscovery()
    discovery.expose_addDevice(makeDevice('a'))
    discovery.expose_addDevice(makeDevice('b'))
    discovery.expose_removeDevice('a')
    const devices = discovery.getDevices()
    expect(devices).toHaveLength(1)
    expect(devices[0].instanceId).toBe('b')
  })

  it('two devices with different instanceIds both appear', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('x'))
    discovery.expose_addDevice(makeDevice('y'))
    expect(seen).toHaveLength(2)
  })
})

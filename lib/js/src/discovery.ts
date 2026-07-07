import type { DmedDevice, DeviceEventHandler } from './types.js'
import { fetchCard } from './transport.js'

export class DmedDiscovery {
  private deviceHandlers: DeviceEventHandler[] = []
  private lostHandlers: DeviceEventHandler[] = []
  private devices = new Map<string, DmedDevice>()

  on(event: 'device' | 'lost', handler: DeviceEventHandler): this {
    if (event === 'device') this.deviceHandlers.push(handler)
    if (event === 'lost') this.lostHandlers.push(handler)
    return this
  }

  async connect(device: DmedDevice): Promise<DmedDevice> {
    if (!device.httpHost || !device.httpPort) {
      throw new Error('connect() requires httpHost and httpPort')
    }
    const card = await fetchCard(device.httpHost, device.httpPort)
    return { ...device, card }
  }

  protected addDevice(device: DmedDevice): void {
    const key = device.instanceId || device.address
    if (!this.devices.has(key)) {
      this.devices.set(key, device)
      this.deviceHandlers.forEach(h => h(device))
    }
  }

  protected removeDevice(key: string): void {
    const device = this.devices.get(key)
    if (device) {
      this.devices.delete(key)
      this.lostHandlers.forEach(h => h(device))
    }
  }

  getDevices(): DmedDevice[] {
    return Array.from(this.devices.values())
  }
}

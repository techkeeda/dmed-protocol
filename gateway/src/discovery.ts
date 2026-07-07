import { Bonjour, type Browser, type Service } from 'bonjour-service'
import { DmedDiscovery, fetchCard } from '@dmed/discovery'
import type { DmedDevice } from '@dmed/discovery'

export class GatewayDiscovery extends DmedDiscovery {
  private bonjour = new Bonjour()
  private browser: Browser | null = null

  start(): void {
    this.browser = this.bonjour.find({ type: 'dmed' })
    this.browser.on('up', (service: Service) => {
      void this.handleUp(service)
    })
    this.browser.on('down', (service: Service) => {
      this.handleDown(service)
    })
  }

  stop(): void {
    this.browser?.stop()
    this.bonjour.destroy()
  }

  private async handleUp(service: Service): Promise<void> {
    const host = service.referer?.address ?? service.addresses?.[0]
    const port = service.port
    if (!host || !port) return

    const instanceId = (service.txt?.id as string | undefined) ?? ''

    try {
      const card = await fetchCard(host, port)
      const device: DmedDevice = {
        instanceId,
        name: service.name,
        discoveredVia: 'mdns',
        address: `${host}:${port}`,
        httpHost: host,
        httpPort: port,
        card
      }
      this.addDevice(device)
    } catch {
      // Card endpoint unreachable — do not register a device without capabilities.
    }
  }

  private handleDown(service: Service): void {
    // Must match the key addDevice() derives: instanceId if present, else "host:port".
    const instanceId = (service.txt?.id as string | undefined) ?? ''
    const host = service.referer?.address ?? service.addresses?.[0]
    const address = host && service.port ? `${host}:${service.port}` : service.name
    this.removeDevice(instanceId || address)
  }
}

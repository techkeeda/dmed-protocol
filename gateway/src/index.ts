import { fileURLToPath } from 'url'
import { loadConfig } from './auth.js'
import { GatewayDiscovery } from './discovery.js'
import { startListening } from './listen.js'
import { createRegistry, registerDevice, unregisterDevice } from './tool-mapper.js'
import { createServer } from './server.js'

const CONFIG_PATH = process.env.DMED_GATEWAY_CONFIG ?? './config.json'

async function main(): Promise<void> {
  const config = loadConfig(CONFIG_PATH)
  const port = config.port ?? 4100

  const registry = createRegistry()
  const discovery = new GatewayDiscovery()
  discovery.on('device', device => {
    registerDevice(registry, device, config.allowlist)
    console.error(`[Gateway] + ${device.name} (${registry.tools.size} tools registered)`)
  })
  discovery.on('lost', device => {
    unregisterDevice(registry, device)
    console.error(`[Gateway] - ${device.name}`)
  })
  discovery.start()

  const app = createServer(registry, config)
  const scheme = config.tls ? 'https' : 'http'
  startListening(app, config, port, () => {
    console.error(`[Gateway] DMED Gateway listening on :${port} (${scheme})`)
    console.error(`[Gateway] MCP endpoint: POST ${scheme}://localhost:${port}/mcp`)
  })
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  main().catch(err => {
    console.error(`[Gateway] Failed to start: ${err instanceof Error ? err.message : err}`)
    process.exit(1)
  })
}

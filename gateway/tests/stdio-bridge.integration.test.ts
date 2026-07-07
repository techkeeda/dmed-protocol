import { spawn } from 'child_process'
import { existsSync } from 'fs'
import type { Server } from 'http'
import { dirname, join } from 'path'
import { fileURLToPath } from 'url'
import { afterAll, beforeAll, describe, expect, it } from 'vitest'
import { createServer } from '../src/server.js'
import { createRegistry, registerDevice } from '../src/tool-mapper.js'
import type { GatewayConfig, ToolRegistry } from '../src/types.js'

// Spawns the *compiled* bridge (requires `npm run build` first) — this specific bug
// class (premature exit on stdin close racing pending fetches) only shows up in a real
// subprocess; a unit test calling handleLine() directly can't observe process lifecycle.
const DIST_BRIDGE = join(dirname(fileURLToPath(import.meta.url)), '../dist/stdio-bridge.js')

function runBridge(input: string, env: Record<string, string>): Promise<string> {
  return new Promise((resolve, reject) => {
    const child = spawn('node', [DIST_BRIDGE], { env: { ...process.env, ...env } })
    let stdout = ''
    let stderr = ''
    child.stdout.on('data', chunk => (stdout += chunk))
    child.stderr.on('data', chunk => (stderr += chunk))
    child.on('close', code => {
      if (code !== 0) return reject(new Error(`bridge exited ${code}: ${stderr}`))
      resolve(stdout)
    })
    child.stdin.write(input)
    child.stdin.end()
  })
}

describe.skipIf(!existsSync(DIST_BRIDGE))('stdio-bridge (compiled) against a real gateway', () => {
  const config: GatewayConfig = { apiKey: 'bridge-test-key' }
  let server: Server
  let port: number
  let registry: ToolRegistry

  beforeAll(async () => {
    registry = createRegistry()
    registerDevice(registry, {
      instanceId: 'bridge01',
      name: 'Bridge Test Device',
      discoveredVia: 'mdns',
      address: '127.0.0.1:0',
      httpHost: '127.0.0.1',
      httpPort: 0,
      card: {
        dmed_version: '0.2.0',
        instance_id: 'bridge01',
        name: 'Bridge Test Device',
        description: '',
        service_type: 'iot_device',
        capabilities: { tools: [{ name: 'ping', description: 'Ping' }] }
      }
    })
    server = createServer(registry, config).listen(0)
    await new Promise(resolve => server.once('listening', resolve))
    const address = server.address()
    port = typeof address === 'object' && address ? address.port : 0
  })

  afterAll(() => {
    server.close()
  })

  it('drains in-flight requests before exiting on stdin close', async () => {
    const input =
      '{"jsonrpc":"2.0","id":1,"method":"initialize"}\n' +
      '{"jsonrpc":"2.0","method":"notifications/initialized"}\n' +
      '{"jsonrpc":"2.0","id":2,"method":"tools/list"}\n'

    const stdout = await runBridge(input, {
      DMED_GATEWAY_URL: `http://localhost:${port}`,
      DMED_GATEWAY_API_KEY: config.apiKey
    })

    const lines = stdout
      .trim()
      .split('\n')
      .filter(Boolean)
      .map(line => JSON.parse(line))

    expect(lines).toHaveLength(2)
    expect(lines.find(l => l.id === 1)?.result?.serverInfo?.name).toBe('DMED Gateway')
    expect(lines.find(l => l.id === 2)?.result?.tools?.[0]?.name).toBe('bridge_test_device__ping')
  })
})

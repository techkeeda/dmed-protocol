#!/usr/bin/env node
import { createInterface } from 'readline'
import { handleLine } from './bridge.js'

const gatewayUrl = process.env.DMED_GATEWAY_URL
const apiKey = process.env.DMED_GATEWAY_API_KEY

if (!gatewayUrl || !apiKey) {
  console.error('[dmed-bridge] DMED_GATEWAY_URL and DMED_GATEWAY_API_KEY must both be set')
  process.exit(1)
}

const rl = createInterface({ input: process.stdin })

// stdin closing (Claude Desktop shutting the subprocess down) must not cut off
// responses to requests still in flight — track them and drain before exiting.
const pending = new Set<Promise<void>>()

rl.on('line', line => {
  const task = handleLine(line, { gatewayUrl, apiKey })
    .then(response => {
      if (response !== undefined) process.stdout.write(`${response}\n`)
    })
    .catch(err => {
      console.error(`[dmed-bridge] Unexpected error: ${err instanceof Error ? err.message : err}`)
    })
  pending.add(task)
  void task.finally(() => pending.delete(task))
})

rl.on('close', () => {
  void Promise.allSettled(pending).then(() => process.exit(0))
})

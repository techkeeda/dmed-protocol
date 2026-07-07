import { mkdtempSync, rmSync, writeFileSync } from 'fs'
import { tmpdir } from 'os'
import { join } from 'path'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import express from 'express'
import { startListening } from '../src/listen.js'
import type { GatewayConfig } from '../src/types.js'

describe('startListening', () => {
  let dir: string

  beforeEach(() => {
    dir = mkdtempSync(join(tmpdir(), 'dmed-listen-'))
  })

  afterEach(() => {
    rmSync(dir, { recursive: true, force: true })
  })

  it('uses plain app.listen when no tls config is present', () => {
    const app = express()
    const fakeServer = {} as ReturnType<typeof app.listen>
    const listenSpy = vi.spyOn(app, 'listen').mockReturnValue(fakeServer)
    const onListening = vi.fn()
    const config: GatewayConfig = { apiKey: 'k' }

    const result = startListening(app, config, 4100, onListening)

    expect(listenSpy).toHaveBeenCalledWith(4100, onListening)
    expect(result).toBe(fakeServer)
  })

  it('uses https.createServer with the configured cert/key when tls is present', () => {
    const certPath = join(dir, 'cert.pem')
    const keyPath = join(dir, 'key.pem')
    writeFileSync(certPath, 'FAKE-CERT')
    writeFileSync(keyPath, 'FAKE-KEY')

    const app = express()
    const fakeHttpsServer = { listen: vi.fn().mockReturnThis() }
    const httpsFactory = vi.fn().mockReturnValue(fakeHttpsServer)
    const onListening = vi.fn()
    const config: GatewayConfig = { apiKey: 'k', tls: { cert: certPath, key: keyPath } }

    const result = startListening(app, config, 4443, onListening, httpsFactory as unknown as typeof import('https').createServer)

    expect(httpsFactory).toHaveBeenCalledWith({ cert: Buffer.from('FAKE-CERT'), key: Buffer.from('FAKE-KEY') }, app)
    expect(fakeHttpsServer.listen).toHaveBeenCalledWith(4443, onListening)
    expect(result).toBe(fakeHttpsServer)
  })
})

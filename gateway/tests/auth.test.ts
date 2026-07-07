import { mkdtempSync, rmSync, writeFileSync } from 'fs'
import { tmpdir } from 'os'
import { join } from 'path'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { NextFunction, Request, Response } from 'express'
import { ConfigError, apiKeyAuth, loadConfig, safeCompare } from '../src/auth.js'

function makeReq(apiKey: string | undefined): Request {
  return { header: (name: string) => (name.toLowerCase() === 'x-api-key' ? apiKey : undefined) } as unknown as Request
}

function makeRes() {
  const res = {} as Response & { statusCode?: number; body?: unknown }
  res.status = vi.fn((code: number) => {
    res.statusCode = code
    return res
  }) as unknown as Response['status']
  res.json = vi.fn((body: unknown) => {
    res.body = body
    return res
  }) as unknown as Response['json']
  return res
}

describe('safeCompare', () => {
  it('returns true for identical strings', () => {
    expect(safeCompare('secret-key', 'secret-key')).toBe(true)
  })

  it('returns false for different strings of the same length', () => {
    expect(safeCompare('secret-key', 'secret-kex')).toBe(false)
  })

  it('returns false (not throws) for strings of different lengths', () => {
    expect(() => safeCompare('short', 'a-much-longer-secret')).not.toThrow()
    expect(safeCompare('short', 'a-much-longer-secret')).toBe(false)
  })

  it('returns false when compared against an empty string', () => {
    expect(safeCompare('', 'secret-key')).toBe(false)
  })
})

describe('apiKeyAuth middleware', () => {
  const config = { apiKey: 'secret-key' }

  it('request with correct X-API-Key header passes through', () => {
    const req = makeReq('secret-key')
    const res = makeRes()
    const next = vi.fn()
    apiKeyAuth(config)(req, res, next as unknown as NextFunction)
    expect(next).toHaveBeenCalledOnce()
    expect(res.status).not.toHaveBeenCalled()
  })

  it('request with wrong key gets 401', () => {
    const req = makeReq('wrong-key')
    const res = makeRes()
    const next = vi.fn()
    apiKeyAuth(config)(req, res, next as unknown as NextFunction)
    expect(next).not.toHaveBeenCalled()
    expect(res.statusCode).toBe(401)
  })

  it('request with no key gets 401', () => {
    const req = makeReq(undefined)
    const res = makeRes()
    const next = vi.fn()
    apiKeyAuth(config)(req, res, next as unknown as NextFunction)
    expect(next).not.toHaveBeenCalled()
    expect(res.statusCode).toBe(401)
  })
})

describe('loadConfig', () => {
  let dir: string

  beforeEach(() => {
    dir = mkdtempSync(join(tmpdir(), 'dmed-gateway-'))
  })

  afterEach(() => {
    rmSync(dir, { recursive: true, force: true })
  })

  it('loads the API key from a config file, not hardcoded', () => {
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ apiKey: 'from-file-key' }))
    const config = loadConfig(path)
    expect(config.apiKey).toBe('from-file-key')
  })

  it('throws ConfigError when the config file is missing', () => {
    const path = join(dir, 'does-not-exist.json')
    expect(() => loadConfig(path)).toThrow(ConfigError)
  })

  it('throws ConfigError when apiKey is missing from the config file', () => {
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ port: 4100 }))
    expect(() => loadConfig(path)).toThrow(ConfigError)
  })

  it('loads successfully when tls is omitted', () => {
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ apiKey: 'k' }))
    expect(() => loadConfig(path)).not.toThrow()
  })

  it('loads successfully when tls cert and key files both exist', () => {
    const certPath = join(dir, 'cert.pem')
    const keyPath = join(dir, 'key.pem')
    writeFileSync(certPath, 'fake-cert')
    writeFileSync(keyPath, 'fake-key')
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ apiKey: 'k', tls: { cert: certPath, key: keyPath } }))
    const config = loadConfig(path)
    expect(config.tls?.cert).toBe(certPath)
  })

  it('throws ConfigError when tls cert file does not exist', () => {
    const keyPath = join(dir, 'key.pem')
    writeFileSync(keyPath, 'fake-key')
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ apiKey: 'k', tls: { cert: join(dir, 'missing-cert.pem'), key: keyPath } }))
    expect(() => loadConfig(path)).toThrow(ConfigError)
  })

  it('throws ConfigError when tls is present but cert/key are not strings', () => {
    const path = join(dir, 'config.json')
    writeFileSync(path, JSON.stringify({ apiKey: 'k', tls: {} }))
    expect(() => loadConfig(path)).toThrow(ConfigError)
  })
})

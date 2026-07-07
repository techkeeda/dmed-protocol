import { createHash, timingSafeEqual } from 'crypto'
import { existsSync, readFileSync } from 'fs'
import type { NextFunction, Request, Response } from 'express'
import type { GatewayConfig } from './types.js'

// Hashing first means both digests are the same fixed length, so timingSafeEqual
// never has to branch on input length — a plain `key !== config.apiKey` (or a
// length-check-then-compare) leaks how many characters matched via timing.
export function safeCompare(a: string, b: string): boolean {
  const hashA = createHash('sha256').update(a).digest()
  const hashB = createHash('sha256').update(b).digest()
  return timingSafeEqual(hashA, hashB)
}

export class ConfigError extends Error {}

export function loadConfig(path: string): GatewayConfig {
  if (!existsSync(path)) {
    throw new ConfigError(
      `Config file not found: ${path}. Copy config.json.example to config.json and set an apiKey.`
    )
  }
  const parsed = JSON.parse(readFileSync(path, 'utf-8'))
  if (!parsed.apiKey || typeof parsed.apiKey !== 'string') {
    throw new ConfigError(`Config file ${path} must define a string "apiKey"`)
  }
  if (parsed.tls !== undefined) {
    const { cert, key } = parsed.tls ?? {}
    if (typeof cert !== 'string' || typeof key !== 'string') {
      throw new ConfigError(`Config file ${path}: "tls" must define string "cert" and "key" paths`)
    }
    if (!existsSync(cert)) throw new ConfigError(`TLS cert file not found: ${cert}`)
    if (!existsSync(key)) throw new ConfigError(`TLS key file not found: ${key}`)
  }
  return parsed as GatewayConfig
}

export function apiKeyAuth(config: GatewayConfig) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const key = req.header('X-API-Key')
    if (!key || !safeCompare(key, config.apiKey)) {
      res.status(401).json({ error: 'Unauthorized' })
      return
    }
    next()
  }
}

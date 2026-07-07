import { describe, it, expect } from 'vitest'
import { parseCard, validateCard, CardValidationError } from '../src/card.js'

const VALID_CARD_JSON = JSON.stringify({
  dmed_version: '0.2.0',
  instance_id: 'c0ffee01',
  name: 'Smart Coffee Machine',
  description: 'Brews coffee',
  service_type: 'iot_device',
  transports: [{ type: 'http', url: 'http://192.168.1.42:3100/mcp', priority: 1 }],
  auth: { type: 'none' },
  capabilities: {
    tools: [
      { name: 'brew_coffee', description: 'Brew coffee' },
      { name: 'get_status', description: 'Get status' }
    ]
  }
})

describe('parseCard', () => {
  it('parses valid card JSON', () => {
    const card = parseCard(VALID_CARD_JSON)
    expect(card.name).toBe('Smart Coffee Machine')
    expect(card.capabilities.tools).toHaveLength(2)
  })

  it('throws on invalid JSON', () => {
    expect(() => parseCard('{not json}')).toThrow(CardValidationError)
  })

  it('throws on non-object JSON', () => {
    expect(() => parseCard('"just a string"')).toThrow(CardValidationError)
  })

  it('throws on missing dmed_version', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.dmed_version
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws on missing instance_id', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.instance_id
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws on missing capabilities', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.capabilities
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws when capabilities.tools is not an array', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    card.capabilities.tools = 'not an array'
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('tolerates unknown extra fields (forward compat)', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    card.future_field = 'ignored'
    expect(() => validateCard(card)).not.toThrow()
  })
})

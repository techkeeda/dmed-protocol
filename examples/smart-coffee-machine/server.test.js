import { describe, it, expect, beforeEach } from 'vitest'
import request from 'supertest'
import { createApp } from './server.js'

// ─── GET /dmed/card ───────────────────────────────────────────────────────────

describe('GET /dmed/card', () => {
  const { app } = createApp()

  it('returns 200', async () => {
    const res = await request(app).get('/dmed/card')
    expect(res.status).toBe(200)
  })

  it('has required DMED fields', async () => {
    const res = await request(app).get('/dmed/card')
    expect(res.body).toMatchObject({
      dmed_version: expect.any(String),
      instance_id: expect.any(String),
      name: expect.any(String),
      transports: expect.any(Array),
    })
  })

  it('capabilities includes brew_coffee, get_status, set_temperature', async () => {
    const res = await request(app).get('/dmed/card')
    const names = res.body.capabilities.tools.map(t => t.name)
    expect(names).toContain('brew_coffee')
    expect(names).toContain('get_status')
    expect(names).toContain('set_temperature')
  })
})

// ─── GET /dmed/actions ────────────────────────────────────────────────────────

describe('GET /dmed/actions', () => {
  const { app } = createApp()

  it('returns 200 with actions array', async () => {
    const res = await request(app).get('/dmed/actions')
    expect(res.status).toBe(200)
    expect(res.body.actions).toBeInstanceOf(Array)
    expect(res.body.actions.length).toBeGreaterThan(0)
  })

  it('action count matches capability card tools', async () => {
    const cardRes = await request(app).get('/dmed/card')
    const actionsRes = await request(app).get('/dmed/actions')
    expect(actionsRes.body.actions.length).toBe(cardRes.body.capabilities.tools.length)
  })

  it('each action has name and description', async () => {
    const res = await request(app).get('/dmed/actions')
    for (const action of res.body.actions) {
      expect(action).toHaveProperty('name')
      expect(action).toHaveProperty('description')
    }
  })
})

// ─── POST /dmed/action — brew_coffee ─────────────────────────────────────────

describe('POST /dmed/action — brew_coffee', () => {
  let app, state

  beforeEach(() => {
    ({ app, state } = createApp())
  })

  it('valid params → 200 with ok status', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'large' } })
    expect(res.status).toBe(200)
    expect(res.body.status).toBe('ok')
  })

  it('result text mentions drink_type and size', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'espresso', size: 'small' } })
    expect(res.body.result.text.toLowerCase()).toMatch(/espresso/)
    expect(res.body.result.text.toLowerCase()).toMatch(/small/)
  })

  it('water_level decreases after a large brew', async () => {
    const before = state.water_level
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'large' } })
    expect(state.water_level).toBe(before - 20)
  })

  it('water_level decreases after a medium brew', async () => {
    const before = state.water_level
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'medium' } })
    expect(state.water_level).toBe(before - 15)
  })

  it('water_level decreases after a small brew', async () => {
    const before = state.water_level
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'espresso', size: 'small' } })
    expect(state.water_level).toBe(before - 10)
  })

  it('last_drink updated after brew', async () => {
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'cappuccino', size: 'medium' } })
    expect(state.last_drink).toBe('medium cappuccino')
  })

  it('brew when water_level < 10 → ok status but error text', async () => {
    state.water_level = 5
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'small' } })
    expect(res.status).toBe(200)
    expect(res.body.status).toBe('ok')
    expect(res.body.result.text.toLowerCase()).toMatch(/water/)
  })

  it('brew when water_level < 10 → water_level unchanged', async () => {
    state.water_level = 5
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'small' } })
    expect(state.water_level).toBe(5)
  })
})

// ─── POST /dmed/action — get_status ──────────────────────────────────────────

describe('POST /dmed/action — get_status', () => {
  it('returns water_level, bean_level, temperature, is_brewing, last_drink', async () => {
    const { app } = createApp()
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'get_status' })
    const body = JSON.parse(res.body.result.text)
    expect(body).toHaveProperty('water_level')
    expect(body).toHaveProperty('bean_level')
    expect(body).toHaveProperty('temperature')
    expect(body).toHaveProperty('is_brewing')
    expect(body).toHaveProperty('last_drink')
  })

  it('reflects state changes from a brew', async () => {
    const { app, state } = createApp()
    state.last_drink = 'large latte'
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'get_status' })
    const body = JSON.parse(res.body.result.text)
    expect(body.last_drink).toBe('large latte')
  })
})

// ─── POST /dmed/action — set_temperature ─────────────────────────────────────

describe('POST /dmed/action — set_temperature', () => {
  let app, state

  beforeEach(() => {
    ({ app, state } = createApp())
  })

  it('valid temperature → ok status', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'set_temperature', params: { temperature: 93 } })
    expect(res.body.status).toBe('ok')
  })

  it('updates state.temperature', async () => {
    await request(app)
      .post('/dmed/action')
      .send({ action: 'set_temperature', params: { temperature: 94 } })
    expect(state.temperature).toBe(94)
  })

  it('result text includes the temperature value', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'set_temperature', params: { temperature: 91 } })
    expect(res.body.result.text).toMatch(/91/)
  })
})

// ─── POST /dmed/action — error cases ─────────────────────────────────────────

describe('POST /dmed/action — error cases', () => {
  const { app } = createApp()

  it('missing action field → 400', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ params: {} })
    expect(res.status).toBe(400)
    expect(res.body.status).toBe('error')
  })

  it('unknown action → 404', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'make_tea' })
    expect(res.status).toBe(404)
    expect(res.body.status).toBe('error')
  })

  it('missing action field error message is helpful', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({})
    expect(res.body.message).toBeTruthy()
  })
})

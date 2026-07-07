import { describe, expect, it } from 'vitest'
import { isActionAllowed, isDeviceAllowed } from '../src/allowlist.js'

describe('isDeviceAllowed', () => {
  it('allows any device when no allowlist is configured', () => {
    expect(isDeviceAllowed(undefined, 'smart_coffee_machine')).toBe(true)
  })

  it('allows any device when allowlist.devices is not set', () => {
    expect(isDeviceAllowed({}, 'smart_coffee_machine')).toBe(true)
  })

  it('allows a device present in the allowlist', () => {
    expect(isDeviceAllowed({ devices: ['smart_coffee_machine'] }, 'smart_coffee_machine')).toBe(true)
  })

  it('rejects a device absent from the allowlist', () => {
    expect(isDeviceAllowed({ devices: ['smart_coffee_machine'] }, 'smart_lamp')).toBe(false)
  })
})

describe('isActionAllowed', () => {
  it('allows any action when no allowlist is configured', () => {
    expect(isActionAllowed(undefined, 'smart_coffee_machine', 'brew_coffee')).toBe(true)
  })

  it('allows any action for an allowlisted device with no action list', () => {
    expect(isActionAllowed({ devices: ['smart_coffee_machine'] }, 'smart_coffee_machine', 'brew_coffee')).toBe(true)
  })

  it('rejects all actions for a non-allowlisted device', () => {
    expect(isActionAllowed({ devices: ['smart_lamp'] }, 'smart_coffee_machine', 'brew_coffee')).toBe(false)
  })

  it('allows an action present in the per-device action list', () => {
    const allowlist = { devices: ['smart_coffee_machine'], actions: { smart_coffee_machine: ['get_status'] } }
    expect(isActionAllowed(allowlist, 'smart_coffee_machine', 'get_status')).toBe(true)
  })

  it('rejects an action absent from the per-device action list', () => {
    const allowlist = { devices: ['smart_coffee_machine'], actions: { smart_coffee_machine: ['get_status'] } }
    expect(isActionAllowed(allowlist, 'smart_coffee_machine', 'brew_coffee')).toBe(false)
  })
})

import { describe, expect, it, vi } from 'vitest'
import { DeviceNotFoundError, ToolNotFoundError, dispatchToolCall, resolveToolCall } from '../src/proxy.js'
import { createRegistry, registerDevice } from '../src/tool-mapper.js'
import type { DmedDevice, ActionResponse } from '@dmed/discovery'

function makeDevice(name: string): DmedDevice {
  return {
    instanceId: name,
    name,
    discoveredVia: 'mdns',
    address: '192.168.1.1:3100',
    httpHost: '192.168.1.1',
    httpPort: 3100,
    card: {
      dmed_version: '0.2.0',
      instance_id: name,
      name,
      description: '',
      service_type: 'iot_device',
      capabilities: { tools: [{ name: 'brew_coffee', description: 'Brew coffee' }] }
    }
  }
}

describe('resolveToolCall', () => {
  it('resolves a registered tool to its device and action', () => {
    const registry = createRegistry()
    registerDevice(registry, makeDevice('Coffee Machine'))
    const { device, action } = resolveToolCall('coffee_machine__brew_coffee', registry)
    expect(device.name).toBe('Coffee Machine')
    expect(action).toBe('brew_coffee')
  })

  it('throws ToolNotFoundError for an unknown tool name', () => {
    const registry = createRegistry()
    registerDevice(registry, makeDevice('Coffee Machine'))
    expect(() => resolveToolCall('unknown__tool', registry)).toThrow(ToolNotFoundError)
  })

  it('throws DeviceNotFoundError when the tool is known but the device went offline', () => {
    const registry = createRegistry()
    registerDevice(registry, makeDevice('Coffee Machine'))
    registry.devices.delete('coffee_machine')
    expect(() => resolveToolCall('coffee_machine__brew_coffee', registry)).toThrow(DeviceNotFoundError)
  })
})

describe('dispatchToolCall', () => {
  const device = makeDevice('Coffee Machine')

  it('dispatches to the correct device and action', async () => {
    const dispatch = vi.fn(async (): Promise<ActionResponse> => ({ status: 'ok', action: 'brew_coffee' }))
    await dispatchToolCall(device, 'brew_coffee', { drink_type: 'latte' }, dispatch)
    expect(dispatch).toHaveBeenCalledWith(device, 'brew_coffee', { drink_type: 'latte' })
  })

  it('passes params through unchanged', async () => {
    const params = { drink_type: 'latte', size: 'large' }
    const dispatch = vi.fn(async (): Promise<ActionResponse> => ({ status: 'ok', action: 'brew_coffee' }))
    await dispatchToolCall(device, 'brew_coffee', params, dispatch)
    expect(dispatch.mock.calls[0][2]).toEqual(params)
  })

  it('maps an ok response to a non-error result', async () => {
    const dispatch = vi.fn(
      async (): Promise<ActionResponse> => ({ status: 'ok', action: 'brew_coffee', result: { text: 'Brewing!' } })
    )
    const result = await dispatchToolCall(device, 'brew_coffee', {}, dispatch)
    expect(result.isError).toBe(false)
    expect(result.text).toBe('Brewing!')
  })

  it('maps an error response to an error result', async () => {
    const dispatch = vi.fn(
      async (): Promise<ActionResponse> => ({ status: 'error', action: 'brew_coffee', message: 'Water level too low' })
    )
    const result = await dispatchToolCall(device, 'brew_coffee', {}, dispatch)
    expect(result.isError).toBe(true)
    expect(result.text).toBe('Water level too low')
  })

  it('maps a network error (device offline) to an error result without throwing', async () => {
    const dispatch = vi.fn(async (): Promise<ActionResponse> => {
      throw new Error('ECONNREFUSED')
    })
    const result = await dispatchToolCall(device, 'brew_coffee', {}, dispatch)
    expect(result.isError).toBe(true)
    expect(result.text).toBe('ECONNREFUSED')
  })
})

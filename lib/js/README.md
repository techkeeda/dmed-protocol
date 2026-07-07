# @dmed/discovery

TypeScript/JavaScript library for the DMED protocol — capability card parsing,
transport selection, action dispatch, and a multi-transport discovery base class.

## Install

```bash
npm install
npm run build
```

## Usage

```ts
import { parseCard, dispatchAction, DmedDiscovery } from '@dmed/discovery'

const card = parseCard(cardJson)

const discovery = new DmedDiscovery()
discovery.on('device', device => console.log('found', device.name))
```

`DmedDiscovery` is transport-agnostic: it exposes `addDevice`/`removeDevice` for
subclasses to call when a BLE or mDNS scanner finds or loses a device, and
deduplicates by `instanceId`.

## Test

```bash
npm test
```

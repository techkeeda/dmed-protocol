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

## Conformance CLI

`dmed-conform` runs a non-mutating checklist against any live DMED endpoint —
useful for any implementer (not just this repo's examples) to self-check their
device before calling it DMED-conformant.

```bash
npm run build
node dist/cli.js http://<host>:<port>          # human-readable report, exit 0/1
node dist/cli.js http://<host>:<port> --json    # machine-readable, for CI
node dist/cli.js http://<host>:<port> --call get_status  # opt into one live dispatch
```

Checks: `/dmed/card` returns a valid card, required fields present, tools are
well-formed, `/dmed/actions` count matches `capabilities.tools`, missing/unknown
actions get the right 400/404, and (if present) `transports` entries are
well-formed. `--call <action>` is opt-in because actions can mutate device state
(e.g. brewing coffee) — omit it to run fully read-only checks.

## Test

```bash
npm test
```

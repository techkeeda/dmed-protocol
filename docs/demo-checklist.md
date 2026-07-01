# DMED Demo Checklist

## Setup

- [ ] `cd examples/smart-coffee-machine && npm start`
- [ ] Confirm: `[DMED] ✓ Broadcasting "Smart Coffee Machine" on _dmed._tcp port 3100`
- [ ] Confirm: `http://<laptop-IP>:3100/dmed/card` returns JSON in browser
- [ ] Phone and laptop on same WiFi network
- [ ] `CLAUDE_API_KEY` set in `android/dmed-scanner-working/local.properties`

## Scanner

- [ ] Open DMED Scanner app on Android
- [ ] Tap "Scan for DMED Devices"
- [ ] "Smart Coffee Machine ☕" appears within 10 seconds (via mDNS)
- [ ] Transport shown as "mDNS" in device card
- [ ] Tap the card — chat screen opens
- [ ] Capability card loads: shows name, description, available actions

## Natural language interaction

- [ ] Type: "what can you do?" — Claude lists available actions
- [ ] Type: "make me a large latte" — `brew_coffee` dispatched, confirmation shown
- [ ] Type: "how much water is left?" — `get_status` called, Claude narrates result
- [ ] Type: "set temperature to 94" — `set_temperature` dispatched with correct param
- [ ] Quick-action chips still work alongside free-text input

## Edge cases

- [ ] Type nonsense: "what's the weather?" — Claude explains it can't do that
- [ ] Stop coffee machine server — next action shows error message gracefully, no crash
- [ ] Restart server — rescan, reconnect, works again
- [ ] Low water: drain `water_level` to 5 in server state, try brew — correct error returned

## Tests

- [ ] `npm test` in `examples/smart-coffee-machine/` → 22 tests green
- [ ] `./gradlew test` in `android/dmed-scanner-working/` → all green

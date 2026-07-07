# Deploying the DMED Gateway on a Raspberry Pi

Run the [DMED-MCP Gateway](../../gateway/README.md) on a Raspberry Pi as an always-on
bridge between your local DMED devices (BLE, mDNS/Ethernet) and remote MCP clients —
the "home server" deployment target from the [architecture doc](../architecture.md).

This is about running the *gateway*, not a DMED device itself. If you're instead putting
a DMED-speaking service on the public internet (a hosted API, not a physical device),
see [Share on the Internet](../tutorials/share-on-internet.md) — that's a different,
DNS-based transport and doesn't involve this gateway at all.

---

## Prerequisites

- Raspberry Pi 3B+ or newer (the gateway is I/O-bound, not CPU-bound — a Pi Zero 2 W works too)
- Raspberry Pi OS (or any Debian-based distro) with network access
- **The Pi must be on the same LAN segment (same L2 broadcast domain) as your DMED
  devices.** mDNS discovery is multicast — it does not cross routed subnets or VLANs
  without an mDNS reflector. If your coffee machine is on the WiFi VLAN and the Pi is on
  a wired VLAN, discovery will silently find nothing.

## 1. Install Node.js

```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
node --version   # expect v20.x
```

## 2. Clone and build

```bash
git clone https://github.com/<your-fork>/dmed-protocol.git
cd dmed-protocol/lib/js && npm install && npm run build
cd ../../gateway && npm install && npm run build
```

`gateway`'s `@dmed/discovery` dependency is a local `file:../lib/js` link — both must be
built before `gateway/dist/index.js` will run.

## 3. Configure

```bash
cp config.json.example config.json
```

For a Pi that's only reachable on your LAN, the minimal config is enough:

```json
{
  "apiKey": "generate-a-long-random-string-here",
  "port": 4100
}
```

Generate the key with `openssl rand -hex 32`. See the [gateway README](../../gateway/README.md#exposing-this-to-the-wan-optional)
before opening this up beyond your LAN — you'll want `allowlist` and either `tls` or a
reverse proxy first (see step 5).

## 4. Run as a systemd service

```ini
# /etc/systemd/system/dmed-gateway.service
[Unit]
Description=DMED MCP Gateway
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=/home/pi/dmed-protocol/gateway
ExecStart=/usr/bin/node dist/index.js
Restart=on-failure
RestartSec=5
User=pi

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now dmed-gateway
sudo journalctl -u dmed-gateway -f    # watch it discover devices
```

You should see the same `[Gateway] + <device name>` lines as running it interactively.

## 5. Exposing it beyond your LAN

Pick one:

**A. Reverse proxy (recommended)** — run the gateway as plain HTTP on `localhost` and let
[Caddy](https://caddyserver.com/) terminate real, auto-renewing TLS in front of it:

```caddyfile
# /etc/caddy/Caddyfile
gateway.yourdomain.com {
    reverse_proxy localhost:4100
}
```

This also gets you HTTP/2, automatic cert renewal, and standard access logs for free.
Forward port 443 on your router to the Pi, and point a DNS A/AAAA record (or a dynamic-DNS
hostname, if your ISP doesn't give you a static IP) at your public address.

**B. Built-in TLS** — if you'd rather not run a second process, point the gateway
directly at a cert/key pair (see the `tls` config block in the
[gateway README](../../gateway/README.md#exposing-this-to-the-wan-optional)). You're
responsible for renewing the cert yourself in this path (e.g. `certbot` with a renewal
hook that just restarts `dmed-gateway`).

Either way, **set `allowlist` before doing this** — decide which devices and actions
should be reachable from outside your home network.

## 6. Verify

From another machine with the repo cloned (`@dmed/discovery` isn't published to npm yet —
run it straight from `lib/js`), check one of your own devices first, catching manifest
problems before they become the gateway's problem:

```bash
node lib/js/dist/cli.js http://<device-ip>:<port>
```

Then check the gateway itself:

```bash
curl https://gateway.yourdomain.com/.well-known/mcp/server-card.json
curl -X POST https://gateway.yourdomain.com/mcp \
  -H "Content-Type: application/json" -H "X-API-Key: <your-key>" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
```

## Security checklist

- [ ] `allowlist.devices` set to only the devices you actually want reachable remotely
- [ ] `allowlist.actions` narrows any device that should be read-only outside the LAN
- [ ] TLS in place (reverse proxy or built-in) before exposing beyond the LAN
- [ ] `apiKey` is a long random value, not something guessable, not committed to git
- [ ] `config.json` permissions restrict it to the service user (`chmod 600`)

## Troubleshooting

- **No devices discovered**: confirm the Pi and the device are on the same L2 segment;
  `avahi-browse -a` on the Pi should show `_dmed._tcp` entries if mDNS is reaching it at all.
- **`Config file not found`**: the systemd unit's `WorkingDirectory` must be `gateway/`,
  since `config.json` is resolved relative to it (or set `DMED_GATEWAY_CONFIG` to an
  absolute path).
- **`TLS cert file not found`**: paths in `config.json`'s `tls` block must be absolute and
  readable by the systemd service's `User`.

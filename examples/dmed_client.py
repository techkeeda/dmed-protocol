#!/usr/bin/env python3
"""
DMED Client — Minimal Reference Implementation
Usage: pip install zeroconf requests && python3 dmed_client.py
"""
import socket, json, time
import requests
from zeroconf import Zeroconf, ServiceBrowser, ServiceListener

class DMEDScanner(ServiceListener):
    def __init__(self):
        self.servers = {}

    def add_service(self, zc, stype, name):
        info = zc.get_service_info(stype, name)
        if not info: return
        host = socket.inet_ntoa(info.addresses[0])
        port = info.port
        props = {k.decode(): v.decode() for k, v in info.properties.items()}
        card_url = f"http://{host}:{port}{props.get('card', '/dmed/card')}"
        try:
            card = requests.get(card_url, timeout=5).json()
        except Exception as e:
            print(f"  [!] Card fetch failed: {e}"); return
        iid = props.get("id", "?")
        self.servers[iid] = {"name": props.get("nm", name), "card": card, "mcp_url": card["transports"][0]["url"]}
        tools = [t["name"] for t in card.get("capabilities", {}).get("tools", [])]
        print(f"\n[+] {props.get('nm', name)} ({iid})")
        print(f"    Tools: {tools}")
        print(f"    MCP:   {card['transports'][0]['url']}")

    def remove_service(self, zc, stype, name): print(f"\n[-] Gone: {name}")
    def update_service(self, zc, stype, name): pass

if __name__ == "__main__":
    print("[DMED] Scanning for servers... (Ctrl+C to stop)\n")
    scanner = DMEDScanner()
    zc = Zeroconf()
    ServiceBrowser(zc, "_dmed._tcp.local.", scanner)
    try:
        while True: time.sleep(1)
    except KeyboardInterrupt: pass
    finally: zc.close()

    if not scanner.servers:
        print("\nNo servers found."); exit()

    servers = list(scanner.servers.values())
    print("\n\n=== Discovered Servers ===")
    for i, s in enumerate(servers): print(f"  [{i}] {s['name']}")
    srv = servers[int(input("\nSelect [0]: ") or "0")]

    # Initialize MCP session
    requests.post(srv["mcp_url"], json={"jsonrpc":"2.0","id":0,"method":"initialize",
        "params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"DMED CLI","version":"0.2"}}})

    tools = srv["card"]["capabilities"]["tools"]
    print(f"\nTools on {srv['name']}:")
    for i, t in enumerate(tools): print(f"  [{i}] {t['name']} — {t.get('description','')}")
    tool = tools[int(input("\nSelect tool [0]: ") or "0")]

    args = input(f"Args for '{tool['name']}' (JSON, or empty): ").strip()
    args = json.loads(args) if args else {}

    resp = requests.post(srv["mcp_url"], json={"jsonrpc":"2.0","id":1,"method":"tools/call",
        "params":{"name": tool["name"], "arguments": args}}).json()
    print(f"\nResult: {json.dumps(resp.get('result', resp.get('error')), indent=2)}")

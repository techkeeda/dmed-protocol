"""
DMED v0.2 Example: Full lifecycle — Discover → Connect → Interact

Demonstrates:
1. Scanning for DMED endpoints on the network
2. Connecting to a discovered endpoint
3. Listing available actions
4. Sending actions/commands

Requires: pip install zeroconf requests
"""
import sys, time
sys.path.insert(0, "../../lib/python")
from dmed import DMEDScanner, DMEDClient

def main():
    print("🔍 Scanning for DMED endpoints...")
    scanner = DMEDScanner()
    endpoints = scanner.scan(timeout=5.0)

    if not endpoints:
        print("No endpoints found. Make sure a DMED server is running.")
        return

    print(f"\n✅ Found {len(endpoints)} endpoint(s):\n")
    for i, (iid, card) in enumerate(endpoints.items(), 1):
        print(f"  [{i}] {card.name} ({card.service_type})")
        print(f"      {card.description}")
        print(f"      Tools: {', '.join(t.name for t in card.tools)}\n")

    # Connect to first endpoint
    first_id = list(endpoints.keys())[0]
    card = endpoints[first_id]
    base_url = card.transports[0].url.rsplit("/", 1)[0]  # strip /mcp path

    print(f"🔗 Connecting to: {card.name}")
    client = DMEDClient(base_url)
    client.connect()

    # List actions
    print("\n📋 Available actions:")
    actions = client.list_actions()
    for a in actions:
        print(f"   • {a['name']} — {a['description']}")

    # Send an action
    print("\n⚡ Sending action: get_status")
    result = client.send_action("get_status")
    print(f"   Response: {result}")

    print("\n✅ Full lifecycle complete: Discover → Connect → Interact")

if __name__ == "__main__":
    main()

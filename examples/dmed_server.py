#!/usr/bin/env python3
"""
DMED Server — Minimal Reference Implementation
Usage: pip install zeroconf flask && python3 dmed_server.py
"""
import socket, datetime, json
from flask import Flask, jsonify, request
from zeroconf import Zeroconf, ServiceInfo

SERVER_NAME = "DMED Example Server"
INSTANCE_ID = "a1b2c3d4"
PORT = 8080

app = Flask(__name__)

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: s.connect(("8.8.8.8", 80)); return s.getsockname()[0]
    finally: s.close()

@app.route("/dmed/card")
def card():
    host = request.host.split(":")[0]
    return jsonify({
        "dmed_version": "0.1.0", "instance_id": INSTANCE_ID,
        "name": SERVER_NAME, "service_type": "tool_utility",
        "transports": [{"type": "http", "url": f"http://{host}:{PORT}/mcp", "priority": 1}],
        "auth": {"type": "none"},
        "capabilities": {"tools": [
            {"name": "echo", "description": "Echo back input text"},
            {"name": "add", "description": "Add two numbers"},
            {"name": "time", "description": "Get current server time"}
        ], "resources": [], "prompts": []}
    })

@app.route("/mcp", methods=["POST"])
def mcp():
    req = request.get_json(); method = req.get("method"); rid = req.get("id")
    if method == "initialize":
        return jsonify({"jsonrpc":"2.0","id":rid,"result":{"protocolVersion":"2025-03-26","capabilities":{"tools":{}},"serverInfo":{"name":SERVER_NAME,"version":"1.0.0"}}})
    if method == "notifications/initialized":
        return "", 204
    if method == "tools/list":
        return jsonify({"jsonrpc":"2.0","id":rid,"result":{"tools":[
            {"name":"echo","description":"Echo back input text","inputSchema":{"type":"object","properties":{"text":{"type":"string"}},"required":["text"]}},
            {"name":"add","description":"Add two numbers","inputSchema":{"type":"object","properties":{"a":{"type":"number"},"b":{"type":"number"}},"required":["a","b"]}},
            {"name":"time","description":"Get current server time","inputSchema":{"type":"object","properties":{}}}
        ]}})
    if method == "tools/call":
        name = req["params"]["name"]; args = req["params"].get("arguments", {})
        if name == "echo": result = args.get("text", "")
        elif name == "add": result = str(args.get("a",0) + args.get("b",0))
        elif name == "time": result = datetime.datetime.now().isoformat()
        else: return jsonify({"jsonrpc":"2.0","id":rid,"error":{"code":-32601,"message":f"Unknown tool: {name}"}})
        return jsonify({"jsonrpc":"2.0","id":rid,"result":{"content":[{"type":"text","text":result}]}})
    return jsonify({"jsonrpc":"2.0","id":rid,"error":{"code":-32601,"message":f"Unknown method: {method}"}})

if __name__ == "__main__":
    ip = get_local_ip()
    zc = Zeroconf()
    zc.register_service(ServiceInfo("_mcp-dmed._tcp.local.", f"{SERVER_NAME}._mcp-dmed._tcp.local.",
        addresses=[socket.inet_aton(ip)], port=PORT,
        properties={"v":"1","id":INSTANCE_ID,"st":"0a","fl":"4","nm":SERVER_NAME,"path":"/mcp","card":"/dmed/card"},
        server=f"{INSTANCE_ID}.local."))
    print(f"[DMED] ✓ {SERVER_NAME} at {ip}:{PORT}")
    print(f"[DMED]   Card: http://{ip}:{PORT}/dmed/card")
    print(f"[DMED]   MCP:  http://{ip}:{PORT}/mcp")
    try: app.run(host="0.0.0.0", port=PORT)
    finally: zc.unregister_all_services(); zc.close()

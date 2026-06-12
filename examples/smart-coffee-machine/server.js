import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import Bonjour from "bonjour-service";
import express from "express";
import { readFileSync } from "fs";
import { networkInterfaces } from "os";
import { z } from "zod";

const card = JSON.parse(readFileSync("./dmed-manifest.json", "utf-8"));
const PORT = 3100;

// --- Simulated machine state ---
let machineState = {
  water_level: 85,
  bean_level: 70,
  temperature: 92,
  is_brewing: false,
  last_drink: null,
};

// --- MCP Endpoint ---
const server = new McpServer({
  name: card.name,
  version: "1.0.0",
});

server.tool(
  "brew_coffee",
  "Brew a coffee with specified type and size",
  {
    drink_type: z.enum(["espresso", "americano", "latte", "cappuccino"]),
    size: z.enum(["small", "medium", "large"]),
  },
  async ({ drink_type, size }) => {
    if (machineState.water_level < 10) {
      return { content: [{ type: "text", text: "Error: Water level too low. Please refill." }] };
    }
    if (machineState.bean_level < 5) {
      return { content: [{ type: "text", text: "Error: Bean level too low. Please refill." }] };
    }
    machineState.is_brewing = true;
    machineState.water_level -= size === "large" ? 20 : size === "medium" ? 15 : 10;
    machineState.bean_level -= size === "large" ? 15 : size === "medium" ? 10 : 7;
    machineState.last_drink = `${size} ${drink_type}`;
    machineState.is_brewing = false;
    return { content: [{ type: "text", text: `☕ Brewing complete! Your ${size} ${drink_type} is ready.` }] };
  }
);

server.tool("get_status", "Get current machine status", {}, async () => {
  return {
    content: [{ type: "text", text: JSON.stringify({
      water_level: `${machineState.water_level}%`,
      bean_level: `${machineState.bean_level}%`,
      temperature: `${machineState.temperature}°C`,
      is_brewing: machineState.is_brewing,
      last_drink: machineState.last_drink || "None",
    }, null, 2) }],
  };
});

server.tool(
  "set_temperature",
  "Set brewing temperature (85-96°C)",
  { temperature: z.number().min(85).max(96) },
  async ({ temperature }) => {
    machineState.temperature = temperature;
    return { content: [{ type: "text", text: `Temperature set to ${temperature}°C` }] };
  }
);

// --- DMED Discovery ---
function getLocalIP() {
  const interfaces = networkInterfaces();
  for (const iface of Object.values(interfaces)) {
    for (const addr of iface) {
      if (addr.family === "IPv4" && !addr.internal) return addr.address;
    }
  }
  return "127.0.0.1";
}

const localIP = getLocalIP();

// Update card transport URL with actual IP
card.transports[0].url = `http://${localIP}:${PORT}/mcp`;

// Serve capability card over HTTP
const app = express();
app.get("/dmed/card", (req, res) => res.json(card));

// v0.2: Action endpoints
app.get("/dmed/actions", (req, res) => {
  res.json({ actions: card.capabilities.tools.map(t => ({
    name: t.name, description: t.description, params: t.inputSchema
  }))});
});

app.post("/dmed/action", express.json(), async (req, res) => {
  const { action, params } = req.body;
  if (!action) return res.status(400).json({ status: "error", message: "Missing 'action' field" });
  const tool = card.capabilities.tools.find(t => t.name === action);
  if (!tool) return res.status(404).json({ status: "error", message: `Unknown action: ${action}` });
  // Delegate to MCP tool handler (simplified for demo)
  try {
    let result;
    if (action === "brew_coffee") {
      const { drink_type, size } = params || {};
      machineState.water_level -= size === "large" ? 20 : size === "medium" ? 15 : 10;
      machineState.bean_level -= size === "large" ? 15 : size === "medium" ? 10 : 7;
      machineState.last_drink = `${size} ${drink_type}`;
      result = `☕ Your ${size} ${drink_type} is ready!`;
    } else if (action === "get_status") {
      result = JSON.stringify(machineState);
    } else if (action === "set_temperature") {
      machineState.temperature = params?.temperature || 92;
      result = `Temperature set to ${machineState.temperature}°C`;
    }
    res.json({ status: "ok", action, result: { text: result } });
  } catch (e) {
    res.status(500).json({ status: "error", action, message: e.message });
  }
});

app.listen(PORT, () => {
  console.error(`[DMED] Card:    http://${localIP}:${PORT}/dmed/card`);
  console.error(`[DMED] Actions: http://${localIP}:${PORT}/dmed/actions`);
  console.error(`[DMED] Invoke:  POST http://${localIP}:${PORT}/dmed/action`);
});

// Broadcast via mDNS (_dmed._tcp)
const bonjour = new Bonjour();
bonjour.publish({
  name: card.name,
  type: "dmed",
  port: PORT,
  txt: {
    v: "1",
    id: card.instance_id,
    st: "01",
    fl: "4",
    nm: card.name,
    card: "/dmed/card",
    path: "/mcp",
  },
});

console.error(`[DMED] ✓ Broadcasting "${card.name}" on _dmed._tcp port ${PORT}`);

// --- Start MCP transport ---
const transport = new StdioServerTransport();
await server.connect(transport);

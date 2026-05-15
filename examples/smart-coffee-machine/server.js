import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import Bonjour from "bonjour-service";
import express from "express";
import { readFileSync } from "fs";
import { networkInterfaces } from "os";
import { z } from "zod";

const manifest = JSON.parse(readFileSync("./dmed-manifest.json", "utf-8"));

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
  name: manifest.endpoint.name,
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

    return {
      content: [{ type: "text", text: `☕ Brewing complete! Your ${size} ${drink_type} is ready.` }],
    };
  }
);

server.tool("get_status", "Get current machine status", {}, async () => {
  return {
    content: [
      {
        type: "text",
        text: JSON.stringify(
          {
            water_level: `${machineState.water_level}%`,
            bean_level: `${machineState.bean_level}%`,
            temperature: `${machineState.temperature}°C`,
            is_brewing: machineState.is_brewing,
            last_drink: machineState.last_drink || "None",
          },
          null,
          2
        ),
      },
    ],
  };
});

server.tool(
  "set_temperature",
  "Set brewing temperature (85-96°C)",
  { temperature: z.number().min(85).max(96) },
  async ({ temperature }) => {
    machineState.temperature = temperature;
    return {
      content: [{ type: "text", text: `Temperature set to ${temperature}°C` }],
    };
  }
);

// --- DMED Discovery: mDNS Broadcasting ---
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
const port = manifest.transport.mdns.port;

// Serve manifest over HTTP so discoverers can fetch full details
const app = express();
app.get("/dmed-manifest.json", (req, res) => res.json(manifest));
app.listen(port, () => {
  console.error(`[DMED] Manifest served at http://${localIP}:${port}/dmed-manifest.json`);
});

// Broadcast via mDNS
const bonjour = new Bonjour();
bonjour.publish({
  name: manifest.endpoint.name,
  type: "dmed",
  port,
  txt: {
    dmed_version: manifest.dmed_version,
    name: manifest.endpoint.name,
    description: manifest.endpoint.description,
    category: manifest.endpoint.category,
    manifest_url: `http://${localIP}:${port}/dmed-manifest.json`,
  },
});

console.error(`[DMED] Broadcasting MCP endpoint "${manifest.endpoint.name}" on _dmed._tcp port ${port}`);

// --- Start MCP transport ---
const transport = new StdioServerTransport();
await server.connect(transport);

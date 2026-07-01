import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import bonjourService from "bonjour-service";
const Bonjour = bonjourService.default || bonjourService;
import express from "express";
import { readFileSync } from "fs";
import { networkInterfaces } from "os";
import { fileURLToPath } from "url";
import { z } from "zod";

const card = JSON.parse(readFileSync("./dmed-manifest.json", "utf-8"));
const PORT = 3100;

// --- Testable app factory ---
// Returns { app, state } — each call gets a fresh isolated state.
// Used by tests to get a clean instance without starting the server.
export function createApp() {
  const state = {
    water_level: 85,
    bean_level: 70,
    temperature: 92,
    is_brewing: false,
    last_drink: null,
  };

  const app = express();

  app.get("/dmed/card", (req, res) => res.json(card));

  app.get("/dmed/actions", (req, res) => {
    res.json({
      actions: card.capabilities.tools.map(t => ({
        name: t.name,
        description: t.description,
        params: t.inputSchema,
      })),
    });
  });

  app.post("/dmed/action", express.json(), async (req, res) => {
    const { action, params } = req.body;
    if (!action) {
      return res.status(400).json({ status: "error", message: "Missing 'action' field" });
    }
    const tool = card.capabilities.tools.find(t => t.name === action);
    if (!tool) {
      return res.status(404).json({ status: "error", message: `Unknown action: ${action}` });
    }

    try {
      let result;
      if (action === "brew_coffee") {
        const { drink_type, size } = params || {};
        if (state.water_level < 10) {
          result = "Error: Water level too low. Please refill.";
        } else if (state.bean_level < 5) {
          result = "Error: Bean level too low. Please refill.";
        } else {
          state.water_level -= size === "large" ? 20 : size === "medium" ? 15 : 10;
          state.bean_level -= size === "large" ? 15 : size === "medium" ? 10 : 7;
          state.last_drink = `${size} ${drink_type}`;
          result = `☕ Your ${size} ${drink_type} is ready!`;
        }
      } else if (action === "get_status") {
        result = JSON.stringify(state);
      } else if (action === "set_temperature") {
        state.temperature = params?.temperature || 92;
        result = `Temperature set to ${state.temperature}°C`;
      }
      res.json({ status: "ok", action, result: { text: result } });
    } catch (e) {
      res.status(500).json({ status: "error", action, message: e.message });
    }
  });

  return { app, state };
}

// --- Only runs when executed directly (not imported by tests) ---
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { app, state } = createApp();

  // MCP server — wired to the same state instance
  const mcpServer = new McpServer({ name: card.name, version: "1.0.0" });

  mcpServer.tool(
    "brew_coffee",
    "Brew a coffee with specified type and size",
    {
      drink_type: z.enum(["espresso", "americano", "latte", "cappuccino"]),
      size: z.enum(["small", "medium", "large"]),
    },
    async ({ drink_type, size }) => {
      if (state.water_level < 10) {
        return { content: [{ type: "text", text: "Error: Water level too low. Please refill." }] };
      }
      if (state.bean_level < 5) {
        return { content: [{ type: "text", text: "Error: Bean level too low. Please refill." }] };
      }
      state.is_brewing = true;
      state.water_level -= size === "large" ? 20 : size === "medium" ? 15 : 10;
      state.bean_level -= size === "large" ? 15 : size === "medium" ? 10 : 7;
      state.last_drink = `${size} ${drink_type}`;
      state.is_brewing = false;
      return { content: [{ type: "text", text: `☕ Brewing complete! Your ${size} ${drink_type} is ready.` }] };
    }
  );

  mcpServer.tool("get_status", "Get current machine status", {}, async () => {
    return {
      content: [{ type: "text", text: JSON.stringify({
        water_level: `${state.water_level}%`,
        bean_level: `${state.bean_level}%`,
        temperature: `${state.temperature}°C`,
        is_brewing: state.is_brewing,
        last_drink: state.last_drink || "None",
      }, null, 2) }],
    };
  });

  mcpServer.tool(
    "set_temperature",
    "Set brewing temperature (85-96°C)",
    { temperature: z.number().min(85).max(96) },
    async ({ temperature }) => {
      state.temperature = temperature;
      return { content: [{ type: "text", text: `Temperature set to ${temperature}°C` }] };
    }
  );

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
  card.transports[0].url = `http://${localIP}:${PORT}/mcp`;

  app.listen(PORT, () => {
    console.error(`[DMED] Card:    http://${localIP}:${PORT}/dmed/card`);
    console.error(`[DMED] Actions: http://${localIP}:${PORT}/dmed/actions`);
    console.error(`[DMED] Invoke:  POST http://${localIP}:${PORT}/dmed/action`);
  });

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

  const transport = new StdioServerTransport();
  await mcpServer.connect(transport);
}

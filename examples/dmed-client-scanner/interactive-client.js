import Bonjour from "bonjour-service";
import { createInterface } from "readline";

const bonjour = new Bonjour();
const discovered = new Map();
let serviceIndex = 0;

const rl = createInterface({ input: process.stdin, output: process.stdout });

console.log("📱 DMED Interactive Client");
console.log("━".repeat(60));
console.log("Scanning for MCP endpoints...\n");

// Discover MCP endpoints
bonjour.find({ type: "dmed" }, (service) => {
  const txt = service.txt || {};
  const id = `${service.name}:${service.port}`;
  if (discovered.has(id)) return;

  serviceIndex++;
  discovered.set(id, { ...service, index: serviceIndex });

  console.log(`  [${serviceIndex}] ${txt.icon || "•"} ${txt.name || service.name}`);
  console.log(`      ${txt.description || ""}`);
  console.log(`      ${service.host}:${service.port}\n`);
});

// After 5 seconds, show menu
setTimeout(() => {
  if (discovered.size === 0) {
    console.log("No MCP endpoints found. Make sure a DMED-enabled endpoint is running on the network.");
    process.exit(0);
  }

  console.log("━".repeat(60));
  showMenu();
}, 5000);

function showMenu() {
  rl.question("\n🎯 Enter endpoint number to connect (or 'q' to quit): ", async (answer) => {
    if (answer === "q") {
      bonjour.destroy();
      rl.close();
      process.exit(0);
    }

    const num = parseInt(answer);
    const service = [...discovered.values()].find((s) => s.index === num);

    if (!service) {
      console.log("Invalid selection.");
      showMenu();
      return;
    }

    const txt = service.txt || {};
    console.log(`\n🔗 Connecting to MCP endpoint: ${txt.name || service.name}`);
    console.log(`   Fetching manifest from: ${txt.manifest_url}`);

    try {
      const res = await fetch(txt.manifest_url);
      const manifest = await res.json();

      console.log(`\n📋 Available tools:`);
      for (const tool of manifest.capabilities.tools) {
        console.log(`   • ${tool.name} — ${tool.description}`);
      }

      console.log(`\n💡 To interact, connect this MCP endpoint to your AI client.`);
      console.log(`   Host: ${service.host}:${service.port}`);
    } catch (e) {
      console.log(`   ❌ Could not fetch manifest: ${e.message}`);
    }

    showMenu();
  });
}

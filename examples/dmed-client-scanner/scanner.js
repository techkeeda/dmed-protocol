import Bonjour from "bonjour-service";

const bonjour = new Bonjour();
const discovered = new Map();

console.log("🔍 DMED Scanner — Searching for MCP endpoints...\n");
console.log("━".repeat(60));

const browser = bonjour.find({ type: "dmed" }, (service) => {
  const txt = service.txt || {};
  const id = `${service.name}:${service.port}`;

  if (discovered.has(id)) return;
  discovered.set(id, service);

  console.log(`\n✅ Found MCP endpoint: ${txt.nm || txt.name || service.name}`);
  console.log(`   📝 ${txt.description || "No description"}`);
  console.log(`   📂 Service Type: ${txt.st || "unknown"}`);
  console.log(`   🌐 Host: ${service.host}:${service.port}`);
  console.log(`   📄 Card: http://${service.host}:${service.port}${txt.card || "/dmed/card"}`);
  console.log(`   🔖 DMED Version: ${txt.v || "unknown"}`);
  console.log("━".repeat(60));
});

// Periodic summary
setInterval(() => {
  console.log(`\n📡 ${discovered.size} MCP endpoint(s) discovered. Still scanning... (Ctrl+C to stop)`);
}, 10000);

// Graceful shutdown
process.on("SIGINT", () => {
  console.log(`\n\n🛑 Scan complete. Found ${discovered.size} MCP endpoint(s):\n`);
  for (const [, service] of discovered) {
    const txt = service.txt || {};
    console.log(`   ${txt.icon || "•"} ${txt.name || service.name} → ${service.host}:${service.port}`);
  }
  console.log("");
  browser.stop();
  bonjour.destroy();
  process.exit(0);
});

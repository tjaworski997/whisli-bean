// Fake device with demo data: serves the real panel build so scripts/screens.sh can photograph it.
// Usage: node scripts/panel-mock.mjs <en|pl> <port> <ui/dist>
import http from "node:http"; import crypto from "node:crypto"; import fs from "node:fs"
const [locale, port, dist] = process.argv.slice(2)
const pl = locale === "pl"
const NEC = "NEC-4BBA-E0", LIGHT = "NEC-4BBA-A0", AMP = "RC5-10-0C", CINEMA = "NEC-4BBA-50", NAS = "IR-7C21D09E"
const name = pl ? "Salon" : "Living room"
const uptimeStart = Date.now() - 3 * 3600e3
const up = () => Date.now() - uptimeStart
const hooks = [
  { id: 1, name: pl ? "Światło w salonie" : "Living room light", method: "POST", url: "http://homeassistant.local:8123/api/webhook/living-room-light", headers: "", body: "json", bodyTemplate: '{\n  "button": "{button}",\n  "gesture": "{gesture}",\n  "from": "{controller}"\n}', insecure: false },
  { id: 2, name: pl ? "Scena kino" : "Cinema scene", method: "GET", url: "http://node-red.local:1880/scene/cinema", headers: "", body: "none", bodyTemplate: "", insecure: false },
]
const servers = [{ id: 1, name: "NAS", mac: "00:11:32:AA:BB:CC", broadcast: "192.168.1.255" }, { id: 2, name: pl ? "Komputer w biurze" : "Office PC", mac: "D8:BB:C1:12:34:56", broadcast: "" }]
const buttons = [
  { code: NEC, name: "Play", note: "", gestures: false, actions: { short: { mqtt: true } } },
  { code: LIGHT, name: pl ? "Światło" : "Light", note: pl ? "Krótko włącza, długo przyciemnia" : "Short switches, long dims", gestures: true, actions: { short: { mqtt: true, webhook: 1 }, long: { mqtt: true, once: false } } },
  { code: AMP, name: pl ? "Wzmacniacz" : "Amplifier", note: "", gestures: false, actions: { short: { mqtt: true } } },
  { code: CINEMA, name: pl ? "Tryb kino" : "Cinema mode", note: "", gestures: true, actions: { short: { webhook: 2 }, long: { wol: 1 } } },
  { code: NAS, name: pl ? "Obudź NAS" : "Wake NAS", note: "", gestures: false, actions: { short: { wol: 1 } } },
]
function necDurations(addr, cmd) {
  const d = [9000, 4500]; const v = (addr) | (cmd << 16) | ((~cmd & 255) << 24)
  for (let i = 0; i < 32; i++) d.push(560, (v >>> i) & 1 ? 1690 : 560)
  return [...d, 560].map(x => x + Math.round(Math.random() * 30 - 15))
}
const frames = () => [
  { id: 3, atMs: up() - 8000, firstLevel: 0, edges: 67, protocol: "nec", remote: true, code: NEC, name: "Play", durationsUs: necDurations(0x4bba, 0xe0) },
  { id: 2, atMs: up() - 41000, firstLevel: 0, edges: 67, protocol: "nec", remote: true, code: LIGHT, name: buttons[1].name, durationsUs: necDurations(0x4bba, 0xa0) },
  { id: 1, atMs: up() - 96000, firstLevel: 0, edges: 67, protocol: "nec", remote: true, code: "NEC-00FF-16", durationsUs: necDurations(0xff, 0x16) },
].reverse()
let learn = { state: "idle", distinct: 0, unreadable: 0 }
const state = () => ({ name, hostname: "whisli-salon", version: "0.001.202611061706", uptime: Math.floor(up() / 1000), uptimeMs: up(), problems: [],
  ir: { pin: 4, level: 1, lastFrameId: 3, press: { count: 12, code: NEC, atMs: up() - 8000, gesture: "short", name: "Play" }, learn },
  actions: { run: null }, mqtt: "connected", rssi: -56, ip: "192.168.1.50", heap: 148000, clock: Math.floor(Date.now() / 1000) })
const system = () => ({ version: "0.001.202611061706", uptime: Math.floor(up() / 1000), resetReason: "powerOn", chip: "ESP32-C3", chipRevision: 4, cpuMhz: 160, sdk: "v5.5", heap: 148000, heapTotal: 280000, minHeap: 121000, maxBlock: 90000, flash: 4194304, firmwareSize: 1732320, firmwareSpace: 1966080, loopPerSecond: 1200, loopAverageUs: 820, loopMaxUs: 9100 })
const logs = () => [
  [500, "info", "system", "boot", "0.001.202611061706 · usb"], [4000, "info", "network", "wifiConnected", "192.168.1.50 · -56 dBm"], [6000, "info", "mqtt", "mqttConnected", ""],
  [up() - 600000, "info", "ir", "learned", NEC], [up() - 590000, "info", "ir", "buttonSaved", "Play"], [up() - 41000, "info", "action", "actionOk", buttons[1].name],
].map(([uptimeMs, level, category, code, detail], i) => ({ id: i + 1, uptimeMs, level, category, code, detail }))
const routes = {
  "/api/v1/auth": () => ({ signedIn: true, defaultPassword: false, name, hostname: "whisli-salon", ip: "192.168.1.50" }),
  "/api/v1/state": state,
  "/api/v1/buttons": () => ({ buttons, runs: [
    { code: NEC, gesture: "short", count: 12, atMs: up() - 8000, steps: [{ type: "mqtt", status: "ok" }] },
    { code: LIGHT, gesture: "short", count: 5, atMs: up() - 41000, steps: [{ type: "mqtt", status: "ok" }, { type: "webhook", status: "ok", detail: "200" }] } ] }),
  "/api/v1/hooks": () => ({ hooks: hooks.map(({ id, name, method, url }) => ({ id, name, method, url })), limits: { count: 20, name: 64, url: 512, headers: 1024, body: 2048 } }),
  "/api/v1/servers": () => ({ servers, limit: 16 }),
  "/api/v1/ir": () => ({ uptimeMs: up(), lastId: 3, frames: frames() }),
  "/api/v1/mqtt": () => ({ enabled: true, host: "homeassistant.local", port: 1883, user: "mqtt", hasPassword: true, baseTopic: "", defaultBaseTopic: "whisli-bean/a1b2", discovery: true, discoveryPrefix: "homeassistant", state: "connected", error: "" }),
  "/api/v1/led": () => ({ mode: "received", brightness: 30, alerts: true }),
  "/api/v1/wifi": () => ({ wifi: { ssid: "Home WiFi", connected: true, ip: "192.168.1.50", rssi: -56, channel: 6, hostname: "whisli-salon" }, ap: { ssid: "Whisli-Bean-A1B2", active: false, defaultPassword: false } }),
  "/api/v1/system": system,
  "/api/v1/diagnostics": () => ({ system: system(), ir: { pin: 4, level: 1, edges: 8412, frames: 126, framesWithCode: 124 },
    network: { ssid: "Home WiFi", connected: true, hostname: "whisli-salon", mac: "AA:BB:CC:DD:A1:B2", disconnects: 0, ap: false, bssid: "AA:BB:CC:00:11:22", channel: 6, rssi: -56, ip: "192.168.1.50", gateway: "192.168.1.1", subnet: "255.255.255.0", dns: "192.168.1.1", connectedFor: 10800 },
    mqtt: { state: "connected", error: "", host: "homeassistant.local", port: 1883, base: "whisli-bean/a1b2", published: 318, connects: 1 } }),
  "/api/v1/logs": () => ({ sessionId: 1, capacity: 120, hasMore: false, entries: logs() }),
}
const server = http.createServer((req, res) => {
  const url = new URL(req.url, "http://x")
  const json = o => { res.writeHead(200, { "Content-Type": "application/json" }); res.end(JSON.stringify(o)) }
  if (url.pathname === "/api/v1/ir/learn") { if (req.method === "POST") learn = { state: "waiting", distinct: 0, unreadable: 0, remainingMs: 25000 }; return json(learn) }
  if (url.pathname === "/api/v1/ir/learn/cancel") { learn = { state: "idle", distinct: 0, unreadable: 0 }; return json({}) }
  if (url.pathname === "/api/v1/hooks/detail") return json(hooks.find(h => h.id === +url.searchParams.get("id")))
  if (req.method === "POST") return json({})
  if (routes[url.pathname]) return json(routes[url.pathname]())
  const html = fs.readFileSync(dist + "/index.html", "utf8").replace("<head>", `<head><script>localStorage.setItem("whisli-bean.locale","${locale}");localStorage.setItem("whisli-bean.theme",location.search.includes("dark")?"dark":"light");if(location.search.includes("expand")){const t=setInterval(()=>{const b=document.querySelector("[aria-expanded]");if(b){clearInterval(t);setTimeout(()=>b.click(),800)}},50)}</script>`)
  res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" }); res.end(html)
})
server.on("upgrade", (req, socket) => {
  const accept = crypto.createHash("sha1").update(req.headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest("base64")
  socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`)
  const send = () => { const b = Buffer.from(JSON.stringify(state())); const h = b.length < 126 ? Buffer.from([0x81, b.length]) : Buffer.from([0x81, 126, b.length >> 8, b.length & 255]); socket.write(Buffer.concat([h, b])) }
  send(); const t = setInterval(send, 1000); socket.on("close", () => clearInterval(t)); socket.on("error", () => clearInterval(t))
})
server.listen(+port, "127.0.0.1")

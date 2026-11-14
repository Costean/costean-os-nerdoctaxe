// Mock NerdOctAxe (shufps v1.1.2 API shape) serving clearly-labelled DEMO data. Never talks to a real device.
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path'; import zlib from 'node:zlib'; import crypto from 'node:crypto';
const PORT = +process.env.PORT || 8099, ROOT = path.resolve(process.env.DIST || 'dist/axe-os');
const FW = process.env.FW_VERSION || '1.2.0-costean.1';
export const writes = [];
const S = { deviceModel: 'NerdOCTAXE-Gamma', asicModel: 'BM1370', version: FW, otp: false, apActive: false, can: { hasExtension: false, enabled: false },
  frequency: 700, coreVoltage: 1150, vrFrequency: 500000, defaultFrequency: 700, defaultCoreVoltage: 1150, defaultVrFrequency: 500000, ecoFrequency: 450, ecoCoreVoltage: 1100, absMinCoreVoltage: 1050, absMaxCoreVoltage: 1400,
  frequencyOptions: [500, 525, 550, 575, 600, 625, 650, 675, 700, 725, 750, 775, 800], voltageOptions: [1100, 1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1210, 1220, 1230, 1240, 1250, 1260],
  poolMode: 1, poolBalance: 70, stratumKeep: 0, jobInterval: 1200, stratumDifficulty: 1000,
  pools: [{ url: 'demo-pool.example', port: 4333, user: 'bc1qDEMOdemoDEMO.costean', enonceSubscribe: false, tls: true, protocol: 0, sv2AuthorityPubkey: '', sv2ChannelType: 0, sv2RequireAuth: false, coinbaseVerifyMode: 1, coinbaseMaxFee: 2, coinbaseVerifyForce: false },
          { url: 'backup-pool.example', port: 3333, user: 'bc1qDEMOdemoDEMO.costean', enonceSubscribe: false, tls: false, protocol: 0, sv2AuthorityPubkey: '', sv2ChannelType: 0, sv2RequireAuth: false, coinbaseVerifyMode: 0, coinbaseMaxFee: 2, coinbaseVerifyForce: false }],
  fans: [{ label: 'Fan 1', mode: 2, manualSpeed: 100, overheatTemp: 70, pid: { targetTemp: 55, p: 6, i: 0.1, d: 10 } }, { label: 'Fan 2', mode: 3, manualSpeed: 100, overheatTemp: 70, pid: { targetTemp: 55, p: 6, i: 0.1, d: 10 } }],
  invertFanPolarity: 1, pidUseMax: true, hostname: 'nerdoctaxe', ssid: 'DEMO-WIFI', mempoolCustom: false, mempoolUrl: '', flipScreen: 1, invertScreen: 0, autoScreenOff: 0 };
const t0 = Date.now(); let sb = 2.1e5; const sessBest = () => { if (Math.random() < 0.18) sb = Math.max(sb, 10 ** (4.5 + Math.random() * 2.2)); return BLOCK ? 1.31e14 : sb; }; const rnd = (a, b) => a + Math.random() * (b - a);
const chipBase = [1428, 1419, 1436, 1411, 1425, 1417, 1433, 1422]; const BLOCK = process.env.SCENARIO === 'block', FAILOVER = process.env.SCENARIO === 'failover', THROTTLE = process.env.SCENARIO === 'throttle';
const acc0 = () => 1060 + Math.floor((Date.now() - t0) / 1900), acc1 = () => 455 + Math.floor((Date.now() - t0) / 4400);
function splitPools(hr) {
  const w = [S.poolBalance, 100 - S.poolBalance], up = [true, !FAILOVER];
  const eff = up[1] ? w : [100, 0];
  const mk = (i, conn, acc, rej, diff, ping) => ({ connected: conn, poolDifficulty: conn ? diff : 0, networkDifficulty: 1.27e14, accepted: acc, rejected: rej, pingRtt: conn ? ping : 0, pingLoss: 0, bestDiff: i ? 9.1e7 : 3.37e8, encrypted: !!S.pools[i].tls,
    host: S.pools[i].url, port: S.pools[i].port, user: S.pools[i].user, splitWeight: w[i], splitEffective: eff[i], splitHealthy: up[i], jobShare: eff[i] / 100 + (conn ? rnd(-0.004, 0.004) : 0),
    assignedHashrate: hr * eff[i] / 100, creditedHashrate: conn ? hr * eff[i] / 100 * rnd(0.9, 1.1) : 0 });
  return [mk(0, true, acc0(), 1, 1000, 23.4), mk(1, up[1], acc1(), 1, 512, 41.7)];
}
function costean() {
  const now = Math.floor(Date.now() / 1000), local = now - 5 * 3600;
  const min = Math.floor((local % 86400) / 60);
  const night = min >= 22 * 60 || min < 7 * 60;
  const prof = THROTTLE ? 'stock' : CFG.profile;
  const P = { eco: { mhz: 600, mv: 1100 }, stock: { mhz: 700, mv: 1150 }, turbo: { mhz: 750, mv: 1180 }, custom: { mhz: S.frequency, mv: S.coreVoltage } };
  const applied = THROTTLE ? { mhz: 650, mv: 1150 } : P[prof];
  const nextMin = night ? 7 * 60 : 22 * 60; let next = local - (local % 86400) + nextMin * 60; if (next <= local) next += 86400;
  return { status: { activeProfile: prof, profileSource: 'user', target: P[prof], applied,
      governor: THROTTLE ? { state: 'throttled', reason: 'chip 68.5C over 66C', throttleSteps: 2, tuneSteps: 0, bestStableSteps: 0, errorPct: 0.41, rejectPct: 0.13, mining: true }
                         : { state: 'hold', reason: 'headroom ok', throttleSteps: 0, tuneSteps: 0, bestStableSteps: 0, errorPct: 0.38, rejectPct: 0.13, mining: true },
      effectiveLimits: { chipTemp: CFG.limits.chipTemp, vrTemp: CFG.limits.vrTemp, power: CFG.limits.power }, presets: P, envelope: { minMhz: 400, maxMhz: 800, stepMhz: 25 },
      schedule: { clockValid: true, profileWindow: -1, lcdWindow: night && CFG.schedule.enabled ? 0 : -1, lcd: night ? 'dim' : 'normal', profile: 'none', localTime: local, dst: false, nextChangeLocal: next },
      zones: ['UTC', 'America/St_Johns', 'America/Halifax', 'America/New_York', 'America/Toronto', 'America/Chicago', 'America/Denver', 'America/Phoenix', 'America/Los_Angeles', 'America/Vancouver', 'Europe/London', 'Europe/Berlin', 'Europe/Helsinki'],
      failover: FAILOVER, poolMode: S.poolMode, maxPools: 2, designMaxPools: 4 }, config: CFG };
}
const CFG = { profile: 'custom', limits: { chipTemp: 66, vrTemp: 78, power: 200 }, autotune: { enabled: false, maxSteps: 2, maxErrorPct: 1, maxRejectPct: 1, holdMin: 15 },
  schedule: { enabled: true, tz: 'America/New_York', windows: [
    { on: true, days: 127, start: 1320, end: 420, profile: 'none', lcd: 'dim', name: 'Night screen' },
    { on: true, days: 62, start: 0, end: 360, profile: 'eco', lcd: 'off', name: 'Quiet night' },
    { on: true, days: 62, start: 540, end: 1020, profile: 'eco', lcd: 'normal', name: 'Workday eco' },
    { on: false, days: 65, start: 600, end: 1080, profile: 'stock', lcd: 'normal', name: 'Weekend' }] } };
function rollup() {
  const slot = 900, end = Math.floor(Date.now() / 1000 / slot) * slot, n = 192, start = end - (n - 1) * slot;
  const H = { slotSec: slot, start, ghs: [], watts: [], chipTemp: [], vrTemp: [], jth: [], flags: [] };
  for (let i = 0; i < n; i++) {
    const t = start + i * slot, lm = Math.floor(((t - 5 * 3600) % 86400) / 60), wd = new Date((t - 5 * 3600) * 1000).getUTCDay();
    const gap = i >= 60 && i < 63; if (gap) { for (const k of ['ghs', 'watts', 'chipTemp', 'vrTemp', 'jth']) H[k].push(null); H.flags.push(0); continue; }
    const eco = (lm < 360 && wd >= 1 && wd <= 5) || (lm >= 540 && lm < 1020 && wd >= 1 && wd <= 5);
    const thr = i >= 120 && i < 126;
    const base = eco ? 9700 : thr ? 10600 : 11350, g = Math.round(base + Math.sin(i / 7) * 90 + Math.sin(i * 1.7) * 40);
    const w = +(g / 1000 * (eco ? 14.9 : 16.3) + Math.sin(i / 5) * 1.5).toFixed(1);
    H.ghs.push(g); H.watts.push(w); H.chipTemp.push(+((eco ? 57.5 : thr ? 67.5 : 62.5) + Math.sin(i / 9) * 1.2).toFixed(1)); H.vrTemp.push(+((eco ? 60 : 64.5) + Math.sin(i / 11)).toFixed(1));
    H.jth.push(Math.round(w / (g / 1000))); H.flags.push((thr ? 1 : 0) | (i >= 150 && i < 154 ? 2 : 0) | (eco || lm >= 1320 || lm < 420 ? 4 : 0));
  }
  return H;
}
function dash(q) {
  const up = (Date.now() - t0) / 1000 + 3600 * 10 + 24 * 60;
  const chips = chipBase.map((b, i) => ({ hashRate: b + rnd(-12, 12), errorHashRate: i === 5 ? rnd(13, 15) : rnd(0.8, 2.6) }));
  const hr = chips.reduce((a, c) => a + c.hashRate, 0);
  const temps = [52.1, 53.4, 55.8, 56.2, 57.9, 59.7, 54.6, 51.9].map(t => +(t + rnd(-0.4, 0.4)).toFixed(1));
  const d = { demo: true, system: { uptime: Math.floor(up), shutdown: false, boardError: 0, overheatTemp: 70 },
    performance: { hashRateTimestamp: Date.now(), hashRate: hr, hashRate1m: hr * 0.995, hashRate10m: hr * 1.002, hashRate1h: hr * 0.998, hashRate1d: hr * 0.99, bestDiff: BLOCK ? 1.31e14 : 1.89e9, bestSessionDiff: sessBest(), sharesAccepted: 1512 + Math.floor((Date.now() - t0) / 1300), sharesRejected: 2, frequency: S.frequency, asicCount: 8, smallCoreCount: 2040, chips },
    power: { watts: +(hr / 1000 * 16.2 + rnd(-0.5, 0.5)).toFixed(1), min: 30, max: 200, voltage: 12.04, voltageMin: 11.5, voltageMax: 12.8, currentA: 12.6, currentAMin: 0, currentAMax: 25, coreVoltageActual: +(1.147 + rnd(-0.001, 0.001)).toFixed(3) },
    thermal: { asicTemp: Math.max(...temps), vrTemp: +(63.4 + rnd(-0.3, 0.3)).toFixed(1), vrTempInt: +(69.1 + rnd(-0.3, 0.3)).toFixed(1), asicTemps: temps, fans: [{ speed: 58, rpm: 3920 }] },
    stratum: { poolMode: S.poolMode, activePoolMode: S.poolMode, usingFallback: false, splitFailover: FAILOVER, totalBestDiff: BLOCK ? 1.31e14 : 1.89e9, pools: splitPools(hr) },
    can: { hasExtension: false, enabled: false }, coinbase: { blockHeaders: [{ pool: 0, blockHeight: 924187, networkDifficulty: 1.27e14, scriptSig: '', coinbaseValueTotalSatoshis: 315871243, coinbaseValueUserSatoshis: 315871243, verificationOk: true, verificationFailCount: 0, verificationCheckCount: 41 }], pools: [] } };
  if (q.get('ts')) { const span = +q.get('historySpan') || 3600e3, n = Math.floor(span / 30000), H = { hashrate_1m: [], hashrate_10m: [], hashrate_1h: [], hashrate_1d: [], timestamps: [], vregTemp: [], asicTemp: [] };
    for (let i = 0; i < n; i++) { const w = Math.sin(i / 25) * 120 + rnd(-60, 60); H.timestamps.push(i * 30000); H.hashrate_1m.push(Math.round((11400 + w) * 100)); H.hashrate_10m.push(Math.round((11420 + w * 0.4) * 100)); H.hashrate_1h.push(Math.round((11410 + w * 0.1) * 100)); H.hashrate_1d.push(1140000); H.asicTemp.push(Math.round((62 + Math.sin(i / 30) * 1.5 + rnd(-0.3, 0.3)) * 100)); H.vregTemp.push(Math.round((66 + Math.sin(i / 40)) * 100)); }
    d.history = { ...H, timestampBase: +q.get('ts'), hasMore: false }; }
  return d;
}
const SYS = () => ({ deviceModel: S.deviceModel, asicModel: S.asicModel, version: FW, uptimeSeconds: Math.floor((Date.now() - t0) / 1000) + 37440, lastResetReason: 'Power on (DEMO)', network: { hostname: S.hostname, ssid: S.ssid, macAddr: 'DE:MO:00:00:00:01', ipAddr: '192.0.2.10', wifiStatus: 'Connected (DEMO)', wifiRSSI: -58 }, memory: { freeHeap: 6100000, freeHeapInt: 92000 } });
let influx = { enabled: false, url: 'http://192.0.2.20', port: 8086, bucket: 'miners', org: 'home', prefix: 'nerdoctaxe' }, alert = { hasWebhook: false, watchdogEnable: true, blockFoundEnable: true, bestDiffEnable: false, coinbaseVerifyEnable: true, showBlockFoundScreen: true };
const json = (res, o) => { res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' }); res.end(JSON.stringify(o)); };
const body = req => new Promise(r => { let b = []; req.on('data', c => b.push(c)); req.on('end', () => r(Buffer.concat(b))); });
const TYPES = { '.html': 'text/html', '.js': 'application/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.json': 'application/json' };
const server = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://x'), p = u.pathname, m = req.method;
  if (m === 'OPTIONS') { res.writeHead(204, { 'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Methods': '*', 'Access-Control-Allow-Headers': '*' }); return res.end(); }
  if (p === '/__writes') return json(res, writes);
  if (p.startsWith('/api/')) {
    if (m !== 'GET') { const b = await body(req); const rec = { m, p, len: b.length, body: b.length < 4096 ? b.toString() : undefined }; writes.push(rec);
      if (p === '/api/v2/settings') { const o = JSON.parse(b); for (const k of Object.keys(o)) if (k !== 'pools' && k !== 'fans') S[k] = o[k]; }
      res.writeHead(200, { 'Access-Control-Allow-Origin': '*' }); return res.end(''); }
    if (p === '/api/v2/dashboard') return json(res, dash(u.searchParams));
    if (p === '/api/v2/settings') return json(res, S);
    if (p === '/api/costean') return json(res, costean());
    if (p === '/api/costean/history') return json(res, rollup());
    if (p === '/api/v2/system') return json(res, SYS());
    if (p === '/api/system/info') return json(res, { foundBlocks: BLOCK ? 1 : 0, frequency: S.frequency, coreVoltage: S.coreVoltage, version: FW });
    if (p === '/api/v2/identify') return json(res, { deviceModel: S.deviceModel, otp: false, apActive: false, coreDumpDownload: false, can: S.can, defaultTheme: 'dark' });
    if (p === '/api/v2/influx') return json(res, influx);
    if (p === '/api/v2/alert') return json(res, alert);
    if (p === '/api/v2/otp/status') return json(res, { enabled: false });
    if (p === '/api/v2/wifi/scan') return json(res, { networks: [{ ssid: 'DEMO-WIFI', rssi: -52, authmode: 3 }, { ssid: 'DEMO-Guest', rssi: -71, authmode: 0 }] });
    if (p === '/api/system/OTA/github') return json(res, { pending: false, running: false, step: 'idle', progress: 0 });
    res.writeHead(404); return res.end('not found');
  }
  let f = path.join(ROOT, p === '/' ? 'index.html' : p);
  if (!f.startsWith(ROOT)) { res.writeHead(403); return res.end(); }
  if (fs.existsSync(f + '.gz')) { res.writeHead(200, { 'Content-Type': TYPES[path.extname(f)] || 'text/plain', 'Content-Encoding': 'gzip' }); return res.end(fs.readFileSync(f + '.gz')); }
  if (fs.existsSync(f) && fs.statSync(f).isFile()) { res.writeHead(200, { 'Content-Type': TYPES[path.extname(f)] || 'text/plain' }); return res.end(fs.readFileSync(f)); }
  res.writeHead(302, { Location: '/' }); res.end();
});
// minimal websocket for /api/ws log stream
server.on('upgrade', (req, sock) => {
  if (!req.url.startsWith('/api/ws')) return sock.destroy();
  const key = crypto.createHash('sha1').update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  sock.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${key}\r\n\r\n`);
  const send = s => { const b = Buffer.from(s); const h = b.length < 126 ? Buffer.from([0x81, b.length]) : Buffer.from([0x81, 126, b.length >> 8, b.length & 255]); sock.write(Buffer.concat([h, b])); };
  const L = ['I (1203) w5500: probe: VERSIONR=0x04 -> present [DEMO]', 'I (1300) stratum: DEMO pool connected (TLS)', 'I (5000) asic: DEMO chip 6 nonce rate low', 'W (6000) power: DEMO VR temp 66.2C', 'I (7000) stratum: DEMO share accepted diff 1532'];
  let i = 0; const iv = setInterval(() => send(L[i++ % L.length]), 400); sock.on('close', () => clearInterval(iv)); sock.on('error', () => clearInterval(iv));
});
server.listen(PORT, () => console.log(`DEMO mock NerdOctAxe on http://127.0.0.1:${PORT} serving ${ROOT}`));

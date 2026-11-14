// Thin client for the shufps v1.1.2 REST API. base = '' (this miner) or another miner's origin.
let base = '';
export const setBase = b => { base = b || ''; };
export const getBase = () => base;
const otpHeaders = totp => { const h = {}; if (totp) h['X-TOTP'] = totp; const s = sessionStorage.getItem('costean.otpSession'); if (s) h['X-OTP-Session'] = s; return h; };
async function req(method, path, body, { totp, raw, timeout = 8000, headers = {} } = {}) {
  const ctl = new AbortController(); const t = setTimeout(() => ctl.abort(), timeout);
  try {
    const r = await fetch(base + path, { method, signal: ctl.signal, headers: { ...(body && !raw ? { 'Content-Type': 'application/json' } : {}), ...(method !== 'GET' ? otpHeaders(totp) : {}), ...headers }, body: raw ? body : body ? JSON.stringify(body) : undefined });
    if (!r.ok) { const txt = await r.text().catch(() => ''); const e = new Error(`${r.status} ${txt || r.statusText}`); e.status = r.status; throw e; }
    const ct = r.headers.get('content-type') || '';
    return ct.includes('json') ? r.json() : r.text();
  } finally { clearTimeout(t); }
}
export const api = {
  dashboard: (hist) => req('GET', '/api/v2/dashboard' + (hist ? `?ts=${hist.ts}&cur=${hist.cur}&historySpan=${hist.span}&limit=${hist.limit || 1000}` : '')),
  settings: () => req('GET', '/api/v2/settings'),
  patchSettings: (p, totp) => req('PATCH', '/api/v2/settings', p, { totp }),
  system: () => req('GET', '/api/v2/system'),
  identify: () => req('GET', '/api/v2/identify'),
  asic: () => req('GET', '/api/system/asic'),
  info: () => req('GET', '/api/system/info'),
  wifiScan: () => req('GET', '/api/v2/wifi/scan', null, { timeout: 20000 }),
  influx: () => req('GET', '/api/v2/influx'),
  patchInflux: (p, totp) => req('PATCH', '/api/v2/influx', p, { totp }),
  alert: () => req('GET', '/api/v2/alert'),
  patchAlert: (p, totp) => req('PATCH', '/api/v2/alert', p, { totp }),
  testAlert: (totp) => req('POST', '/api/v2/alert/test', {}, { totp }),
  restart: (totp) => req('POST', '/api/system/restart', null, { totp }),
  shutdown: (totp) => req('POST', '/api/system/shutdown', null, { totp }),
  resetStats: (allTime, totp) => req('POST', '/api/system/reset-stats' + (allTime ? '?allTime=1' : ''), null, { totp }),
  githubOta: (url, keepConfig, totp) => req('POST', '/api/system/OTA/github', { url, keep_config: keepConfig }, { totp }),
  githubOtaStatus: () => req('GET', '/api/system/OTA/github'),
  otpStatus: () => req('GET', '/api/v2/otp/status'),
  otpEnroll: () => req('POST', '/api/v2/otp', {}),
  otpSet: (enabled, totp) => req('PATCH', '/api/v2/otp', { enabled }, { totp }),
  otpSession: async (totp) => { const r = await req('POST', '/api/v2/otp/session', {}, { totp }); if (r?.token) sessionStorage.setItem('costean.otpSession', r.token); return r; },
  coredumpUrl: () => base + '/api/system/coredump',
  costean: () => req('GET', '/api/costean'),
  costeanHistory: () => req('GET', '/api/costean/history'),
  patchCostean: (p, totp) => req('PATCH', '/api/costean', p, { totp }),
  wwwVersion: () => fetch(base + '/version.json', { cache: 'no-store' }).then(r => r.ok ? r.json() : null).catch(() => null),
};
/** Raw OTA upload with progress via XHR. kind: 'firmware' | 'www' */
export function upload(kind, file, onProgress, totp) {
  return new Promise((resolve, reject) => {
    const x = new XMLHttpRequest(); x.open('POST', base + (kind === 'www' ? '/api/system/OTAWWW' : '/api/system/OTA'));
    x.setRequestHeader('Content-Type', 'application/octet-stream');
    for (const [k, v] of Object.entries(otpHeaders(totp))) x.setRequestHeader(k, v);
    x.upload.onprogress = e => e.lengthComputable && onProgress?.(e.loaded / e.total);
    x.onload = () => x.status < 300 ? resolve(x.responseText) : reject(new Error(x.status + ' ' + x.responseText));
    x.onerror = () => reject(new Error('Upload failed (network)'));
    x.send(file);
  });
}
export function logSocket(onLine, onState) {
  const u = (base || location.origin).replace(/^http/, 'ws') + '/api/ws';
  const ws = new WebSocket(u); ws.onopen = () => onState?.('open'); ws.onclose = () => onState?.('closed'); ws.onerror = () => onState?.('error');
  ws.onmessage = e => String(e.data).split('\n').filter(Boolean).forEach(onLine);
  return ws;
}
/** Validate a firmware image before upload: ESP image magic 0xE9; www.bin is a raw SPIFFS image (no magic). */
export async function inspectImage(file, kind) {
  const head = new Uint8Array(await file.slice(0, 256).arrayBuffer());
  if (kind === 'firmware') {
    if (head[0] !== 0xE9) return { ok: false, why: 'Not an ESP32 app image (missing 0xE9 magic). Did you pick www.bin?' };
    if (file.size > 4 * 1024 * 1024) return { ok: false, why: 'Larger than the 4 MB OTA partition.' };
    // esp_app_desc_t sits at offset 0x20 after the 24-byte image + 8-byte segment header; version at +16 (32 bytes)
    const v = new TextDecoder().decode(head.slice(48, 80)).replace(/\0.*$/s, '');
    return { ok: true, version: v };
  }
  if (head[0] === 0xE9) return { ok: false, why: 'This looks like a firmware app image, not www.bin.' };
  if (file.size > 3 * 1024 * 1024) return { ok: false, why: 'Larger than the 3 MB www partition.' };
  return { ok: true };
}

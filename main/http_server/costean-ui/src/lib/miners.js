// Multi-miner quick switcher; list lives only in this browser's localStorage.
const KEY = 'costean.miners';
export function normalize(host) {
  host = (host || '').trim().replace(/\/+$/, ''); if (!host) return '';
  if (!/^https?:\/\//.test(host)) host = 'http://' + host;
  try { const u = new URL(host); return u.origin; } catch { return ''; }
}
export function load(ls = globalThis.localStorage) { try { return JSON.parse(ls.getItem(KEY)) || []; } catch { return []; } }
export function save(list, ls = globalThis.localStorage) { ls.setItem(KEY, JSON.stringify(list)); }
export function addMiner(list, host, name) { const o = normalize(host); if (!o || list.some(m => m.origin === o)) return list; return [...list, { origin: o, name: name || o.replace(/^https?:\/\//, '') }]; }
export function removeMiner(list, origin) { return list.filter(m => m.origin !== origin); }

// Accept a peer only when its dashboard reply looks like a real miner (upstream v1.1.2 swarm hardening).
export function isMinerDash(d) {
  const hr = d?.performance?.hashRate;
  return !!d && typeof d === 'object' && Number.isFinite(hr) && hr >= 0;
}

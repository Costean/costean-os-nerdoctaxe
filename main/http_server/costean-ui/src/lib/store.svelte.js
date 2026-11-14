import { api, setBase } from './api.js';
import { analyze, zAnomaly } from './insights.js';
import { Ring, decodeHistory } from './history.js';
import { efficiency } from './format.js';
import { trend, labRecord } from './trend.js';
import { ask } from './confirm.svelte.js';
import { isAboveStock } from './insights.js';
import { ABOVE_STOCK_PHRASE } from './commands.js';

export const app = $state({
  page: (location.hash.slice(2).split('?')[0] || 'dashboard'), online: false, lastOk: 0, dash: null, settings: null, system: null, identify: null, wwwVersion: null,
  theme: 'dark', base: localStorage.getItem('costean.base') || '', toast: null, paletteOpen: false,
  notify: localStorage.getItem('costean.notify') === '1', fwHistory: [], tick: 0, insight: null, demo: false, costean: null, rollup: null,
});
export const ring = new Ring(4320);
let timer, histTimer, lastNotify = {};

export function toast(msg, kind = 'ok') { app.toast = { msg, kind, id: Date.now() }; setTimeout(() => { if (app.toast && Date.now() - app.toast.id > 3500) app.toast = null; }, 3600); }
export function setTheme(t) { app.theme = t; localStorage.setItem('costean.theme', t); document.documentElement.dataset.theme = t; }
export function go(p) { location.hash = '#/' + p; app.page = p; }

function notify(key, title, body) {
  if (!app.notify || typeof Notification === 'undefined' || Notification.permission !== 'granted') return;
  if (lastNotify[key] && Date.now() - lastNotify[key] < 10 * 60e3) return; lastNotify[key] = Date.now();
  try { new Notification(title, { body, icon: 'icon.svg', tag: key }); } catch { }
}
export async function enableNotifications() {
  if (typeof Notification === 'undefined') return toast('Notifications not supported in this browser', 'err');
  const p = await Notification.requestPermission(); app.notify = p === 'granted'; localStorage.setItem('costean.notify', app.notify ? '1' : '0');
  toast(app.notify ? 'Notifications on: temperature, faults, offline' : 'Notification permission denied', app.notify ? 'ok' : 'err');
}

async function poll() {
  try {
    const d = await api.dashboard(); app.dash = d; app.demo = !!d.demo;
    api.costean().then(c => app.costean = c).catch(() => { });
    if (!app.online) { app.online = true; loadStatic(); }
    app.lastOk = Date.now();
    const w = d.power?.watts, gh = d.performance?.hashRate;
    const pt = { t: Date.now(), w, gh, eff: efficiency(w, gh), asic: d.thermal?.asicTemp, vr: d.thermal?.vrTemp, fan: d.thermal?.fans?.[0]?.rpm,
      chr: (d.performance?.chips || []).map(c => c.hashRate || 0), cerr: (d.performance?.chips || []).map(c => c.errorHashRate || 0), ct: d.thermal?.asicTemps || [],
      acc: d.performance?.sharesAccepted, rej: d.performance?.sharesRejected, f: d.performance?.frequency, mv: d.power?.coreVoltageActual,
      vri: d.thermal?.vrTempInt, bd: d.performance?.bestDiff, bsd: d.performance?.bestSessionDiff, pd: (d.stratum?.pools || []).find(x => x.connected)?.poolDifficulty, nd: (d.stratum?.pools || []).find(x => x.connected)?.networkDifficulty };
    if (d.demo && !ring.a.length) demoSeed(pt);   // DEMO mock only: pre-fill 10 min of preview history
    ring.push(pt); trend.add(pt); labRecord(pt);
    app.insight = analyze(d, app.settings);
    const z = zAnomaly(ring.a.slice(-60).map(p => p.gh || 0));
    if (z && z.score < 0) app.insight.items.push({ level: 'warn', key: 'hrDrop', title: 'Sudden hashrate drop', why: `Latest ${z.last.toFixed(0)} GH/s vs ${z.mean.toFixed(0)} GH/s recent average.` });
    for (const it of app.insight.items) if (it.level === 'crit') notify(it.key, 'Costean OS: ' + it.title, it.why);
  } catch (e) {
    if (app.online && Date.now() - app.lastOk > 15000) { app.online = false; app.insight = analyze(null); notify('offline', 'Costean OS offline', 'No response from ' + (app.base || location.host)); }
  }
  app.tick++;
}
export async function loadStatic() {
  const [s, sys, id, wv] = await Promise.allSettled([api.settings(), api.system(), api.identify(), api.wwwVersion()]);
  if (s.value) app.settings = s.value; if (sys.value) app.system = sys.value; if (id.value) app.identify = id.value; app.wwwVersion = wv.value || null;
}
export async function loadHistory(span = 3 * 3600e3) {
  const now = Date.now();
  try { const d = await api.dashboard({ ts: now - span, cur: now, span }); app.fwHistory = decodeHistory(d.history, now); } catch { }
}
export async function loadRollup() { try { app.rollup = await api.costeanHistory(); } catch { } }
export function start() {
  setBase(app.base); clearInterval(timer); clearInterval(histTimer);
  app.dash = null; app.settings = null; app.online = false; app.fwHistory = []; ring.a = [];
  poll(); timer = setInterval(poll, 5000); loadHistory(); loadRollup(); histTimer = setInterval(() => { loadHistory(); loadRollup(); }, 60000);
}
export function switchMiner(origin) { app.base = origin || ''; localStorage.setItem('costean.base', app.base); start(); }

/** The single write path for settings. */
export async function applySettings(patch, label) {
  const s = app.settings; const above = isAboveStock(patch.frequency ?? null, patch.coreVoltage ?? null, s);
  const detail = JSON.stringify(redact(patch), null, 2);
  const r = await ask(label || 'Apply settings', { detail, danger: above, phrase: above ? ABOVE_STOCK_PHRASE : '', otp: !!app.identify?.otp });
  if (!r.ok) return false;
  try { await api.patchSettings(patch, r.totp); toast('Saved. Some changes apply after restart.'); await loadStatic(); return true; }
  catch (e) { toast('Save failed: ' + e.message, 'err'); return false; }
}
/** Write path for Costean OS profile / limits / auto-tune / schedule settings (PATCH /api/costean). */
export async function applyCostean(patch, label, { danger = false, phrase = '', warn = '' } = {}) {
  const r = await ask(label || 'Apply Costean settings', { detail: JSON.stringify(patch, null, 2), danger, phrase, warn, otp: !!app.identify?.otp });
  if (!r.ok) return false;
  try { await api.patchCostean(patch, r.totp); toast('Saved. Applies within 5 seconds.'); app.costean = await api.costean(); return true; }
  catch (e) { toast('Not saved: ' + e.message, 'err'); return false; }
}
export async function runAction(label, fn, { danger = false } = {}) {
  const r = await ask(label, { danger, otp: !!app.identify?.otp }); if (!r.ok) return false;
  try { await fn(r.totp); toast(label + ': done'); return true; } catch (e) { toast(label + ' failed: ' + e.message, 'err'); return false; }
}
export const redact = o => JSON.parse(JSON.stringify(o, (k, v) => /pass|token/i.test(k) && v ? '••••••' : v));

// Only ever runs against the DEMO mock server (payload has demo:true); real firmware never sets it.
function demoSeed(pt) {
  let sb = 3e5;
  for (let i = 120; i > 0; i--) {
    const w = Math.sin(i / 9) * 35 + Math.sin(i / 3.1) * 14;
    if (i % 17 === 0) sb = Math.max(sb, 10 ** (5 + (120 - i) / 120 * 1.4 + Math.random() * .4));
    ring.push({ ...pt, t: pt.t - i * 5000, gh: (pt.gh || 8100) + w, asic: (pt.asic || 59) + Math.sin(i / 14) * 0.8, vr: (pt.vr || 63) + Math.sin(i / 20) * 0.6, vri: (pt.vri || 69) + Math.sin(i / 18) * 0.7,
      bsd: i % 5 ? sb * (0.2 + Math.random() * 0.5) : sb, pd: pt.pd || 1000 });
  }
}

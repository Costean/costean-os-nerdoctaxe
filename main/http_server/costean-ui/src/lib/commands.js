// Command palette: natural-ish commands -> planned API actions. Pure; executed after confirm.
import { isAboveStock } from './insights.js';
export const NAV = [
  ['dashboard', 'Dashboard'], ['tuning', 'Profiles & Limits'], ['split', 'Pool Split'], ['schedules', 'Schedules'], ['history', 'History'], ['mining', 'Mining & Tuning'], ['pools', 'Pools & Stratum'], ['thermal', 'Thermal & Safe Mode'], ['block', 'Block Found'],
  ['swarm', 'Swarm'], ['influx', 'InfluxDB & Alerts'], ['network', 'System & Network'], ['diagnostics', 'Diagnostics & Logs'], ['update', 'Update & Recovery'],
];
const num = s => { const m = s.match(/(-?\d+(?:\.\d+)?)/); return m ? +m[1] : null; };

/** returns {kind:'nav'|'write'|'action'|'ui', label, ...} or null */
export function parse(input, s) {
  const q = input.trim().toLowerCase(); if (!q) return null;
  for (const [id, label] of NAV) if (q === id || q === label.toLowerCase() || q === 'go ' + id || q === 'open ' + id) return { kind: 'nav', label: 'Open ' + label, to: id };
  let m;
  if ((m = q.match(/^(?:set )?fans? (?:to )?(\d{1,3})\s*%?$/))) { const v = Math.min(100, Math.max(0, +m[1])); return write(`Set fan to manual ${v}%`, { fans: (s?.fans || [{}]).map(() => ({ mode: 0, manualSpeed: v })) }, s); }
  if (/^(?:set )?fans? (?:to )?auto/.test(q)) return write('Set fans to automatic (PID)', { fans: (s?.fans || [{}]).map(() => ({ mode: 2 })) }, s);
  if ((m = q.match(/^(?:set )?(?:freq|frequency) (?:to )?(\d+)/))) return write(`Set ASIC frequency ${+m[1]} MHz`, { frequency: +m[1] }, s);
  if ((m = q.match(/^(?:set )?(?:volt|voltage|core ?voltage) (?:to )?(\d+)/))) return write(`Set core voltage ${+m[1]} mV`, { coreVoltage: +m[1] }, s);
  if ((m = q.match(/^(?:set )?(?:target ?temp|pid ?temp) (?:to )?(\d+)/))) return write(`Set fan PID target ${+m[1]}°C`, { fans: (s?.fans || [{}]).map(() => ({ pid: { targetTemp: +m[1] } })) }, s);
  if (/^(stock|default) ?(mode|settings|tune)?$/.test(q) && s) return write(`Apply stock ${s.defaultFrequency} MHz / ${s.defaultCoreVoltage} mV`, { frequency: s.defaultFrequency, coreVoltage: s.defaultCoreVoltage }, s);
  if (/^safe ?(mode|start)?$/.test(q)) return write('Apply SAFE 700 MHz / 1150 mV', { frequency: 700, coreVoltage: 1150 }, s);
  if (/^eco ?(mode)?$/.test(q) && s && s.ecoFrequency) return write(`Apply eco ${s.ecoFrequency} MHz / ${s.ecoCoreVoltage} mV`, { frequency: s.ecoFrequency, coreVoltage: s.ecoCoreVoltage }, s);
  if ((m = q.match(/^hostname (.+)$/))) return write(`Set hostname to "${input.trim().slice(9)}"`, { hostname: input.trim().slice(9) }, s);
  if (/^(flip|rotate) ?screen/.test(q)) return write('Toggle screen flip', { flipScreen: !s?.flipScreen }, s);
  if (/^restart|^reboot/.test(q)) return { kind: 'action', label: 'Restart miner', action: 'restart' };
  if (/^shut ?down/.test(q)) return { kind: 'action', label: 'Shut down ASICs', action: 'shutdown', danger: true };
  if (/^reset stats/.test(q)) return { kind: 'action', label: 'Reset session statistics', action: 'resetStats' };
  if (/^(light|dark|toggle) ?(theme|mode)?$/.test(q)) return { kind: 'ui', label: 'Toggle light/dark theme', action: 'theme' };
  if (/^(backup|export)/.test(q)) return { kind: 'ui', label: 'Download settings backup (JSON)', action: 'backup' };
  if (/^notif/.test(q)) return { kind: 'ui', label: 'Enable browser notifications', action: 'notify' };
  for (const [id, label] of NAV) if (label.toLowerCase().includes(q) || id.startsWith(q)) return { kind: 'nav', label: 'Open ' + label, to: id };
  return null;
}
function write(label, patch, s) {
  const above = isAboveStock(patch.frequency ?? null, patch.coreVoltage ?? null, s);
  const outOfRange = s && patch.coreVoltage != null && ((s.absMaxCoreVoltage && patch.coreVoltage > s.absMaxCoreVoltage) || (s.absMinCoreVoltage && patch.coreVoltage < s.absMinCoreVoltage));
  return { kind: 'write', label, patch, aboveStock: !!above, blocked: outOfRange ? `Voltage outside firmware limits ${s.absMinCoreVoltage}–${s.absMaxCoreVoltage} mV` : null };
}
export const ABOVE_STOCK_PHRASE = 'ABOVE STOCK';
export const suggestions = ['fans 70%', 'fans auto', 'eco', 'stock', 'frequency 600', 'restart', 'backup', 'light theme', 'notifications', 'safe', 'block', 'thermal'];

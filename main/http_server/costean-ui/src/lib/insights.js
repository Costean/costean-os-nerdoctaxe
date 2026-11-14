// Local, rule-based health score + anomaly detection. No cloud.
import { efficiency } from './format.js';
export const LIMITS = { asicWarn: 65, asicCrit: 70, vrWarn: 80, vrCrit: 95, rejectWarn: 1.0, chipDevWarn: 0.25, chipErrWarn: 0.02, pingWarn: 150 };

/** dash = /api/v2/dashboard payload, settings = /api/v2/settings (optional) */
export function analyze(dash, settings) {
  const out = []; let score = 100;
  if (!dash) return { score: 0, grade: 'offline', items: [{ level: 'crit', key: 'offline', title: 'Miner offline', why: 'No response from the miner API.' }] };
  const t = dash.thermal || {}, p = dash.performance || {}, pw = dash.power || {}, sys = dash.system || {}, st = dash.stratum || {};
  const add = (level, key, title, why, penalty) => { out.push({ level, key, title, why }); score -= penalty; };
  if (sys.boardError) add('crit', 'boardError', 'Board fault active', `Firmware reports error code ${sys.boardError}. Mining is protected until it clears.`, 50);
  if (sys.shutdown) add('crit', 'shutdown', 'ASICs shut down', 'Power management has shut the ASICs down (overheat or manual).', 40);
  const at = t.asicTemp, vr = t.vrTemp;
  if (at >= LIMITS.asicCrit) add('crit', 'asicTemp', `ASIC temperature ${at.toFixed(1)}°C`, `Above ${LIMITS.asicCrit}°C. Raise fan speed or lower frequency.`, 25);
  else if (at >= LIMITS.asicWarn) add('warn', 'asicTemp', `ASIC temperature ${at.toFixed(1)}°C`, `Above ${LIMITS.asicWarn}°C comfort range.`, 10);
  if (vr >= LIMITS.vrCrit) add('crit', 'vrTemp', `Regulator temperature ${vr.toFixed(1)}°C`, `Above ${LIMITS.vrCrit}°C. Improve airflow over the VRM.`, 25);
  else if (vr >= LIMITS.vrWarn) add('warn', 'vrTemp', `Regulator temperature ${vr.toFixed(1)}°C`, `Above ${LIMITS.vrWarn}°C.`, 10);
  const acc = p.sharesAccepted || 0, rej = p.sharesRejected || 0;
  const rr = acc + rej > 50 ? rej / (acc + rej) * 100 : 0;
  if (rr > LIMITS.rejectWarn) add('warn', 'rejects', `Reject rate ${rr.toFixed(2)}%`, 'Higher than 1%. Check pool latency or stale work.', 10);
  // per-chip
  const chips = p.chips || [];
  if (chips.length && !sys.shutdown) {
    const hr = chips.map(c => c.hashRate || 0); const mean = hr.reduce((a, b) => a + b, 0) / hr.length;
    hr.forEach((h, i) => { if (mean > 0 && h < mean * (1 - LIMITS.chipDevWarn)) add('warn', 'chip' + i, `ASIC #${i + 1} low hashrate`, `${h.toFixed(0)} GH/s vs ${mean.toFixed(0)} GH/s average (${((1 - h / mean) * 100).toFixed(0)}% below).`, 6); });
    chips.forEach((c, i) => { const tot = (c.hashRate || 0) + (c.errorHashRate || 0); if (tot > 0 && c.errorHashRate / tot > LIMITS.chipErrWarn) add('warn', 'chipErr' + i, `ASIC #${i + 1} error rate ${(c.errorHashRate / tot * 100).toFixed(1)}%`, 'Hardware errors above 2%. Voltage may be too low for this frequency.', 5); });
  }
  const temps = t.asicTemps || [];
  if (temps.length > 1) { const mx = Math.max(...temps), mn = Math.min(...temps); if (mx - mn > 12) add('info', 'tempSpread', `Chip temperature spread ${(mx - mn).toFixed(1)}°C`, 'Uneven cooling or heatsink contact.', 3); }
  const pools = st.pools || [];
  if (pools.length && !pools.some(x => x.connected)) add('crit', 'pool', 'No pool connected', 'Check pool URL, port, TLS and network.', 30);
  if (st.usingFallback) add('warn', 'fallback', 'Running on fallback pool', 'Primary pool is unreachable.', 8);
  pools.forEach((x, i) => { if (x.connected && x.pingRtt > LIMITS.pingWarn) add('info', 'ping' + i, `Pool ${i + 1} ping ${x.pingRtt.toFixed(0)} ms`, 'High latency increases stale shares.', 2); });
  if (pw.voltage && pw.voltageMin && pw.voltage < pw.voltageMin) add('warn', 'vin', `Input voltage low ${pw.voltage.toFixed(2)} V`, 'PSU may be undersized or cable too thin.', 10);
  if (settings) {
    const above = isAboveStock(settings.frequency, settings.coreVoltage, settings);
    if (above) add('info', 'aboveStock', 'Running above stock', `${settings.frequency} MHz / ${settings.coreVoltage} mV vs stock ${settings.defaultFrequency} MHz / ${settings.defaultCoreVoltage} mV.`, 0);
  }
  const eff = efficiency(pw.watts, p.hashRate);
  score = Math.max(0, Math.min(100, Math.round(score)));
  const grade = score >= 90 ? 'excellent' : score >= 75 ? 'good' : score >= 50 ? 'fair' : 'poor';
  return { score, grade, efficiency: eff, items: out };
}

export function isAboveStock(freq, mv, s) {
  if (!s) return false;
  return (freq != null && s.defaultFrequency && freq > s.defaultFrequency) || (mv != null && s.defaultCoreVoltage && mv > s.defaultCoreVoltage);
}

/** Simple z-score anomaly on a numeric series (latest point vs window) */
export function zAnomaly(series, z = 3) {
  if (!series || series.length < 10) return null;
  const last = series[series.length - 1], win = series.slice(0, -1);
  const m = win.reduce((a, b) => a + b, 0) / win.length;
  const sd = Math.sqrt(win.reduce((a, b) => a + (b - m) ** 2, 0) / win.length);
  if (sd === 0) return null;
  const s = (last - m) / sd;
  return Math.abs(s) >= z ? { score: s, mean: m, last } : null;
}

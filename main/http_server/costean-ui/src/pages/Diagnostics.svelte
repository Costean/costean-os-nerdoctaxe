<script>
  import { app } from '../lib/store.svelte.js';
  import { fmtUptime, fmtBytes, BOARD_ERRORS } from '../lib/format.js';
  import Logs from './Logs.svelte';
  let s = $derived(app.system || {}), d = $derived(app.dash), ins = $derived(app.insight);
</script>
<h1>Diagnostics & Logs</h1>
<section class="grid g3">
  <div class="card c-ok" data-testid="health"><h2>Health score</h2><div class="big">{ins?.score ?? '—'}<span class="mute small"> / 100 · {ins?.grade || ''}</span></div>
    {#each ins?.items || [] as it}<div class="small {it.level === 'crit' ? 'crit' : it.level === 'warn' ? 'warn' : 'mute'}">• {it.title} — {it.why}</div>{:else}<div class="mute small">No issues detected.</div>{/each}</div>
  <div class="card c-cyan"><h2>Device</h2><table><tbody>
    <tr><td class="mute">Model</td><td>{s.deviceModel || '—'} · {s.asicModel || ''}</td></tr><tr><td class="mute">Firmware</td><td class="mono">{s.version || '—'}</td></tr>
    <tr><td class="mute">Uptime</td><td>{fmtUptime(s.uptimeSeconds)}</td></tr><tr><td class="mute">Last reset</td><td>{s.lastResetReason || '—'}</td></tr>
    <tr><td class="mute">Board error</td><td>{d?.system?.boardError ? BOARD_ERRORS[d.system.boardError] : 'none'}</td></tr></tbody></table></div>
  <div class="card c-blue"><h2>Memory & link</h2><table><tbody>
    <tr><td class="mute">Free PSRAM</td><td>{s.memory ? fmtBytes(s.memory.freeHeap) : '—'}</td></tr><tr><td class="mute">Free internal</td><td>{s.memory ? fmtBytes(s.memory.freeHeapInt) : '—'}</td></tr>
    <tr><td class="mute">Link</td><td>{s.network?.wifiStatus || '—'}</td></tr><tr><td class="mute">RSSI</td><td>{s.network?.wifiRSSI ?? '—'} dBm</td></tr></tbody></table></div>
</section>
<Logs />

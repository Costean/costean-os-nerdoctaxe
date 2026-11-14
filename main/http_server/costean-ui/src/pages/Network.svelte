<script>
  import { app, applySettings, toast } from '../lib/store.svelte.js';
  import { api } from '../lib/api.js';
  import { rssiLabel } from '../lib/format.js';
  let hostname = $state(''), ssid = $state(''), pass = $state(''), nets = $state(null), scanning = $state(false), init = false;
  $effect(() => { if (app.settings && !init) { init = true; hostname = app.settings.hostname; ssid = app.settings.ssid; } });
  let n = $derived(app.system?.network || {});
  let ethLines = $state([]);
  async function scan() { scanning = true; try { nets = (await api.wifiScan()).networks.sort((a, b) => b.rssi - a.rssi); } catch (e) { toast('Scan failed: ' + e.message, 'err'); } scanning = false; }
  async function save() { const p = {}; if (hostname !== app.settings.hostname) p.hostname = hostname; if (ssid !== app.settings.ssid) p.ssid = ssid; if (pass) p.wifiPass = pass; if (Object.keys(p).length && await applySettings(p, 'Save network settings (applies after restart)')) pass = ''; }
  let usingEth = $derived(n.wifiStatus && /eth/i.test(n.wifiStatus));
</script>
<h1>System & Network</h1><div class="mute">Wi-Fi, Ethernet (W5500), hostname.</div>
<div class="grid g2">
  <div class="card"><h2>Status</h2><table data-testid="net-status"><tbody>
    <tr><td class="mute">IP address</td><td class="mono">{n.ipAddr || '—'}</td></tr><tr><td class="mute">Hostname</td><td>{n.hostname || '—'}</td></tr>
    <tr><td class="mute">MAC</td><td class="mono">{n.macAddr || '—'}</td></tr><tr><td class="mute">Link</td><td>{n.wifiStatus || '—'}</td></tr>
    <tr><td class="mute">Wi-Fi SSID</td><td>{n.ssid || '—'}</td></tr><tr><td class="mute">Wi-Fi signal</td><td>{n.wifiRSSI ?? '—'} dBm · {rssiLabel(n.wifiRSSI)}</td></tr>
    <tr><td class="mute">Setup AP</td><td>{app.settings?.apActive ? 'active' : 'off'}</td></tr></tbody></table></div>
  <div class="card fm" data-testid="ethernet"><h2>Ethernet (W5500)</h2>
    <div class="row"><span class="pill"><span class="dot {usingEth ? 'ok' : ''}"></span>{usingEth ? 'Ethernet link in use' : 'Wi-Fi in use / no Ethernet link reported'}</span></div>
    <p class="mute small">Firmware v1.1.2 detects the W5500 Ethernet HAT automatically at boot (SPI probe) and prefers the wired link when it has an IP; the miner's screen shows a network icon for the active link. On NerdOctAxe boards with the temperature mux fitted, the Ethernet pins are not available. v1.1.2 exposes <b>no Ethernet settings or status fields</b> over the REST API (DHCP only), so there is nothing to configure here. Boot messages from the <span class="mono">w5500</span> driver appear in Live logs.</p>
    <a href="#/logs" onclick={() => sessionStorage.setItem('costean.logFilter', 'w5500')}>Show w5500 log lines →</a>
  </div>
  <div class="card fm"><h2>Wi-Fi & hostname</h2>
    <label class="f"><span>Hostname</span><input bind:value={hostname} /></label>
    <div class="row" style="align-items:end"><label class="f sp"><span>SSID</span><input bind:value={ssid} list="nets" /></label><button onclick={scan} disabled={scanning}>{scanning ? 'Scanning…' : 'Scan'}</button></div>
    <datalist id="nets">{#each nets || [] as x}<option value={x.ssid}></option>{/each}</datalist>
    {#if nets}<div class="nets">{#each nets as x}<button onclick={() => ssid = x.ssid}>{x.ssid || '(hidden)'} <span class="mute small">{x.rssi} dBm {x.authmode ? '🔒' : ''}</span></button>{/each}</div>{/if}
    <label class="f"><span>Wi-Fi password (leave blank to keep)</span><input type="password" bind:value={pass} autocomplete="new-password" /></label>
    <div class="row"><span class="mute small">Applies after restart.</span><span class="sp"></span><button class="pri" onclick={save}>Review & save</button></div></div>
  {#if app.settings?.can?.hasExtension}<div class="card fm"><h2>CAN swarm extension</h2><label class="chk"><input type="checkbox" checked={app.settings.can.enabled} onchange={ev => applySettings({ canMaster: ev.currentTarget.checked }, 'Toggle CAN master')} /> Enable CAN master</label></div>{/if}
</div>
<style>.fm{display:grid;gap:10px;align-content:start}.nets{display:flex;flex-wrap:wrap;gap:6px;max-height:160px;overflow:auto}.nets button{padding:4px 10px;font-size:12px}</style>

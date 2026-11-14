<script>
  import { app, runAction, toast } from '../lib/store.svelte.js';
  import { ask } from '../lib/confirm.svelte.js';
  import { api } from '../lib/api.js';
  import { enableNotifications } from '../lib/store.svelte.js';
  let inf = $state(null), al = $state(null), webhook = $state(''), token = $state('');
  $effect(() => { if (app.online && !inf) { api.influx().then(v => inf = v).catch(() => inf = {}); api.alert().then(v => al = v).catch(() => al = {}); } });
  async function save(kind) {
    const body = kind === 'influx' ? { ...inf, ...(token ? { token } : {}) } : { ...al, ...(webhook ? { webhookUrl: webhook } : {}) };
    delete body.hasWebhook; if (kind === 'influx' && !token) delete body.token;
    const r = await ask(kind === 'influx' ? 'Save InfluxDB settings' : 'Save alert settings', { detail: JSON.stringify({ ...body, token: body.token ? '••••' : undefined, webhookUrl: body.webhookUrl ? '••••' : undefined }, null, 2), otp: !!app.identify?.otp });
    if (!r.ok) return;
    try { kind === 'influx' ? await api.patchInflux(body, r.totp) : await api.patchAlert(body, r.totp); toast('Saved'); token = ''; webhook = ''; } catch (e) { toast('Save failed: ' + e.message, 'err'); }
  }
</script>
<h1>InfluxDB & Alerts</h1>
<div class="grid g2">
  <div class="card fm"><h2>Browser notifications</h2><p class="mute small">Local notifications from this tab for faults, overheating and the miner going offline. Nothing leaves your network.</p>
    <div class="row"><span class="pill"><span class="dot {app.notify ? 'ok' : ''}"></span>{app.notify ? 'On' : 'Off'}</span><button onclick={enableNotifications}>Enable</button></div></div>
  {#if al}<div class="card fm"><h2>Discord / webhook alerts (firmware)</h2>
    <label class="f"><span>Webhook URL {al.hasWebhook ? '(set — leave blank to keep)' : ''}</span><input type="password" bind:value={webhook} placeholder="https://discord.com/api/webhooks/…" /></label>
    <label class="chk"><input type="checkbox" bind:checked={al.watchdogEnable} /> Watchdog / reboot alerts</label>
    <label class="chk"><input type="checkbox" bind:checked={al.blockFoundEnable} /> Block found</label>
    <label class="chk"><input type="checkbox" bind:checked={al.bestDiffEnable} /> New best difficulty</label>
    <label class="chk"><input type="checkbox" bind:checked={al.coinbaseVerifyEnable} /> Coinbase verification failures</label>
    <label class="chk"><input type="checkbox" bind:checked={al.showBlockFoundScreen} /> Show block-found screen on device</label>
    <div class="row"><button onclick={() => runAction('Send test alert', t => api.testAlert(t))}>Send test</button><span class="sp"></span><button class="pri" onclick={() => save('alert')}>Review & save</button></div></div>{/if}
  {#if inf}<div class="card fm"><h2>InfluxDB export</h2>
    <label class="chk"><input type="checkbox" bind:checked={inf.enabled} /> Enabled</label>
    <div class="row"><label class="f sp"><span>URL</span><input bind:value={inf.url} placeholder="http://192.0.2.20 or https://influx.example.com" /></label><label class="f" style="width:100px"><span>Port</span><input type="number" bind:value={inf.port} /></label></div>
    <label class="f"><span>Token (leave blank to keep)</span><input type="password" bind:value={token} /></label>
    <div class="row"><label class="f sp"><span>Org</span><input bind:value={inf.org} /></label><label class="f sp"><span>Bucket</span><input bind:value={inf.bucket} /></label><label class="f sp"><span>Prefix</span><input bind:value={inf.prefix} /></label></div>
    <div class="row"><span class="sp"></span><button class="pri" onclick={() => save('influx')}>Review & save</button></div></div>{/if}
</div>
<style>.fm{display:grid;gap:10px;align-content:start}</style>

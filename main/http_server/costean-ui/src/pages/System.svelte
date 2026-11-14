<script>
  import { app, runAction, toast, applySettings } from '../lib/store.svelte.js';
  import { api, upload, inspectImage } from '../lib/api.js';
  import { ask } from '../lib/confirm.svelte.js';
  import { toPatch, diff } from '../lib/backup.js';
  import { downloadBackup } from '../lib/download.js';
  import { fmtUptime, fmtBytes } from '../lib/format.js';
  import { mismatch } from '../lib/versions.js';
  let s = $derived(app.system || {});
  let prog = $state({}), gh = $state({ url: '', keep: true, status: null }), otp = $state(null);
  $effect(() => { if (app.online && otp === null) api.otpStatus().then(v => otp = v).catch(() => otp = { enabled: false }); });
  let mm = $derived(mismatch(s.version, app.wwwVersion?.www));
  async function doUpload(kind, ev) {
    const file = ev.currentTarget.files[0]; ev.currentTarget.value = ''; if (!file) return;
    const info = await inspectImage(file, kind); if (!info.ok) return toast(info.why, 'err');
    const r = await ask(kind === 'www' ? 'Upload web UI (www.bin)' : 'Upload firmware (app .bin)', { danger: true, otp: !!app.identify?.otp,
      detail: `${file.name}\n${fmtBytes(file.size)}${info.version ? '\nimage version: ' + info.version : ''}\n\nInstall the app .bin first, then the matching www.bin. The miner reboots after a firmware upload.` });
    if (!r.ok) return;
    try { await upload(kind, file, p => prog[kind] = p, r.totp); toast(kind === 'www' ? 'Web UI updated — reload the page' : 'Firmware uploaded — miner is rebooting'); } catch (e) { toast('Upload failed: ' + e.message, 'err'); }
    prog[kind] = null;
  }
  async function ghUpdate() {
    if (!/^https:\/\/github\.com\/.+\.bin$|^https:\/\/.+\.bin$/.test(gh.url)) return toast('Enter an https URL to a .bin release asset', 'err');
    if (!await runAction('Update from URL', t => api.githubOta(gh.url, gh.keep, t), { danger: true })) return;
    const iv = setInterval(async () => { try { gh.status = await api.githubOtaStatus(); if (!gh.status.running && !gh.status.pending) clearInterval(iv); } catch { clearInterval(iv); } }, 1500);
  }
  async function restore(ev) {
    const f = ev.currentTarget.files[0]; ev.currentTarget.value = ''; if (!f) return;
    try { const b = JSON.parse(await f.text()); const patch = toPatch(b); const rows = diff(app.settings, patch);
      if (!rows.length) return toast('Backup matches current settings');
      await applySettings(patch, `Restore ${rows.length} setting(s) from ${f.name}`);
    } catch (e) { toast('Restore failed: ' + e.message, 'err'); }
  }
  async function otpToggle() {
    if (!otp.enabled) { const r = await ask('Start OTP enrollment', { detail: 'A QR code will appear on the miner screen. Scan it with your authenticator app, then enter the code to enable.' }); if (!r.ok) return; try { await api.otpEnroll(); } catch (e) { return toast(e.message, 'err'); } }
    const r = await ask(otp.enabled ? 'Disable OTP' : 'Enable OTP', { otp: true, danger: otp.enabled }); if (!r.ok) return;
    try { await api.otpSet(!otp.enabled, r.totp); otp = await api.otpStatus(); toast('OTP ' + (otp.enabled ? 'enabled' : 'disabled')); } catch (e) { toast(e.message, 'err'); }
  }
</script>
<h1>Update & Recovery</h1><div class="card c-gold small">If an update ever goes wrong, the miner keeps a <a href="/recovery">fallback recovery page</a> for uploading www.bin / firmware. USB factory images restore everything.</div>
<div class="grid g2">
  <div class="card"><h2>Device</h2><table><tbody>
    <tr><td class="mute">Model</td><td>{s.deviceModel || '—'} · {s.asicModel || ''}</td></tr>
    <tr><td class="mute">Firmware</td><td class="mono">{s.version || '—'}</td></tr>
    <tr><td class="mute">Web UI</td><td class="mono">{app.wwwVersion?.www || 'unknown'} {#if mm}<span class="pill warn">mismatch</span>{:else if s.version}<span class="pill ok">match</span>{/if}</td></tr>
    <tr><td class="mute">Uptime</td><td>{fmtUptime(s.uptimeSeconds)}</td></tr><tr><td class="mute">Last reset</td><td>{s.lastResetReason || '—'}</td></tr>
    <tr><td class="mute">Free PSRAM / internal</td><td>{s.memory ? fmtBytes(s.memory.freeHeap) + ' / ' + fmtBytes(s.memory.freeHeapInt) : '—'}</td></tr></tbody></table></div>
  <div class="card fm" data-testid="update"><h2>Firmware update</h2>
    <p class="mute small">Install as a pair: app <b>.bin</b> first (miner reboots), then <b>www.bin</b>, then reload. Files are checked locally before upload. Keep the factory image for USB recovery.</p>
    <label class="up"><span>1 · Firmware (app .bin)</span><input type="file" accept=".bin" onchange={e => doUpload('firmware', e)} />{#if prog.firmware != null}<progress value={prog.firmware}></progress>{/if}</label>
    <label class="up"><span>2 · Web UI (www.bin)</span><input type="file" accept=".bin" onchange={e => doUpload('www', e)} />{#if prog.www != null}<progress value={prog.www}></progress>{/if}</label>
    <details><summary class="small">Update from release URL (firmware fetches it)</summary><div class="fm" style="margin-top:8px"><input placeholder="https://github.com/…/esp-miner-NerdOCTAXE-Gamma.bin" bind:value={gh.url} />
      <label class="chk"><input type="checkbox" bind:checked={gh.keep} /> Keep configuration</label><button onclick={ghUpdate}>Start</button>
      {#if gh.status}<div class="small">{gh.status.step} {gh.status.progress}%</div><progress value={gh.status.progress / 100}></progress>{/if}</div></details>
  </div>
  <div class="card fm"><h2>Backup & restore</h2><p class="mute small">Exports /api/v2/settings plus alert and InfluxDB settings to JSON. Passwords and tokens are never exposed by the firmware, so they are not in the file. Restore shows every change for confirmation.</p>
    <div class="row"><button class="pri" onclick={downloadBackup}>Download backup</button><label class="btn">Restore…<input hidden type="file" accept=".json,application/json" onchange={restore} /></label></div></div>
  <div class="card fm"><h2>Power & maintenance</h2><div class="row">
    <button onclick={() => runAction('Restart miner', t => api.restart(t))}>Restart</button>
    <button onclick={() => runAction('Reset session statistics', t => api.resetStats(false, t))}>Reset session stats</button>
    <button onclick={() => runAction('Reset ALL-TIME statistics (best diff, found blocks)', t => api.resetStats(true, t), { danger: true })}>Reset all-time stats</button>
    <button class="danger" onclick={() => runAction('Shut down ASICs', t => api.shutdown(t), { danger: true })}>Shut down ASICs</button></div>
    {#if app.identify?.coreDumpDownload}<a href={api.coredumpUrl()} download>Download core dump</a>{/if}</div>
  <div class="card fm"><h2>Security · OTP</h2><p class="mute small">Time-based one-time codes protect every write (settings, OTA, restart). Codes are entered in the confirm dialog.</p>
    <div class="row"><span class="pill"><span class="dot {otp?.enabled ? 'ok' : ''}"></span>OTP {otp?.enabled ? 'enabled' : 'disabled'}</span><span class="sp"></span><button onclick={otpToggle} disabled={!otp}>{otp?.enabled ? 'Disable' : 'Enable'}</button></div></div>
  <div class="card"><h2>About</h2><p class="small mute">Costean OS web UI {app.wwwVersion?.www}. Built on <a href="https://github.com/shufps/ESP-Miner-NerdQAxePlus">shufps/ESP-Miner-NerdQAxePlus</a> v1.1.2 (itself derived from bitaxeorg/ESP-Miner). Licensed GPL-3.0. Runs entirely on the miner and in your browser — no cloud services.</p></div>
</div>
<style>.fm{display:grid;gap:10px;align-content:start}.up{display:grid;gap:6px;padding:10px;border:1px dashed var(--line);border-radius:12px}.up span{font-weight:600}progress{width:100%}</style>

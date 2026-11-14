<script>
  import { app, applySettings } from '../lib/store.svelte.js';
  import { changes, clone } from '../lib/form.js';
  import { fmtDiff } from '../lib/format.js';
  let e = $state(null), orig = null;
  $effect(() => { if (app.settings && !e) { orig = clone(app.settings); e = clone(app.settings); e.pools.forEach(p => p.password = ''); } });
  let live = $derived(app.dash?.stratum || {});
  async function save() {
    const pick = ({ pools, poolMode, poolBalance, stratumDifficulty, stratumKeep, mempoolCustom, mempoolUrl }) => ({ pools, poolMode, poolBalance, stratumDifficulty, stratumKeep, mempoolCustom, mempoolUrl });
    const p = changes(pick(orig), pick(e)) || {};
    if (p.pools) p.pools = p.pools.map(x => { if (!x.password) delete x.password; return x; });
    if (!Object.keys(p).length || (p.pools && Object.keys(p).length === 1 && p.pools.every(x => !Object.keys(x).length))) return;
    if (await applySettings(p, 'Save pool settings')) { e = null; }
  }
</script>
<h1>Pools & Stratum</h1><div class="mute">Primary + secondary pool with Stratum V1/V2, TLS and coinbase verification.</div>
{#if e}
<div class="card row"><label class="f"><span>Pool mode</span><select bind:value={e.poolMode}><option value={0}>Failover (secondary is backup)</option><option value={1}>Dual pool (split hashrate)</option></select></label>
  {#if e.poolMode === 1}<label class="f" style="flex:1"><span>Balance: {e.poolBalance}% primary / {100 - e.poolBalance}% secondary</span><input type="range" min="0" max="100" bind:value={e.poolBalance} /></label>{/if}
  <span class="pill">{live.usingFallback ? 'Using fallback' : live.activePoolMode === 1 ? 'Dual mode active' : 'Primary active'}</span></div>
<div class="grid g2">
  {#each e.pools as pool, i}
    {@const l = live.pools?.[i]}
    <div class="card fm" data-testid="pool-{i}">
      <div class="row"><h3>{i === 0 ? 'Primary pool' : 'Secondary pool'}</h3><span class="sp"></span>
        <span class="pill"><span class="dot {l?.connected ? 'ok' : 'crit'}"></span>{l?.connected ? 'connected' : 'offline'}{#if l?.encrypted} · 🔒{/if}</span></div>
      {#if l}<div class="mute small">ping {l.pingRtt?.toFixed(0)} ms · loss {l.pingLoss ?? 0}% · diff {fmtDiff(l.poolDifficulty)} · {l.accepted} acc / {l.rejected} rej · best {fmtDiff(l.bestDiff)}</div>{/if}
      <div class="r2"><label class="f" style="flex:3"><span>Stratum host</span><input bind:value={pool.url} placeholder="public-pool.io" /></label><label class="f" style="flex:1"><span>Port</span><input type="number" bind:value={pool.port} /></label></div>
      <label class="f"><span>Worker / user</span><input bind:value={pool.user} placeholder="bc1q….worker" /></label>
      <label class="f"><span>Password (leave blank to keep)</span><input type="password" bind:value={pool.password} autocomplete="new-password" /></label>
      <div class="r2"><label class="f"><span>Protocol</span><select bind:value={pool.protocol}><option value={0}>Stratum V1</option><option value={1}>Stratum V2</option></select></label>
        {#if pool.protocol === 0}<label class="chk"><input type="checkbox" bind:checked={pool.tls} /> TLS (stratum+ssl)</label>{/if}
        <label class="chk"><input type="checkbox" bind:checked={pool.enonceSubscribe} /> Extranonce subscribe</label></div>
      {#if pool.protocol === 1}<label class="f"><span>SV2 authority public key</span><input class="mono" bind:value={pool.sv2AuthorityPubkey} /></label>
        <div class="r2"><label class="f"><span>SV2 channel type</span><input type="number" bind:value={pool.sv2ChannelType} /></label><label class="chk"><input type="checkbox" bind:checked={pool.sv2RequireAuth} /> Require authenticated pool</label></div>{/if}
      <div class="r2"><label class="f"><span>Coinbase verification</span><select bind:value={pool.coinbaseVerifyMode}><option value={0}>Off</option><option value={1}>Basic (payout goes to you)</option><option value={2}>Extended (max pool fee)</option></select></label>
        {#if pool.coinbaseVerifyMode === 2}<label class="f"><span>Max fee %</span><input type="number" step="0.1" bind:value={pool.coinbaseMaxFee} /></label>{/if}
        {#if pool.coinbaseVerifyMode > 0}<label class="chk"><input type="checkbox" bind:checked={pool.coinbaseVerifyForce} /> Block work on failure</label>{/if}</div>
    </div>
  {/each}
</div>
<div class="card fm"><h3>Advanced</h3><div class="r2">
  <label class="f"><span>Suggested difficulty</span><input type="number" bind:value={e.stratumDifficulty} /></label>
  <label class="chk"><input type="checkbox" checked={!!e.stratumKeep} onchange={ev => e.stratumKeep = ev.currentTarget.checked ? 1 : 0} /> Keep stratum connection alive</label>
  <label class="chk"><input type="checkbox" bind:checked={e.mempoolCustom} /> Custom mempool API</label>
  {#if e.mempoolCustom}<label class="f" style="flex:2"><span>Mempool URL</span><input bind:value={e.mempoolUrl} /></label>{/if}</div></div>
<div class="row"><span class="sp"></span><button onclick={() => e = null}>Revert</button><button class="pri" onclick={save} data-testid="save-pools">Review & save</button></div>
{:else}<div class="card mute">Loading settings…</div>{/if}
<style>.fm{display:grid;gap:10px}.r2{display:flex;gap:10px;flex-wrap:wrap;align-items:end}.r2>*{flex:1;min-width:140px}</style>

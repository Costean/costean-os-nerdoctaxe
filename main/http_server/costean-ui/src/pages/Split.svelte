<script>
  // Multi-pool split: weighted job sharing with automatic failover. 2 pools in this UI, firmware designed for 4.
  import { app, applySettings, runAction } from '../lib/store.svelte.js';
  import { api } from '../lib/api.js';
  import { poolRows, effectiveSplit } from '../lib/costean.js';
  import { fmtDiff } from '../lib/format.js';
  let rows = $derived(poolRows(app.dash?.stratum, app.settings));
  let mode = $derived(app.settings?.poolMode);
  let st = $derived(app.costean?.status);
  let bal = $state(app.settings?.poolBalance ?? null);
  $effect(() => { if (app.settings && bal == null) bal = app.settings.poolBalance ?? 50; });
  let preview = $derived(bal == null ? [] : effectiveSplit([bal, 100 - bal], rows.map(r => r.healthy)));
  let dirty = $derived(bal != null && bal !== app.settings?.poolBalance);
  const ths = gh => gh == null ? '—' : (gh / 1000).toFixed(2);
  const name = r => r.host || `Pool ${r.i + 1} (not set)`;
  async function save() { if (await applySettings({ poolBalance: bal }, `Set pool split to ${bal} / ${100 - bal}`)) bal = null; }
  async function switchMode(m) {
    if (await applySettings({ poolMode: m }, m === 1 ? 'Turn on pool split (needs a restart)' : 'Back to failover only (needs a restart)'))
      await runAction('Restart miner now to apply the pool mode', t => api.restart(t));
  }
</script>
<h1>Pool Split</h1><div class="mute">Share your hashrate between pools by percentage. If a pool goes down, its share moves to the others automatically and comes back 30 s after the pool recovers.</div>
{#if app.settings && bal != null}
{#if mode !== 1}
<div class="card c-gold row" data-testid="split-off"><div><b>Split is off.</b> <span class="mute">The miner is in failover mode: pool 1 gets all the work and pool 2 is only a backup.</span></div><span class="sp"></span>
  <button class="pri" onclick={() => switchMode(1)}>Turn on split…</button></div>
{/if}
<section class="card c-cyan fm" data-testid="split-card">
  <div class="row"><h2>Split</h2><span class="sp"></span>
    {#if st?.failover}<span class="tag warn" data-testid="failover-badge">failover active</span>{:else if mode === 1}<span class="tag ok">all pools healthy</span>{/if}</div>
  <div class="big-split">
    <div class="side"><span class="dot p0"></span><div><div class="pct mono">{bal}%</div><div class="mute small">{name(rows[0] || { i: 0 })}</div></div></div>
    <input type="range" min="0" max="100" step="1" bind:value={bal} disabled={mode !== 1} data-testid="split-slider" />
    <div class="side r"><div><div class="pct mono">{100 - bal}%</div><div class="mute small">{name(rows[1] || { i: 1 })}</div></div><span class="dot p1"></span></div>
  </div>
  <div class="splitbar tall">{#each preview as w, i}<i class="p{i}" style:width="{w}%"></i>{/each}</div>
  <div class="row small"><span class="mute">Effective now (after failover): {preview.map((w, i) => `pool ${i + 1} ${w}%`).join(' · ')}</span><span class="sp"></span>
    {#if dirty}<button onclick={() => bal = app.settings.poolBalance}>Revert</button><button class="pri" onclick={save} data-testid="save-split">Review & save</button>{/if}</div>
  <div class="mute small">A pool set to 0% is a backup only: it gets work just when every weighted pool is down. Changes apply live, no restart.</div>
</section>
<section class="grid g2">
  {#each rows as r}
    <div class="card pool c-{r.i ? 'blue' : 'cyan'}" data-testid="split-pool-{r.i}">
      <div class="row"><span class="dot p{r.i}"></span><h3 class="mono">{name(r)}{r.port ? ':' + r.port : ''}</h3><span class="sp"></span>
        <span class="pill"><span class="dot {r.healthy ? 'ok' : 'crit'}"></span>{r.healthy ? 'healthy' : r.configured ? 'down' : 'not set'}</span></div>
      <div class="nums">
        <div><div class="eyebrow">Weight</div><div class="n mono">{r.weight}%</div></div>
        <div><div class="eyebrow">Getting</div><div class="n mono" class:warn={r.effective != null && r.effective !== r.weight}>{r.effective ?? '—'}%</div></div>
        <div><div class="eyebrow">Hashrate</div><div class="n mono">{ths(r.assigned)}<small> TH/s</small></div></div>
        <div><div class="eyebrow">Pool credit</div><div class="n mono">{ths(r.credited)}<small> TH/s</small></div></div>
      </div>
      <div class="row small mono"><span class="ok">{r.accepted} accepted</span><span class={r.rejected ? 'warn' : 'mute'}>{r.rejected} rejected ({r.rej.toFixed(2)}%)</span><span class="sp"></span>
        <span class="mute">diff {fmtDiff(r.diff)} · ping {r.ping ? r.ping.toFixed(0) + ' ms' : '—'}</span></div>
      <div class="mute small">Job share {r.jobShare != null ? (r.jobShare * 100).toFixed(1) + '%' : '—'} of work sent. Pool credit is accepted share difficulty over the last 10 min, so it is noisy at low difficulty.</div>
    </div>
  {/each}
</section>
<section class="card fm">
  <h2>How failover works</h2>
  <ol class="small how">
    <li>Each new job goes to a pool picked by weighted round robin, so a 70 / 30 split is exact over time, not random.</li>
    <li>A pool that disconnects, has no work or is blocked by coinbase checks drops out at once. Its share goes to the healthy pools in proportion.</li>
    <li>When it comes back it must stay up for 30 s before it rejoins, so a flapping pool cannot steal work.</li>
    <li>Pool settings (hosts, workers, TLS) live on <a href="#/pools">Pools & Stratum</a>. {mode === 1 ? '' : 'Split needs the dual pool mode.'}</li>
  </ol>
  <div class="row small"><span class="tag">pools 3 and 4</span><span class="mute">The split engine already handles up to {st?.designMaxPools ?? 4} pools; this release shows {st?.maxPools ?? 2}.</span><span class="sp"></span>
    {#if mode === 1}<button onclick={() => switchMode(0)}>Back to failover only…</button>{/if}</div>
</section>
{:else}<div class="card mute">Loading settings…</div>{/if}
<style>.fm{display:grid;gap:12px}.big-split{display:grid;grid-template-columns:minmax(180px,1fr) 2fr minmax(180px,1fr);gap:18px;align-items:center}
.side{display:flex;gap:10px;align-items:center}.side.r{justify-content:flex-end;text-align:right}.side .dot{width:12px;height:12px;animation:none;box-shadow:none}
.pct{font-size:34px;font-weight:800;line-height:1}input[type=range]{accent-color:var(--acc)}.tall{height:22px;border-radius:9px}
.pool{display:grid;gap:10px}.pool h3{margin:0;font-size:14px}.pool .dot.p0,.pool .dot.p1{animation:none;box-shadow:none}
.nums{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.n{font-size:22px;font-weight:800}.n small{font-size:11px;color:var(--mute)}
.how{margin:0;padding-left:18px;display:grid;gap:4px}
@media (max-width:800px){.big-split{grid-template-columns:1fr}.nums{grid-template-columns:1fr 1fr}}</style>

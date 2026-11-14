<script>
  // rows × time cells. rows: [{label, values:[number|null]}]; scale(v)->0..1
  let { rows = [], times = [], lo = 0, hi = 1, unit = '', palette = 'heat', height = 16 } = $props();
  const col = (f, p) => { if (f == null || isNaN(f)) return 'var(--glass2)'; f = Math.max(0, Math.min(1, f));
    return p === 'heat' ? `hsl(${220 - f * 220} 85% ${32 + f * 22}%)` : `hsl(${f * 140} 70% ${30 + f * 20}%)`; };
  let tip = $state(null);
</script>
<div class="hm" data-testid="heatmap">
  {#each rows as r, ri}
    <div class="r"><span class="lb">{r.label}</span>
      <div class="cells" style:grid-template-columns="repeat({Math.max(1, r.values.length)},1fr)" style:height="{height}px">
        {#each r.values as v, i}<i style:background={col(v == null ? null : (v - lo) / ((hi - lo) || 1), palette)} onpointerenter={() => tip = { ri, i, v }} onpointerleave={() => tip = null}></i>{/each}
      </div></div>
  {/each}
  <div class="ft mute small"><span>{times[0] ? new Date(times[0]).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : ''}</span>
    {#if tip}<span>{rows[tip.ri].label} · {times[tip.i] ? new Date(times[tip.i]).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : ''} · <b>{tip.v?.toFixed(1) ?? '—'} {unit}</b></span>{:else}<span class="scale"><em>{lo.toFixed(0)}</em><b class={palette}></b><em>{hi.toFixed(0)} {unit}</em></span>{/if}
    <span>now</span></div>
</div>
<style>.hm{display:grid;gap:3px}.r{display:grid;grid-template-columns:34px 1fr;gap:6px;align-items:center}.lb{font:600 11px var(--num);color:var(--mute)}
.cells{display:grid;gap:1px;border-radius:4px;overflow:hidden}.cells i{display:block;min-width:1px}.cells i:hover{outline:1px solid var(--text)}
.ft{display:flex;justify-content:space-between;margin-top:4px;padding-left:40px}.scale{display:flex;gap:6px;align-items:center}.scale em{font-style:normal}
.scale b{display:inline-block;width:90px;height:6px;border-radius:3px}.scale b.heat{background:linear-gradient(90deg,hsl(220 85% 32%),hsl(110 85% 43%),hsl(0 85% 54%))}.scale b.good{background:linear-gradient(90deg,hsl(0 70% 30%),hsl(140 70% 50%))}</style>

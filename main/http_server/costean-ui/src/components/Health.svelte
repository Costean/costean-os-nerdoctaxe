<script>
  let { insight } = $props();
  let s = $derived(insight?.score ?? 0);
  let col = $derived(s >= 90 ? 'var(--ok)' : s >= 75 ? 'var(--acc2)' : s >= 50 ? 'var(--warn)' : 'var(--crit)');
  const C = 2 * Math.PI * 52;
</script>
<div class="card hl">
  <h2>Health · local insights</h2>
  <div class="row" style="align-items:flex-start;gap:18px">
    <svg width="130" height="130" viewBox="0 0 130 130" data-testid="health"><circle cx="65" cy="65" r="52" stroke="var(--glass2)" stroke-width="12" fill="none"/>
      <circle cx="65" cy="65" r="52" stroke={col} stroke-width="12" fill="none" stroke-linecap="round" stroke-dasharray="{C * s / 100} {C}" transform="rotate(-90 65 65)" style="transition:stroke-dasharray .6s"/>
      <text x="65" y="66" text-anchor="middle" font-size="32" font-weight="700" fill="var(--text)">{s}</text><text x="65" y="88" text-anchor="middle" font-size="11" fill="var(--mute)">{insight?.grade?.toUpperCase()}</text></svg>
    <div class="sp items">
      {#each insight?.items || [] as it}<div class="it {it.level}"><b>{it.title}</b><div class="mute small">{it.why}</div></div>
      {:else}<div class="it ok"><b>All systems nominal</b><div class="mute small">Temperatures, chips, shares and pool connection look healthy.</div></div>{/each}
    </div>
  </div>
</div>
<style>.items{display:grid;gap:6px;min-width:200px}.it{border-left:3px solid var(--mute);padding:4px 10px;background:var(--glass2);border-radius:0 10px 10px 0}
.it.ok{border-color:var(--ok)}.it.warn{border-color:var(--warn)}.it.crit{border-color:var(--crit)}.it.info{border-color:var(--acc2)}</style>

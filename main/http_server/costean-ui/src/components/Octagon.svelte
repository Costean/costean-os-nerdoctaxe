<script>
  // "Octa radar": the 8 BM1370s on an octagon. Outer polygon = hashrate vs board mean, inner = temperature. Each vertex is a chip.
  let { chips = [], temps = [], errs = [], size = 320, sel = $bindable(-1) } = $props();
  const c = 160, R = 120;
  let mean = $derived(chips.length ? chips.reduce((a, b) => a + b, 0) / chips.length : 0);
  const ang = i => (-90 + i * 45) * Math.PI / 180;
  const P = (i, r) => [c + r * Math.cos(ang(i)), c + r * Math.sin(ang(i))];
  // hashrate ring: mean sits at 75% radius, ±33% spans the band
  const hr = h => R * Math.max(0.15, Math.min(1, 0.75 + (mean ? (h - mean) / mean : 0) * 0.75));
  const tr = t => R * Math.max(0.1, Math.min(0.62, ((t ?? 40) - 35) / 40 * 0.62));
  const poly = f => Array.from({ length: 8 }, (_, i) => P(i, f(i)).map(v => v.toFixed(1)).join(',')).join(' ');
  const tone = t => t == null ? 'var(--mute)' : t >= 70 ? 'var(--crit)' : t >= 65 ? 'var(--warn)' : t >= 55 ? 'var(--acc2)' : 'var(--ok)';
  const er = i => { const t = (chips[i] || 0) + (errs[i] || 0); return t ? (errs[i] || 0) / t * 100 : 0; };
</script>
<svg viewBox="0 0 320 320" width="100%" style:max-width="{size}px" class="o" data-testid="octa" role="img" aria-label="Per-chip radar">
  <defs><radialGradient id="og"><stop offset="0" stop-color="var(--acc)" stop-opacity=".25"/><stop offset="1" stop-color="var(--acc)" stop-opacity="0"/></radialGradient></defs>
  {#each [0.25, 0.5, 0.75, 1] as k}<polygon points={poly(() => R * k)} class="ring" class:mean={k === 0.75}/>{/each}
  {#each Array(8) as _, i}{@const p = P(i, R)}<line x1={c} y1={c} x2={p[0]} y2={p[1]} class="spoke"/>{/each}
  {#if chips.length}
    <polygon points={poly(i => hr(chips[i] || 0))} class="hr" fill="url(#og)"/>
    <polygon points={poly(i => tr(temps[i]))} class="tp"/>
    {#each chips as h, i}{@const p = P(i, hr(h))}{@const lp = P(i, R + 22)}
      <g class="node" class:on={sel === i} onclick={() => sel = sel === i ? -1 : i} role="button" tabindex="0" onkeydown={e => e.key === 'Enter' && (sel = i)}>
        <circle cx={p[0]} cy={p[1]} r={sel === i ? 8 : 6} fill={tone(temps[i])} stroke="var(--bg)" stroke-width="2"/>
        {#if er(i) > 2}<circle cx={p[0]} cy={p[1]} r="12" class="pulse"/>{/if}
        <text x={lp[0]} y={lp[1] - 3} text-anchor="middle" class="lb">#{i + 1}</text>
        <text x={lp[0]} y={lp[1] + 9} text-anchor="middle" class="lv">{h.toFixed(0)}</text>
      </g>{/each}
  {/if}
  <text x={c} y={c - 4} text-anchor="middle" class="cv">{(chips.reduce((a, b) => a + b, 0) / 1000).toFixed(2)}</text>
  <text x={c} y={c + 12} text-anchor="middle" class="cu">TH/s · 8× BM1370</text>
</svg>
<style>.o{display:block;margin:auto;overflow:visible}.ring{fill:none;stroke:var(--line)}.ring.mean{stroke-dasharray:4 4;stroke:var(--mute);opacity:.5}.spoke{stroke:var(--line)}
.hr{stroke:var(--acc);stroke-width:2;stroke-linejoin:round;transition:all .8s}.tp{fill:color-mix(in srgb,var(--warn) 18%,transparent);stroke:var(--warn);stroke-width:1.5;stroke-dasharray:3 2;transition:all .8s}
.node{cursor:pointer}.node circle{transition:all .8s}.lb{font:700 11px var(--num);fill:var(--text)}.lv{font:10px var(--num);fill:var(--mute)}
.pulse{fill:none;stroke:var(--crit);animation:p 1.6s infinite}@keyframes p{from{opacity:1;r:8}to{opacity:0;r:18}}
.cv{font:700 26px var(--num);fill:var(--text)}.cu{font:10px var(--ui);fill:var(--mute);letter-spacing:.1em}</style>

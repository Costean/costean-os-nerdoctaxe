<script>
  // efficiency map: x = frequency, y = J/TH, bubble = GH/s, color = max temp
  let { pts = [], cur = null, height = 260 } = $props();
  let w = $state(500); const pad = { l: 44, r: 14, t: 14, b: 30 };
  let xs = $derived(pts.map(p => p.f)), ys = $derived(pts.map(p => p.jth));
  let x0 = $derived(Math.min(...xs, cur?.f ?? Infinity) - 15), x1 = $derived(Math.max(...xs, cur?.f ?? -Infinity) + 15);
  let y0 = $derived(Math.min(...ys) * 0.95), y1 = $derived(Math.max(...ys) * 1.05);
  const X = v => pad.l + (v - x0) / ((x1 - x0) || 1) * (w - pad.l - pad.r), Y = v => pad.t + (1 - (v - y0) / ((y1 - y0) || 1)) * (height - pad.t - pad.b);
  let best = $derived(pts.length ? pts.reduce((a, b) => b.jth < a.jth ? b : a) : null);
  let ghMax = $derived(Math.max(1, ...pts.map(p => p.gh)));
  const tone = t => t >= 70 ? 'var(--crit)' : t >= 65 ? 'var(--warn)' : t >= 55 ? 'var(--acc2)' : 'var(--ok)';
</script>
<div bind:clientWidth={w} class="sc" data-testid="scatter">
{#if !pts.length}<div class="empty mute" style:height="{height}px">No settled points yet. Each frequency/voltage you run for 3+ minutes becomes a point here.</div>{:else}
<svg width={w} {height}>
  {#each [0, 1, 2, 3] as i}{@const v = y0 + (y1 - y0) * i / 3}<line x1={pad.l} x2={w - pad.r} y1={Y(v)} y2={Y(v)} class="gl"/><text x={pad.l - 6} y={Y(v) + 4} text-anchor="end" class="ax">{v.toFixed(1)}</text>{/each}
  {#each pts as p}<circle cx={X(p.f)} cy={Y(p.jth)} r={5 + p.gh / ghMax * 9} fill={tone(p.tmax)} fill-opacity=".55" stroke={tone(p.tmax)}><title>{p.f} MHz · {p.mv} mV · {p.gh.toFixed(0)} GH/s · {p.w.toFixed(1)} W · {p.jth.toFixed(2)} J/TH · max {p.tmax.toFixed(1)}°C · {p.mins.toFixed(0)} min</title></circle>
    <text x={X(p.f)} y={Y(p.jth) - 16} text-anchor="middle" class="ax">{p.mv}</text>{/each}
  {#if best}<circle cx={X(best.f)} cy={Y(best.jth)} r="20" class="best"/>{/if}
  <text x={pad.l} y={height - 8} class="ax">{x0.toFixed(0)} MHz</text><text x={w - pad.r} y={height - 8} text-anchor="end" class="ax">{x1.toFixed(0)} MHz</text>
  <text x={w / 2} y={height - 8} text-anchor="middle" class="ax">frequency → · labels = mV · size = GH/s · colour = max °C</text>
</svg>{/if}
</div>
<style>.sc{width:100%}.gl{stroke:var(--line)}.ax{fill:var(--mute);font:10px var(--num)}.best{fill:none;stroke:var(--ok);stroke-dasharray:4 3;animation:s 6s linear infinite}@keyframes s{to{stroke-dashoffset:-70}}.empty{display:grid;place-items:center;text-align:center;padding:20px}</style>

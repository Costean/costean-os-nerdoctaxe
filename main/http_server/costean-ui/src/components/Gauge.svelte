<script>
  // 270° instrument arc with tick ring and needle glow
  let { value = 0, max = 1, label = '', unit = '', text = '', size = 180, color = 'var(--acc)', zones = [] } = $props();
  const R = 70, C = 2 * Math.PI * R, ARC = C * 0.75;
  let f = $derived(Math.max(0, Math.min(1, max ? value / max : 0)));
  const pt = (fr, r) => { const a = (135 + fr * 270) * Math.PI / 180; return [90 + r * Math.cos(a), 90 + r * Math.sin(a)]; };
</script>
<svg viewBox="-8 -8 196 196" width={size} height={size} class="g" role="img" aria-label={label}>
  <circle cx="90" cy="90" r={R} class="trk" stroke-dasharray="{ARC} {C}" transform="rotate(135 90 90)"/>
  {#each zones as z}<circle cx="90" cy="90" r={R - 11} fill="none" stroke={z.c} stroke-width="3" stroke-dasharray="{ARC * (z.to - z.from)} {C}" stroke-dashoffset={-ARC * z.from} transform="rotate(135 90 90)" opacity=".8"/>{/each}
  <circle cx="90" cy="90" r={R} fill="none" stroke={color} stroke-width="10" stroke-linecap="round" stroke-dasharray="{ARC * f} {C}" transform="rotate(135 90 90)" style="transition:stroke-dasharray .8s;filter:drop-shadow(0 0 6px {color})"/>
  {#each Array(28) as _, i}{@const a = pt(i / 27, 84)}{@const b = pt(i / 27, i % 3 ? 88 : 92)}<line x1={a[0]} y1={a[1]} x2={b[0]} y2={b[1]} class="tk"/>{/each}
  <text x="90" y="92" text-anchor="middle" class="v">{text}</text>
  <text x="90" y="112" text-anchor="middle" class="u">{unit}</text>
  <text x="90" y="160" text-anchor="middle" class="l">{label}</text>
</svg>
<style>.g{display:block;margin:auto}.trk{fill:none;stroke:var(--line);stroke-width:10;stroke-linecap:round}.tk{stroke:var(--mute);opacity:.5}
.v{font:700 30px var(--num);fill:var(--text)}.u{font:8.5px var(--num);fill:var(--mute)}.l{font:600 10px var(--ui);fill:var(--mute);letter-spacing:.18em;text-transform:uppercase}</style>

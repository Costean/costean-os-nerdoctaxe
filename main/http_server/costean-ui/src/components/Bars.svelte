<script>
  // histogram bars [{x0,x1,n}], highlights the bin containing `mark`
  let { bins = [], mark = null, unit = '', color = 'var(--acc2)', height = 120 } = $props();
  let mx = $derived(Math.max(1, ...bins.map(b => b.n)));
</script>
<div class="bars" style:height="{height}px">{#each bins as b}<i class:mk={mark != null && mark >= b.x0 && mark < b.x1} style:height="{b.n / mx * 100}%" style:--c={color} title="{b.x0.toFixed(0)}–{b.x1.toFixed(0)} {unit}: {b.n}"></i>{/each}</div>
{#if bins.length}<div class="ax mute small"><span>{bins[0].x0.toFixed(0)} {unit}</span><span>{bins[bins.length - 1].x1.toFixed(0)} {unit}</span></div>{:else}<div class="mute small">Collecting samples…</div>{/if}
<style>.bars{display:flex;align-items:flex-end;gap:2px}.bars i{flex:1;background:color-mix(in srgb,var(--c) 55%,transparent);border-radius:3px 3px 0 0;min-height:1px}.bars i.mk{background:var(--warn);box-shadow:0 0 10px var(--warn)}
.ax{display:flex;justify-content:space-between;margin-top:4px}</style>

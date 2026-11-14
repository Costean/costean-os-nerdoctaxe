<script>
  import { onMount } from 'svelte';
  import { logSocket } from '../lib/api.js';
  let lines = $state([]), filter = $state(sessionStorage.getItem('costean.logFilter') || ''), paused = $state(false), st = $state('connecting'), box = $state();
  onMount(() => { sessionStorage.removeItem('costean.logFilter'); const ws = logSocket(l => { if (paused) return; lines.push(l.replace(/\x1b\[[0-9;]*m/g, '')); if (lines.length > 2000) lines.splice(0, lines.length - 2000); queueMicrotask(() => box && (box.scrollTop = box.scrollHeight)); }, s => st = s); return () => ws.close(); });
  let shown = $derived(filter ? lines.filter(l => l.toLowerCase().includes(filter.toLowerCase())) : lines);
  const lvl = l => /^E \(|error/i.test(l) ? 'crit' : /^W \(/.test(l) ? 'warn' : '';
  function dl() { const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([lines.join('\n')])); a.download = 'costean-nerdoctaxe-log.txt'; a.click(); }
</script>
<div class="row"><h2 style="margin:0">Live logs</h2><span class="pill"><span class="dot {st === 'open' ? 'ok' : 'crit'}"></span>{st}</span><span class="sp"></span>
  <input style="width:200px" placeholder="Filter…" bind:value={filter} /><button onclick={() => paused = !paused}>{paused ? 'Resume' : 'Pause'}</button><button onclick={() => lines = []}>Clear</button><button onclick={dl}>Download</button></div>
<div class="card log mono" bind:this={box}>{#each shown as l}<div class={lvl(l)}>{l}</div>{:else}<div class="mute">Waiting for log lines over /api/ws…</div>{/each}</div>
<style>.log{height:70vh;overflow:auto;font-size:12px;white-space:pre-wrap;word-break:break-all}</style>

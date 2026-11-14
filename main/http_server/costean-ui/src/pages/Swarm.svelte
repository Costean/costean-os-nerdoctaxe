<script>
  // Swarm: other miners on your LAN (saved only in this browser) + CAN swarm nodes when the extension is fitted.
  import { onMount } from 'svelte';
  import { app, switchMiner } from '../lib/store.svelte.js';
  import * as M from '../lib/miners.js';
  import { fmtHash } from '../lib/format.js';
  let miners = $state(M.load()), host = $state(''), live = $state({}), can = $state(null);
  async function poll() {
    for (const m of miners) fetch(m.origin + '/api/v2/dashboard', { signal: AbortSignal.timeout(4000) }).then(r => r.json()).then(d => live[m.origin] = M.isMinerDash(d) ? { ok: true, gh: d.performance.hashRate, w: d.power?.watts, t: d.thermal?.asicTemp } : { ok: false }).catch(() => live[m.origin] = { ok: false });
    if (app.settings?.can?.hasExtension) fetch((app.base || '') + '/api/v2/can/nodes').then(r => r.json()).then(v => can = v).catch(() => {});
  }
  onMount(() => { poll(); const iv = setInterval(poll, 10000); return () => clearInterval(iv); });
  function add() { miners = M.addMiner(miners, host); M.save(miners); host = ''; poll(); }
  function rm(o) { miners = M.removeMiner(miners, o); M.save(miners); }
  let me = $derived(app.dash);
</script>
<h1>Swarm</h1><div class="mute">Watch every miner on your network from here. The list is stored only in this browser; nothing leaves your LAN.</div>
<div class="card c-cyan">
  <table data-testid="swarm"><thead><tr><th>Miner</th><th>Status</th><th>Hashrate</th><th>Power</th><th>PKG max</th><th></th></tr></thead><tbody>
    <tr><td><b>This miner</b> <span class="mute small mono">{app.system?.network?.ipAddr || ''}</span></td><td><span class="dot {app.online ? 'ok' : 'crit'}" style="display:inline-block"></span></td><td class="num">{fmtHash(me?.performance?.hashRate)}</td><td class="num">{me?.power?.watts?.toFixed(1) ?? '—'} W</td><td class="num">{me?.thermal?.asicTemp?.toFixed(1) ?? '—'} °C</td><td><button onclick={() => switchMiner('')} disabled={!app.base}>View</button></td></tr>
    {#each miners as m}{@const l = live[m.origin]}
    <tr><td><b>{m.name}</b></td><td><span class="dot {l?.ok ? 'ok' : 'crit'}" style="display:inline-block"></span></td><td class="num">{fmtHash(l?.gh)}</td><td class="num">{l?.w?.toFixed(1) ?? '—'} W</td><td class="num">{l?.t?.toFixed(1) ?? '—'} °C</td>
      <td class="row"><button onclick={() => switchMiner(m.origin)}>View</button><button onclick={() => rm(m.origin)} aria-label="Remove">✕</button></td></tr>
    {/each}</tbody></table>
  <div class="row" style="margin-top:12px"><input style="max-width:320px" placeholder="Add miner IP, e.g. 192.0.2.21" bind:value={host} onkeydown={e => e.key === 'Enter' && add()} /><button class="pri" onclick={add}>Add</button></div>
</div>
<div class="card c-ice"><h2>CAN swarm extension</h2>
  {#if app.settings?.can?.hasExtension}
    {#if can?.nodes?.length}<table><thead><tr><th>Node</th><th>Hashrate</th><th>Temp</th></tr></thead><tbody>{#each can.nodes as n}<tr><td>{n.id ?? n.name}</td><td>{fmtHash(n.hashRate)}</td><td>{n.asicTemp ?? '—'}</td></tr>{/each}</tbody></table>{:else}<div class="mute">No CAN nodes reported.</div>{/if}
  {:else}<div class="mute small">No CAN extension detected on this board (firmware reports <span class="mono">can.hasExtension = false</span>).</div>{/if}
</div>

import { api } from './api.js';
import { makeBackup } from './backup.js';
import { toast } from './store.svelte.js';
export async function downloadBackup() {
  try {
    const [s, inf, al] = await Promise.all([api.settings(), api.influx().catch(() => null), api.alert().catch(() => null)]);
    const b = makeBackup(s, { influx: inf ? { ...inf, token: undefined } : null, alert: al });
    const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([JSON.stringify(b, null, 2)], { type: 'application/json' }));
    a.download = `costean-nerdoctaxe-${(s.hostname || 'miner')}-${new Date().toISOString().slice(0, 10)}.json`; a.click(); toast('Backup downloaded');
  } catch (e) { toast('Backup failed: ' + e.message, 'err'); }
}

// Settings backup/restore. GET /api/v2/settings -> JSON file; restore PATCHes only writable keys.
export const WRITABLE = ['hostname', 'ssid', 'frequency', 'coreVoltage', 'vrFrequency', 'jobInterval', 'stratumDifficulty', 'flipScreen', 'invertScreen', 'autoScreenOff', 'mempoolCustom', 'mempoolUrl', 'invertFanPolarity', 'pidUseMax', 'stratumKeep', 'canMaster', 'poolMode', 'poolBalance'];
const POOL_KEYS = ['url', 'port', 'user', 'enonceSubscribe', 'tls', 'protocol', 'sv2AuthorityPubkey', 'sv2ChannelType', 'sv2RequireAuth', 'coinbaseVerifyMode', 'coinbaseMaxFee', 'coinbaseVerifyForce'];
const FAN_KEYS = ['mode', 'manualSpeed', 'overheatTemp'];
export function makeBackup(settings, extra = {}) {
  return { format: 'costean-settings', formatVersion: 1, created: new Date().toISOString(), deviceModel: settings.deviceModel, firmware: settings.version, settings, ...extra,
    note: 'Wi-Fi password and pool passwords are not exposed by the firmware API and are not included.' };
}
const pick = (o, keys) => Object.fromEntries(keys.filter(k => o && o[k] !== undefined).map(k => [k, o[k]]));
export function toPatch(backup) {
  if (!backup || backup.format !== 'costean-settings' || !backup.settings) throw new Error('Not a NerdOctAxe settings backup');
  const s = backup.settings; const patch = pick(s, WRITABLE);
  if (s.canMaster === undefined && s.can) patch.canMaster = !!s.can.enabled;
  if (Array.isArray(s.pools)) patch.pools = s.pools.slice(0, 2).map(p => pick(p, POOL_KEYS));
  if (Array.isArray(s.fans)) patch.fans = s.fans.slice(0, 2).map(f => ({ ...pick(f, FAN_KEYS), ...(f.pid ? { pid: f.pid.targetTemp >= 0 ? f.pid : pick(f.pid, ['p', 'i', 'd']) } : {}) }));
  for (const k of ['flipScreen', 'invertScreen', 'autoScreenOff', 'invertFanPolarity']) if (k in patch) patch[k] = !!patch[k];
  return patch;
}
export function diff(current, patch) {
  const rows = [];
  const walk = (a, b, path) => { for (const k of Object.keys(b)) { const p = path ? path + '.' + k : k; if (b[k] && typeof b[k] === 'object') walk(a?.[k] ?? {}, b[k], p); else if (a?.[k] !== b[k]) rows.push([p, a?.[k], b[k]]); } };
  walk(current, patch, ''); return rows;
}

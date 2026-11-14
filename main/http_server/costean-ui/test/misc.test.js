import { describe, it, expect } from 'vitest';
import { toPatch, makeBackup, diff } from '../src/lib/backup.js';
import { decodeHistory, Ring, downsample } from '../src/lib/history.js';
import { normalize, addMiner, removeMiner, load, save, isMinerDash } from '../src/lib/miners.js';
import { mismatch } from '../src/lib/versions.js';
import { changes } from '../src/lib/form.js';
import { efficiency, fmtHash, fmtDiff } from '../src/lib/format.js';
import { settings } from './fixtures.js';
describe('backup', () => {
  it('round trips writable keys only', () => { const p = toPatch(makeBackup(settings)); expect(p.frequency).toBe(525); expect(p.version).toBeUndefined(); expect(p.deviceModel).toBeUndefined(); expect(p.flipScreen).toBe(true); expect(p.pools[0].url).toBe('public-pool.io'); expect(p.canMaster).toBe(false); });
  it('rejects junk', () => expect(() => toPatch({ foo: 1 })).toThrow());
  it('diff lists changes', () => { const rows = diff(settings, { frequency: 600, pools: [{ port: 1 }] }); expect(rows.map(r => r[0])).toEqual(['frequency', 'pools.0.port']); });
});
describe('history', () => {
  it('decodes x100 ints', () => { const h = { timestamps: [0, 1000], hashrate_1m: [960000, 970000], hashrate_10m: [1, 2], hashrate_1h: [1, 2], hashrate_1d: [1, 2], asicTemp: [5800, 5900], vregTemp: [6000, 6100] }; const r = decodeHistory(h, 10000); expect(r[1]).toMatchObject({ t: 10000, hr1m: 9700, asic: 59 }); expect(r[0].t).toBe(9000); });
  it('ring caps', () => { const r = new Ring(3); for (let i = 0; i < 5; i++) r.push({ t: i }); expect(r.a.map(x => x.t)).toEqual([2, 3, 4]); });
  it('downsample', () => expect(downsample(Array(1000).fill(0), 100)).toHaveLength(100));
});
describe('miners', () => {
  it('normalizes', () => { expect(normalize('192.0.2.5')).toBe('http://192.0.2.5'); expect(normalize('http://10.0.0.2:8080/')).toBe('http://10.0.0.2:8080'); expect(normalize('')).toBe(''); });
  it('add/remove/dedupe', () => { let l = addMiner([], '10.0.0.2'); l = addMiner(l, '10.0.0.2'); expect(l).toHaveLength(1); expect(removeMiner(l, 'http://10.0.0.2')).toHaveLength(0); });
  it('only accepts real miner replies (v1.1.2 swarm hardening)', () => { expect(isMinerDash({ performance: { hashRate: 9600 } })).toBe(true); expect(isMinerDash({ performance: { hashRate: 0 } })).toBe(true); expect(isMinerDash({})).toBe(false); expect(isMinerDash(null)).toBe(false); expect(isMinerDash({ performance: { hashRate: '9600' } })).toBe(false); expect(isMinerDash({ performance: { hashRate: -1 } })).toBe(false); expect(isMinerDash('<html>')).toBe(false); });
  it('persists', () => { const m = new Map(); const ls = { getItem: k => m.get(k) ?? null, setItem: (k, v) => m.set(k, v) }; save([{ origin: 'x' }], ls); expect(load(ls)).toEqual([{ origin: 'x' }]); });
});
describe('misc', () => {
  it('version mismatch', () => { expect(mismatch('a', 'a')).toBeNull(); expect(mismatch('nerdoctaxe-1.1.1-ui1', 'v1.1.1').text).toMatch(/pair/); expect(mismatch(null, 'x')).toBeNull(); });
  it('form changes', () => { expect(changes({ a: 1, p: [{ u: 'x', v: 1 }] }, { a: 1, p: [{ u: 'y', v: 1 }] })).toEqual({ p: [{ u: 'y' }] }); expect(changes({ a: 1 }, { a: 1 })).toBeUndefined(); expect(changes({ pw: undefined }, { pw: '' })).toBeUndefined(); });
  it('efficiency', () => { expect(efficiency(150, 10000)).toBe(15); expect(efficiency(0, 1)).toBeNull(); expect(fmtHash(9600)).toBe('9.60 TH/s'); expect(fmtDiff(1.5e9)).toBe('1.50G'); });
});

import { describe, it, expect } from 'vitest';
import { analyze, isAboveStock, zAnomaly } from '../src/lib/insights.js';
import { dash, settings } from './fixtures.js';
describe('health score', () => {
  it('healthy miner scores high', () => { const r = analyze(dash(), settings); expect(r.score).toBe(100); expect(r.items).toHaveLength(0); expect(r.efficiency).toBeCloseTo(15.6, 1); });
  it('offline is 0', () => expect(analyze(null).score).toBe(0));
  it('hot ASIC is critical', () => { const d = dash(); d.thermal.asicTemp = 72; const r = analyze(d); expect(r.items.find(i => i.key === 'asicTemp').level).toBe('crit'); expect(r.score).toBeLessThan(80); });
  it('flags a weak chip', () => { const d = dash(); d.performance.chips[3].hashRate = 600; expect(analyze(d).items.some(i => i.key === 'chip3')).toBe(true); });
  it('flags chip hw errors', () => { const d = dash(); d.performance.chips[1].errorHashRate = 100; expect(analyze(d).items.some(i => i.key === 'chipErr1')).toBe(true); });
  it('no pool connected', () => { const d = dash(); d.stratum.pools.forEach(p => p.connected = false); expect(analyze(d).items.some(i => i.key === 'pool')).toBe(true); });
  it('board error', () => { const d = dash(); d.system.boardError = 1; expect(analyze(d).score).toBeLessThanOrEqual(50); });
  it('reject rate', () => { const d = dash(); d.performance.sharesRejected = 50; expect(analyze(d).items.some(i => i.key === 'rejects')).toBe(true); });
  it('above stock info', () => { expect(analyze(dash(), { ...settings, frequency: 600 }).items.some(i => i.key === 'aboveStock')).toBe(true); });
});
describe('above stock', () => {
  it('detects freq or voltage', () => { expect(isAboveStock(600, 1150, settings)).toBe(true); expect(isAboveStock(525, 1200, settings)).toBe(true); expect(isAboveStock(450, 1100, settings)).toBe(false); expect(isAboveStock(null, null, settings)).toBe(false); });
});
describe('z anomaly', () => {
  it('finds drop', () => { const s = Array(30).fill(0).map((_, i) => 9600 + (i % 2 ? 20 : -20)); s.push(5000); expect(zAnomaly(s).score).toBeLessThan(-3); });
  it('none on steady', () => expect(zAnomaly(Array(30).fill(1))).toBeNull());
});

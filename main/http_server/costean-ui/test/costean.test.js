import { describe, it, expect } from 'vitest';
import * as C from '../src/lib/costean.js';
describe('schedule helpers', () => {
  it('formats and parses times', () => { expect(C.hhmm(0)).toBe('00:00'); expect(C.hhmm(1439)).toBe('23:59'); expect(C.parseHHMM('7:05')).toBe(425); expect(C.parseHHMM('24:00')).toBe(null); expect(C.parseHHMM('x')).toBe(null); });
  it('labels day masks', () => { expect(C.daysLabel(0x7f)).toBe('Every day'); expect(C.daysLabel(0x3e)).toBe('Weekdays'); expect(C.daysLabel(0x41)).toBe('Weekends'); expect(C.daysLabel(0b10)).toBe('Mon'); expect(C.toggleDay(0x7f, 0)).toBe(0x7e); });
  it('validates like the firmware', () => {
    const w = C.blankWindow(); expect(C.validateWindow(w)).toBe(null);
    expect(C.validateWindow({ ...w, days: 0 })).toMatch(/day/);
    expect(C.validateWindow({ ...w, profile: 'none', lcd: 'normal' })).toMatch(/nothing/);
    expect(C.validateWindow({ ...w, start: 1440 })).toMatch(/times/);
    expect(C.validateSchedule(Array(9).fill(w))).toMatch(/8/);
    expect(C.validateWindow({ ...w, start: 600, end: 600 })).toBe(null); expect(C.windowMinutes({ start: 600, end: 600 })).toBe(1440);
  });
  it('overnight windows belong to their start day', () => {
    const w = { on: true, days: 1 << 5, start: 22 * 60, end: 7 * 60 }; // Friday night
    expect(C.activeAt(w, 5, 23 * 60)).toBe(true); expect(C.activeAt(w, 6, 6 * 60)).toBe(true);
    expect(C.activeAt(w, 6, 23 * 60)).toBe(false); expect(C.activeAt(w, 5, 6 * 60)).toBe(false);
    expect(C.activeAt({ ...w, on: false }, 5, 23 * 60)).toBe(false);
  });
  it('week grid: lowest power profile and darkest screen win', () => {
    const g = C.weekGrid([{ on: true, days: 0x7f, start: 0, end: 360, profile: 'turbo', lcd: 'dim' }, { on: true, days: 0x7f, start: 120, end: 240, profile: 'eco', lcd: 'off' }]);
    expect(g[1][0]).toEqual({ profile: 'turbo', lcd: 'dim' }); expect(g[1][2]).toEqual({ profile: 'eco', lcd: 'off' }); expect(g[1][12]).toBe(null);
  });
});
describe('pool split', () => {
  it('largest remainder sums to 100', () => { expect(C.largestRemainder([33.3, 33.3, 33.4])).toEqual([33, 33, 34]); expect(C.largestRemainder([100 / 3, 100 / 3, 100 / 3]).reduce((a, b) => a + b)).toBe(100); });
  it('failover moves a dead pool share to the others in proportion', () => {
    expect(C.effectiveSplit([70, 30], [true, true])).toEqual([70, 30]);
    expect(C.effectiveSplit([70, 30], [false, true])).toEqual([0, 100]);
    expect(C.effectiveSplit([50, 30, 20, 0], [true, false, true, true])).toEqual([71, 0, 29, 0]);
    expect(C.effectiveSplit([100, 0], [false, true])).toEqual([0, 100]);
    expect(C.effectiveSplit([60, 40], [false, false])).toEqual([0, 0]);
  });
  it('builds pool rows with reject %', () => {
    const r = C.poolRows({ pools: [{ host: 'a.example', connected: true, splitWeight: 70, splitEffective: 70, accepted: 99, rejected: 1, assignedHashrate: 7000 }, { host: 'b.example', connected: false, accepted: 0, rejected: 0 }] }, { poolBalance: 70 });
    expect(r[0].rej).toBeCloseTo(1); expect(r[1].weight).toBe(30); expect(r[1].healthy).toBe(false);
  });
});
describe('rollup history', () => {
  it('decodes points, gaps and flags', () => {
    const { points, gaps } = C.decodeRollup({ slotSec: 900, start: 1000, ghs: [11000, null, 9000], watts: [180, null, 150], chipTemp: [60, null, 58], vrTemp: [64, null, 62], jth: [16, null, 17], flags: [1, 0, 6] });
    expect(points.length).toBe(3); expect(gaps).toBe(1); expect(points[1].t).toBe(1000e3 + 900e3); expect(points[0].ths).toBe(11); expect(points[0].throttled).toBe(true); expect(points[2].failover && points[2].scheduled).toBe(true);
    const s = C.rollupSummary(points); expect(s.hours).toBe(0.5); expect(s.avgThs).toBe(10); expect(s.kwh).toBeCloseTo(0.0825); expect(s.throttledH).toBe(0.25);
  });
  it('handles empty input', () => { expect(C.decodeRollup(null).points).toEqual([]); expect(C.rollupSummary([])).toBe(null); });
  it('governor text', () => {
    expect(C.governorText({ governor: { mining: true, state: 'throttled', reason: 'chip 69C', throttleSteps: 2 }, envelope: { stepMhz: 25 } }).text).toMatch(/50 MHz/);
    expect(C.governorText({ governor: { mining: false } }).level).toBe('mute');
  });
});

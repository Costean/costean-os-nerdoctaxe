import { describe, it, expect } from 'vitest';
import { errPct, errLevel, errSummary } from '../src/lib/errors.js';
describe('asic errors', () => {
  it('pct', () => { expect(errPct(990, 10)).toBeCloseTo(1); expect(errPct(0, 0)).toBe(0); });
  it('levels', () => { expect(errLevel(0.5)).toBe('ok'); expect(errLevel(1)).toBe('warn'); expect(errLevel(2.5)).toBe('crit'); });
  it('summary', () => { const s = errSummary([{ hashRate: 1000, errorHashRate: 10 }, { hashRate: 1000, errorHashRate: 30 }]); expect(s.err).toBe(40); expect(s.pct).toBeCloseTo(40 / 2040 * 100); });
});

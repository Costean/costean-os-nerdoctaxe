import { describe, it, expect } from 'vitest';
import { parse } from '../src/lib/commands.js';
import { settings } from './fixtures.js';
describe('palette', () => {
  it('fans %', () => { const p = parse('fans 70%', settings); expect(p.kind).toBe('write'); expect(p.patch.fans[0]).toEqual({ mode: 0, manualSpeed: 70 }); expect(p.aboveStock).toBe(false); });
  it('clamps fans', () => expect(parse('set fan to 150', settings).patch.fans[0].manualSpeed).toBe(100));
  it('fans auto', () => expect(parse('fans auto', settings).patch.fans[0].mode).toBe(2));
  it('frequency above stock needs phrase', () => { const p = parse('frequency 700', settings); expect(p.aboveStock).toBe(true); });
  it('voltage outside limits blocked', () => expect(parse('voltage 1400', settings).blocked).toMatch(/limits/));
  it('eco and stock presets', () => { expect(parse('eco', settings).patch).toEqual({ frequency: 450, coreVoltage: 1100 }); expect(parse('stock', settings).patch).toEqual({ frequency: 525, coreVoltage: 1150 }); });
  it('nav', () => { expect(parse('pools').to).toBe('pools'); expect(parse('block').to).toBe('block'); expect(parse('safe', settings).patch).toEqual({ frequency: 700, coreVoltage: 1150 }); });
  it('actions', () => { expect(parse('restart').action).toBe('restart'); expect(parse('shutdown').danger).toBe(true); });
  it('ui', () => expect(parse('light theme').action).toBe('theme'));
  it('unknown', () => expect(parse('make coffee')).toBeNull());
});

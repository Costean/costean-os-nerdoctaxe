// ASIC hardware-error helpers (pure). hashRate / errorHashRate in GH/s from performance.chips[i].
export const errPct = (gh, err) => (gh + err) > 0 ? err / (gh + err) * 100 : 0;
export const errLevel = pct => pct > 2 ? 'crit' : pct >= 1 ? 'warn' : 'ok';
export function errSummary(chips = []) {
  const gh = chips.reduce((a, c) => a + (c.hashRate || 0), 0), err = chips.reduce((a, c) => a + (c.errorHashRate || 0), 0);
  return { gh, err, pct: errPct(gh, err) };
}

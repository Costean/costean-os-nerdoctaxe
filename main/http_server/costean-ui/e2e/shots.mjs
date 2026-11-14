// Costean OS screenshots + smoke checks against the DEMO mock (never a real miner).
import { chromium } from '/node_modules/playwright-core/index.mjs';
import { spawn } from 'node:child_process'; import fs from 'node:fs';
const OUT = process.env.SCREENS || 'screens'; fs.mkdirSync(OUT, { recursive: true });
let fail = 0; const ok = (c, m) => { console.log(c ? '  ✓' : '  ✗', m); if (!c) fail++; };
async function run(scenario, port, shots) {
  const srv = spawn('node', ['e2e/mock-server.mjs'], { stdio: 'ignore', env: { ...process.env, PORT: String(port), SCENARIO: scenario } });
  await new Promise(r => setTimeout(r, 800)); const URL = `http://127.0.0.1:${port}/`;
  const browser = await chromium.launch({ executablePath: '/usr/bin/google-chrome', args: ['--no-sandbox'] });
  try {
    const ctx = await browser.newContext({ viewport: { width: 1440, height: 960 }, deviceScaleFactor: 1 });
    const page = await ctx.newPage(); const errs = []; page.on('pageerror', e => errs.push(e.message));
    await page.goto(URL); await page.waitForSelector('[data-testid=demo]'); await page.waitForTimeout(4000);
    for (const [name, hash, check] of shots) {
      await page.goto(URL + '#/' + hash); await page.waitForTimeout(2500);
      if (check) ok(await check(page), `${scenario}: ${name}`);
      await page.screenshot({ path: `${OUT}/${name}.png`, fullPage: true });
    }
    ok(errs.length === 0, `${scenario}: no page errors ${errs.join(' | ')}`);
    const w = await (await fetch(URL + '__writes')).json(); ok(w.length === 0, `${scenario}: no writes sent`);
    await ctx.close();
  } finally { await browser.close(); srv.kill(); }
}
await run('normal', 8097, [
  ['web-01-dashboard', 'dashboard', async p => (await p.textContent('body')).includes('TH/s') && (await p.locator('.chip').count()) === 8 && (await p.locator('.chip .er').count()) === 8],
  ['web-02-profiles-limits', 'tuning', async p => (await p.locator('[data-testid=presets] .preset').count()) === 4 && (await p.textContent('[data-testid=autotune]')).includes('off (default)')],
  ['web-03-pool-split', 'split', async p => (await p.textContent('[data-testid=split-card]')).includes('70%') && (await p.locator('[data-testid^=split-pool-]').count()) === 2],
  ['web-04-schedules', 'schedules', async p => (await p.locator('.win').count()) === 4 && (await p.locator('.week .cell').count()) === 168 && !(await p.locator('[data-testid=sched-err]').count())],
  ['web-05-history', 'history', async p => (await p.textContent('[data-testid=hist-summary]')).includes('TH/s')],
  ['web-06-mining-manual', 'mining', async p => (await p.textContent('[data-testid=safe-card]')).includes('active now') && (await p.locator('[data-testid=asic-errors] .ec').count()) === 8 && (await p.textContent('[data-testid=live-err]')).includes('%')],
  ['web-07-block-found', 'block', async p => (await p.textContent('[data-testid=block-card]')).includes('HUNTING')],
  ['web-08-thermal', 'thermal', null], ['web-09-pools', 'pools', null], ['web-10-swarm', 'swarm', null],
  ['web-11-influx', 'influx', null], ['web-12-network', 'network', null], ['web-13-diagnostics', 'diagnostics', null], ['web-14-update', 'update', null],
]);
await run('failover', 8095, [['web-03b-pool-split-failover', 'split', async p => (await p.textContent('[data-testid=failover-badge]')).includes('failover') && (await p.textContent('[data-testid=split-pool-1]')).includes('down')]]);
await run('throttle', 8094, [['web-02b-profiles-throttled', 'tuning', async p => (await p.textContent('[data-testid=gov-card]')).includes('Throttling')], ['web-01b-dashboard-throttled', 'dashboard', async p => (await p.textContent('[data-testid=mode-pill]')).includes('THROTTLED')]]);
await run('block', 8096, [['web-07b-block-candidate', 'block', async p => (await p.textContent('[data-testid=block-card]')).includes('BLOCK CANDIDATE')]]);
console.log(fail ? `FAIL ${fail}` : 'ALL OK'); process.exit(fail ? 1 : 0);

// Stamp the web UI build with the firmware version string from <repo>/version.txt
import fs from 'node:fs';
const v = fs.readFileSync(new URL('../../../../version.txt', import.meta.url), 'utf8').trim();
fs.writeFileSync(new URL('../public/version.json', import.meta.url), JSON.stringify({ www: v, ui: 'Costean OS control center', base: 'shufps/ESP-Miner-NerdQAxePlus v1.1.2' }) + '\n');
console.log('www version', v);

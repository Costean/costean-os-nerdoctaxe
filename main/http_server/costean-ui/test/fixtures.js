// DEMO data shaped like /api/v2/dashboard and /api/v2/settings of shufps v1.1.2
export const settings = { deviceModel: 'NerdOCTAXE-Gamma', asicModel: 'BM1370', version: 'nerdoctaxe-1.1.1-ui1', frequency: 525, coreVoltage: 1150, defaultFrequency: 525, defaultCoreVoltage: 1150, ecoFrequency: 450, ecoCoreVoltage: 1100, absMinCoreVoltage: 1000, absMaxCoreVoltage: 1300,
  hostname: 'nerdoctaxe', ssid: 'DEMO-WIFI', flipScreen: 1, invertScreen: 0, autoScreenOff: 0, invertFanPolarity: 1, stratumKeep: 0, poolMode: 0, poolBalance: 50, can: { hasExtension: false, enabled: false },
  pools: [{ url: 'public-pool.io', port: 21496, user: 'bc1qdemo.oct', tls: true, protocol: 0 }, { url: 'solo.ckpool.org', port: 3333, user: 'bc1qdemo.oct', tls: false, protocol: 0 }],
  fans: [{ label: 'Fan 1', mode: 2, manualSpeed: 100, overheatTemp: 70, pid: { targetTemp: 55, p: 6, i: 0.1, d: 10 } }] };
export const dash = (o = {}) => ({ system: { uptime: 3600, shutdown: false, boardError: 0, overheatTemp: 70 },
  performance: { hashRate: 9600, hashRate1h: 9550, sharesAccepted: 1000, sharesRejected: 2, frequency: 525, asicCount: 8, chips: Array.from({ length: 8 }, () => ({ hashRate: 1200, errorHashRate: 2 })) },
  power: { watts: 150, voltage: 12.1, voltageMin: 11.5 }, thermal: { asicTemp: 58, vrTemp: 60, asicTemps: [55, 56, 57, 58, 56, 55, 57, 58], fans: [{ speed: 60, rpm: 4200 }] },
  stratum: { usingFallback: false, pools: [{ connected: true, pingRtt: 30 }, { connected: false }] }, ...o });

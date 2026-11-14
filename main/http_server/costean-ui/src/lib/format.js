export const fmtHash = gh => gh == null || isNaN(gh) ? '—' : gh >= 1000 ? (gh / 1000).toFixed(2) + ' TH/s' : gh.toFixed(1) + ' GH/s';
export const fmtDiff = d => { if (!d) return '0'; const u = ['', 'K', 'M', 'G', 'T', 'P', 'E']; let i = 0; while (d >= 1000 && i < u.length - 1) { d /= 1000; i++; } return d.toFixed(i ? 2 : 0) + u[i]; };
export const fmtUptime = s => { s = Math.floor(s || 0); const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60); return (d ? d + 'd ' : '') + h + 'h ' + m + 'm'; };
export const fmtBytes = b => b >= 1048576 ? (b / 1048576).toFixed(2) + ' MB' : (b / 1024).toFixed(0) + ' KB';
/** J/TH from watts and GH/s */
export const efficiency = (w, gh) => (gh > 0 && w > 0) ? w / (gh / 1000) : null;
export const BOARD_ERRORS = ['', 'Miner overheated', 'Voltage regulator overheated', 'PSU fault', 'Current protection', 'Core voltage fault', 'Coinbase verification fault'];
export const rssiLabel = r => r == null ? '—' : r > -55 ? 'Excellent' : r > -67 ? 'Good' : r > -75 ? 'Fair' : 'Weak';

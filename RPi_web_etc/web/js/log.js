/* ─── log.js ─────────────────────────────────────────────────────── */
/* Jednoduchý in-memory log systém                                   */

let logs = [];
const MAX_LOGS = 100;

export function addLog(msg, type = 'info') {
  const log = {
    ts: Date.now(),
    msg,
    type, // 'info', 'warn', 'err', 'ok'
  };
  logs.push(log);
  if (logs.length > MAX_LOGS) logs.shift();
  
  // Zobrazí také jako toast pro okamžitou informaci
  import('./notify.js').then(m => {
    const titles = { info: 'Info', warn: 'Varování', err: 'Chyba', ok: 'Úspěch' };
    m.showToast(titles[type] || 'Systém', msg, type, type === 'err' ? 10000 : 5000);
  });

  // Pokud je v admin sekci, hned refresh
  import('./state.js').then(m => {
    if (m.selectedId === 'SYSTEM') {
      import('./panel.js').then(p => p.refreshSystemView());
    }
  });
}

export function clearLog() {
  logs = [];
  import('./panel.js').then(p => p.refreshSystemView());
}

export function getLogs() {
  return logs;
}

export function getLogCount() {
  return logs.length;
}

export function getLogHTML() {
  if (logs.length === 0) return '<div style="opacity:0.5;padding:10px;">Žádné zprávy.</div>';
  return logs.map(l => {
    let color = 'var(--text)';
    if (l.type === 'warn') color = 'var(--amber)';
    if (l.type === 'err')  color = 'var(--red)';
    if (l.type === 'ok')   color = 'var(--green)';
    
    const time = new Date(l.ts).toLocaleTimeString('cs-CZ');
    return `<div style="padding:4px 0;border-bottom:1px solid var(--surface2);line-height:1.4;">
      <span style="color:var(--muted);font-size:0.65rem;font-family:var(--mono);">[${time}]</span>
      <span style="color:${color};font-size:0.75rem;">${l.msg}</span>
    </div>`;
  }).join('');
}

/* ─── charts.js ──────────────────────────────────────────────────── */
/* Správa Chart.js grafů                                             */

import {
  charts, history, sensors,
  globalRangeMin, syncCharts, chartRanges, setChartRanges, setGlobalRangeMin,
  selectedId, selectedType, COLORS, CFILL,
} from './state.js';
import { stypeInfo, fmtTime } from './helpers.js';
import { loadHistory, fetchAllSparklines } from './influx.js';
import { renderSensorSidebar } from './sidebar.js';

// Držíme reference na zvětšený graf
let largeChart         = null;
export let activeLargeChartKey = null;

/** Zničí všechny aktivní grafy */
export function destroyCharts() {
  Object.values(charts).forEach(c => c.destroy());
  for (const k in charts) delete charts[k];
}

/* ─── Tlačítka rozsahu ───────────────────────────────────────────── */
export function rangeButtons(activeMin, key) {
  const ranges = [
    { v: 10,    l: '10m' },
    { v: 30,    l: '30m' },
    { v: 60,    l: '1h'  },
    { v: 360,   l: '6h'  },
    { v: 1440,  l: '24h' },
    { v: 10080, l: '7d'  },
    { v: 43200, l: '30d' },
    { v: 0,     l: 'Vše' },
  ];
  return ranges
    .map(r => `<button class="rng-btn ${activeMin == r.v ? 'active' : ''}" onclick="App.setRange(${r.v},'${key}')">${r.l}</button>`)
    .join('');
}

/** Změní časový rozsah grafů */
export function setRange(minutes, key) {
  if (syncCharts) {
    setGlobalRangeMin(minutes);
    setChartRanges({});
    fetchAllSparklines();
  } else {
    const updated = { ...chartRanges, [key]: minutes };
    setChartRanges(updated);
  }

  if (selectedId && selectedType === 'sensor') {
    if (syncCharts) {
      document.querySelectorAll('.chart-btns').forEach(cb => {
        const k = cb.id.replace('btns_', '');
        cb.innerHTML = rangeButtons(globalRangeMin, k);
      });
      loadHistory(selectedId, globalRangeMin);
    } else {
      const cb = document.getElementById('btns_' + key);
      if (cb) cb.innerHTML = rangeButtons(minutes, key);
      loadHistory(selectedId, minutes, key);
    }
  }

  // Obnov tlačítka v otevřeném zvětšeném grafu
  if (activeLargeChartKey !== null) {
    const lcb = document.getElementById('largeChartBtns');
    if (lcb) {
      const rMin = syncCharts ? globalRangeMin : (chartRanges[activeLargeChartKey] ?? globalRangeMin);
      lcb.innerHTML = rangeButtons(rMin, activeLargeChartKey);
    }
    // Refresh dat ve velkém grafu
    const title = document.getElementById('chartModalTitle')?.textContent || '';
    const label = title.replace('Graf: ', '');
    showLargeChart(activeLargeChartKey, label, true);
  }
}

/** Přepne synchronizaci rozsahů grafů */
export function toggleSync() {
  const newSync = !syncCharts;
  import('./state.js').then(({ setSyncCharts, setChartRanges }) => {
    setSyncCharts(newSync);
    if (newSync) setChartRanges({});
    const btn = document.getElementById('syncToggleBtn');
    if (btn) {
      btn.classList.toggle('active', newSync);
      btn.textContent  = newSync ? 'ZAPNUTÁ' : 'VYPNUTÁ';
      btn.style.background = newSync ? 'var(--blue)' : 'transparent';
      btn.style.color      = newSync ? '#fff' : 'var(--muted)';
    }
  });
}

/* ─── Sestavení grafů pro senzor ────────────────────────────────── */
export function buildCharts(id) {
  const hist = history[id] || {};
  const s    = sensors[id];
  const info = stypeInfo(s?.stype);

  function buildOne(key, label, unit, colorHex, fillRgba, yOpts = {}) {
    const canvas = document.getElementById(`chart_${key}`);
    if (!canvas) return;

    const rMin   = syncCharts ? globalRangeMin : (chartRanges[key] ?? globalRangeMin);
    const cutoff = rMin > 0 ? Date.now() - rMin * 60000 : 0;
    const pts    = (hist[key] || []).filter(p => p.t >= cutoff);
    const ctx    = canvas.getContext('2d');

    let gradient = fillRgba;
    try {
      gradient = ctx.createLinearGradient(0, 0, 0, 200);
      gradient.addColorStop(0, fillRgba);
      gradient.addColorStop(1, 'transparent');
    } catch { /* nevadí */ }

    charts[key] = new Chart(ctx, {
      type: 'line',
      data: {
        labels:   pts.map(p => fmtTime(p.t)),
        datasets: [{
          data:            pts.map(p => p.v),
          borderColor:     colorHex,
          backgroundColor: gradient,
          borderWidth:     2,
          pointRadius:     0,
          pointHoverRadius: 4,
          tension:         0.4,
          fill:            true,
        }],
      },
      options: {
        animation:           false,
        responsive:          true,
        maintainAspectRatio: false,
        interaction:         { intersect: false, mode: 'index' },
        plugins: {
          legend: { display: false },
          tooltip: {
            enabled: true,
            backgroundColor: 'rgba(15,23,42,0.92)',
            titleFont:  { size: 11, family: "'IBM Plex Sans', sans-serif" },
            bodyFont:   { size: 12, family: "'IBM Plex Mono', monospace" },
            padding: 8, cornerRadius: 8,
            callbacks: { label: ctx => ` ${ctx.parsed.y.toFixed(2)}` },
          },
        },
        scales: {
          x: {
            ticks: {
              maxTicksLimit: 5, autoSkip: true, maxRotation: 0,
              color: getComputedStyle(document.documentElement).getPropertyValue('--chart-tick').trim() || '#94a3b8',
              font: { size: 9, family: "'IBM Plex Mono', monospace" },
            },
            grid: { display: false },
            border: { display: false },
          },
          y: {
            ticks: {
              color: getComputedStyle(document.documentElement).getPropertyValue('--chart-tick').trim() || '#94a3b8',
              font: { size: 9, family: "'IBM Plex Mono', monospace" },
              maxTicksLimit: 5,
            },
            grid: {
              color: getComputedStyle(document.documentElement).getPropertyValue('--chart-grid').trim() || 'rgba(203,213,225,0.5)',
              lineWidth: 1,
            },
            border: { display: false, dash: [3, 3] },
            ...yOpts,
          },
        },
      },
    });
  }

  if (s?._floats) {
    s._floats.forEach((_, i) => buildOne(i, info.labels[i] || `val${i}`, info.units[i] || '', COLORS[i % 5], CFILL[i % 5]));
  }
  buildOne('rssi',      'RSSI',          'dBm', '#ef4444', 'rgba(239,68,68,0.2)',    { suggestedMax: 0, max: 0 });
  buildOne('active_us', 'Aktivní čas',   'ms',  '#6366f1', 'rgba(99,102,241,0.2)',   { suggestedMin: 0 });
  buildOne('batt',      'Baterie',       'V',   '#f59e0b', 'rgba(245,158,11,0.2)',   { suggestedMin: 3 });
}

/* ─── Zvětšený modální graf ──────────────────────────────────────── */
export function showLargeChart(key, label, isRefresh = false) {
  const modal    = document.getElementById('chartModal');
  const title    = document.getElementById('chartModalTitle');
  const canvas   = document.getElementById('largeChartCanvas');
  const btns     = document.getElementById('largeChartBtns');
  activeLargeChartKey = key;

  // Určení jednotky
  const unitsMap = { rssi: 'dBm', batt: 'V', active_us: 'ms' };
  let unit = unitsMap[key] || '';
  if (key === 'gw_link') unit = ''; 
  else if (!unit && sensors[selectedId]) {
    const info = stypeInfo(sensors[selectedId].stype);
    unit = info.units[parseInt(key)] || '';
  }

  title.textContent = (key === 'gw_link') ? `Graf: Konektivita (WiFi vs NB-IoT)` : `Graf: ${label}`;
  const rMin = syncCharts ? globalRangeMin : (chartRanges[key] ?? globalRangeMin);
  btns.innerHTML    = rangeButtons(rMin, key);

  if (largeChart) largeChart.destroy();
  const sourceChart = charts[key];
  if (!sourceChart) return;

  const ctx      = canvas.getContext('2d');
  const dataCopy = JSON.parse(JSON.stringify(sourceChart.data));
  dataCopy.datasets[0].label = (key === 'gw_link') ? 'Konektivita' : (unit ? `${label} (${unit})` : label);

  // Velké plátno
  const gradient = ctx.createLinearGradient(0, 0, 0, 400);
  const color    = sourceChart.data.datasets[0].borderColor;
  let rgba       = 'rgba(37,99,235,0.15)';
  if (color.startsWith('#')) {
    const r = parseInt(color.slice(1, 3), 16);
    const g = parseInt(color.slice(3, 5), 16);
    const b = parseInt(color.slice(5, 7), 16);
    rgba = `rgba(${r},${g},${b},0.15)`;
  }
  gradient.addColorStop(0, rgba);
  gradient.addColorStop(1, 'rgba(0,0,0,0)');
  dataCopy.datasets[0].backgroundColor = gradient;
  dataCopy.datasets[0].pointHoverRadius = 5;

  const isDark   = document.documentElement.getAttribute('data-theme') === 'dark';
  const gridClr  = isDark ? 'rgba(48,54,61,0.8)' : 'rgba(203,213,225,0.5)';
  const tickClr  = isDark ? '#6e7681' : '#94a3b8';
  const tooltipBg = isDark ? 'rgba(22,27,34,0.95)' : 'rgba(15,23,42,0.92)';
  const isMobile = window.innerWidth <= 768;

  largeChart = new Chart(ctx, {
    type: 'line',
    data: dataCopy,
    options: {
      animation:           { duration: 300 },
      maintainAspectRatio: false,
      interaction:         { intersect: false, mode: 'index' },
      plugins: {
        legend: {
          display: true, position: 'top',
          labels: { boxWidth: 12, font: { size: 12, weight: 600 }, color: tickClr },
        },
        tooltip: {
          enabled:    true,
          backgroundColor: tooltipBg,
          titleFont:  { size: 12, family: "'IBM Plex Sans', sans-serif" },
          bodyFont:   { size: 13, family: "'IBM Plex Mono', monospace" },
          padding:    12, cornerRadius: 10,
          callbacks:  { 
            title: (items) => (key === 'gw_link') ? fmtTime(items[0].parsed.x) : items[0].label,
            label: (ctx) => {
              if (key === 'gw_link') return ` Link: ${ctx.parsed.y === 1 ? 'WiFi' : 'NB-IoT'}`;
              return ` ${ctx.dataset.label}: ${ctx.parsed.y.toFixed(2)}`;
            }
          },
        },
      },
      scales: {
        x: {
          ...sourceChart.options.scales.x,
          ticks: {
            maxTicksLimit: isMobile ? 5 : 10, autoSkip: true, maxRotation: 0,
            color: tickClr, font: { size: 10, family: "'IBM Plex Mono', monospace" },
            callback: (val) => (key === 'gw_link') ? fmtTime(val) : sourceChart.options.scales.x.ticks.callback(val)
          },
          grid:   { display: false },
          border: { display: false },
        },
        y: {
          ...sourceChart.options.scales.y,
          ticks: {
            color: tickClr, font: { size: 10, family: "'IBM Plex Mono', monospace" },
            maxTicksLimit: 8,
            callback: (val) => {
              if (key === 'gw_link') return val === 1 ? 'WiFi' : (val === 0 ? 'NB-IoT' : '');
              return val;
            }
          },
          grid:   { color: gridClr, lineWidth: 1 },
          border: { display: false, dash: [3, 3] },
        },
      },
    },
  });

  if (!isRefresh) modal.classList.add('visible');
}

/** Zavře modální graf */
export function hideChartModal() {
  const modal = document.getElementById('chartModal');
  if (modal) modal.classList.remove('visible');
  activeLargeChartKey = null;
}

/* ─── Brána Link Graf ─────────────────────────────────────────── */
let gwChart = null;

export function updateGwChart() {
  const canvas = document.getElementById('chart_gw_link');
  // Pokud nemáme canvas, smazat graf a skončit
  if (!canvas) {
    if (gwChart) { gwChart.destroy(); gwChart = null; }
    return;
  }

  // Pokud jsem na jiném detailu než Brána/System, graf neaktualizuju 
  if (selectedId !== 'GATEWAY' && selectedId !== 'SYSTEM') return;

  const hist = history['GATEWAY']?.['link'] || [];
  if (hist.length === 0) return;

  const ctx = canvas.getContext('2d');
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  const gridClr = isDark ? 'rgba(48,54,61,0.8)' : 'rgba(203,213,225,0.5)';
  const tickClr = isDark ? '#6e7681' : '#94a3b8';

  // Pokud už graf existuje, ale na jiném canvasu, zničit ho
  if (charts['gw_link'] && charts['gw_link'].ctx.canvas !== canvas) {
    charts['gw_link'].destroy();
    delete charts['gw_link'];
  }

  if (!charts['gw_link']) {
    canvas.onclick = () => showLargeChart('gw_link', 'Konektivita');
    charts['gw_link'] = new Chart(ctx, {
      type: 'line',
      data: {
        labels: hist.map(p => p.t),
        datasets: [{
          label: 'Konektivita',
          data: hist.map(p => p.v),
          borderColor: '#6366f1',
          backgroundColor: 'rgba(99,102,241,0.1)',
          borderWidth: 2,
          fill: true,
          stepped: true,
          pointRadius: 0,
          spanGaps: true,
          borderJoinStyle: 'round'
        }]
      },
      options: {
        animation: false,
        responsive: true,
        maintainAspectRatio: false,
        layout: {
          padding: { bottom: 12 }
        },
        plugins: {
          legend: { display: false },
          tooltip: {
            callbacks: {
              title: (items) => fmtTime(items[0].parsed.x),
              label: (ctx) => ` Link: ${ctx.parsed.y === 1 ? 'WiFi' : 'NB-IoT'}`
            }
          }
        },
        scales: {
          x: {
            type: 'linear',
            ticks: { 
              color: tickClr, font: { size: 9 }, maxTicksLimit: 5,
              callback: (val) => fmtTime(val)
            },
            grid: { display: false }
          },
          y: {
            min: -0.3, max: 1.3,
            ticks: {
              color: tickClr,
              font: { size: 9 },
              stepSize: 1,
              callback: (val) => val === 1 ? 'WiFi' : (val === 0 ? 'NB-IoT' : '')
            },
            grid: { color: gridClr }
          }
        }
      }
    });
  } else {
    charts['gw_link'].data.labels = hist.map(p => p.t);
    charts['gw_link'].data.datasets[0].data = hist.map(p => p.v);
    charts['gw_link'].update('none');
  }
}

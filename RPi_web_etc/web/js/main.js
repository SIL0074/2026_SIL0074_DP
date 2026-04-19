/* ─── main.js ────────────────────────────────────────────────────── */
/* Hlavní vstupní bod aplikace, správa UI a navigace                 */

import * as State from './state.js';
import { 
  setSelected, setActiveTab, setMsgCount, saveNodeConfig,
  currentUser, setCurrentUser, setCurrentUserIp, nodeAliases, nodeGroups
} from './state.js';
import { 
  renderGwSidebar, renderSensorSidebar, renderRouterSidebar,
  toggleSidebar, closeSidebar, navAndClose,
} from './sidebar.js';
import { connect } from './mqtt.js';
import { 
  fetchHistoricalNodes, fetchNodeConfigs, saveNodeConfigGlobal,
  fetchIpHistory
} from './influx.js';
import { showToast } from './notify.js';

import { 
  handleUserClick, updateUserUI
} from './auth.js';

import { exportNodeCSV, exportNodeJSON } from './export.js';
import { showLargeChart, hideChartModal, setRange, toggleSync } from './charts.js';

// Dynamické importy pro úsporu místa
const getPanel = () => import('./panel.js?t=' + Date.now());

/* ─── Přepínání témat ───────────────────────────────────────────── */
function applyTheme(theme) {
  document.documentElement.setAttribute('data-theme', theme);
  const icon = document.getElementById('themeIcon');
  if (icon) {
    if (theme === 'dark') {
      icon.innerHTML = '<circle cx="12" cy="12" r="5"/><path d="M12 1v2m0 18v2M4.22 4.22l1.42 1.42m12.72 12.72l1.42 1.42M1 12h2m18 0h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/>';
      icon.style.color = '#f59e0b';
      icon.style.fill = '#f59e0b';
    } else {
      icon.innerHTML = '<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>';
      icon.style.color = '#64748b';
      icon.style.fill = '#64748b';
    }
  }
}

function toggleTheme() {
  const current = document.documentElement.getAttribute('data-theme') || 'light';
  const next = current === 'light' ? 'dark' : 'light';
  localStorage.setItem('theme', next);
  applyTheme(next);
}

/* ─── Hodiny a Uptime ───────────────────────────────────────────── */
setInterval(() => {
  const now = new Date();
  const clock = document.getElementById('liveClock');
  if (clock) clock.textContent = now.toLocaleTimeString('cs-CZ', { hour12: false });

  const uptimeEl = document.getElementById('netUptime');
  if (uptimeEl) {
    const s = State.gateway?.uptime_s;
    if (s) {
      const d = Math.floor(s / 86400);
      const h = Math.floor((s % 86400) / 3600);
      const m = Math.floor((s % 3600) / 60);
      uptimeEl.textContent = `Síť běží: ${d > 0 ? d + 'd ' : ''}${h}h ${m}m`;
    } else {
      uptimeEl.textContent = `Síť běží: —`;
    }
  }
}, 1000);

/* ─── Přejmenování uzlů ─────────────────────────────────────────── */
let currentRenameNodeId = null;
let currentRenameNodeType = null;

function showRenameModal(id, type = 'sensor') {
  currentRenameNodeId = id;
  currentRenameNodeType = type;
  const modal = document.getElementById('renameModal');
  const input = document.getElementById('renameInput');
  const grpInput = document.getElementById('renameGroupInput');
  const label = document.getElementById('renameIdLabel');

  if (label) label.textContent = `ID: ${id}`;
  if (input) input.value = nodeAliases[id] || '';
  if (grpInput) grpInput.value = nodeGroups[id] || '';
  
  if (modal) modal.classList.add('visible');
  if (input) input.focus();
}

function hideRenameModal() {
  const modal = document.getElementById('renameModal');
  if (modal) modal.classList.remove('visible');
}

async function submitRename() {
  if (!currentRenameNodeId) return;
  const id = currentRenameNodeId;
  const type = currentRenameNodeType;
  const name = document.getElementById('renameInput').value.trim();
  const group = document.getElementById('renameGroupInput').value.trim();

  saveNodeConfig(id, name, group); 
  _applyRenameChanges(id, type);
  hideRenameModal();

  const success = await saveNodeConfigGlobal(id, name, group);
  if (success) {
    showToast('Uloženo', `Konfigurace uzlu ${id} byla aktualizována.`, 'ok');
  } else {
    showToast('Chyba', 'Konfigurace uložena jen lokálně (chyba InfluxDB).', 'warn');
  }
}

async function resetRename() {
  if (!currentRenameNodeId) return;
  const id = currentRenameNodeId;
  const type = currentRenameNodeType;
  
  saveNodeConfig(id, '', ''); 
  _applyRenameChanges(id, type);
  hideRenameModal();

  const success = await saveNodeConfigGlobal(id, '', '');
  if (success) {
    showToast('Resetováno', `Uzel ${id} používá výchozí nastavení`, 'info');
  } else {
    showToast('Chyba', 'Reset proběhl jen lokálně (chyba InfluxDB).', 'warn');
  }
}

function _applyRenameChanges(id, type) {
  renderSensorSidebar();
  renderRouterSidebar();
  getPanel().then(m => {
    if (State.selectedId === 'SYSTEM') m.refreshSystemView();
    if (State.selectedType === 'overview') m.refreshNetworkOverview();
    if (State.selectedId === id) m.showDetail(id, type);
  });
}

/* ─── Veřejné API pro window.App ─────────────────────────────────── */
window.App = {
  navAndClose: (fn) => navAndClose(fn),
  showNetworkOverview: () => {
    setSelected('OVERVIEW', 'overview');
    setActiveTab('dashboard');
    getPanel().then(m => {
      const p = document.getElementById('panel');
      if (p) p.innerHTML = m.buildNetworkOverview();
      if (m.initTopologyGraph) m.initTopologyGraph();
      if (m.refreshNetworkOverview) m.refreshNetworkOverview();
      window._panel = m; 
    });
  },
  showSystemView: () => {
    setSelected('SYSTEM', 'system');
    setActiveTab('dashboard');
    getPanel().then(m => {
      const p = document.getElementById('panel');
      if (p) p.innerHTML = m.buildSystemView();
      if (m.refreshSystemView) m.refreshSystemView();
      window._panel = m;
    });
  },
  showDetail: (id, type) => {
    setSelected(id, type);
    getPanel().then(m => {
      if (m.showDetail) m.showDetail(id, type);
      window._panel = m;
    });
  },
  
  showTopoTip: (el, e, d) => window._panel?.showTopoTip?.(el, e, d),
  hideTopoTip: () => window._panel?.hideTopoTip?.(),
  
  refreshSensorDetail: (id) => window._panel?.refreshSensorDetail?.(id),
  refreshRouterDetail: (id) => window._panel?.refreshRouterDetail?.(id),
  refreshGatewayDetail: () => window._panel?.refreshGatewayDetail?.(),
  refreshNetworkOverview: () => window._panel?.refreshNetworkOverview?.(),
  refreshNodeWidget: () => window._panel?.refreshNodeWidget?.(),
  refreshSystemView: () => window._panel?.refreshSystemView?.(),
  
  initTopologyGraph: () => window._panel?.initTopologyGraph?.(),
  drawTopologySVG: () => window._panel?.drawTopologySVG?.(),
  triggerPacketAnimation: (id) => window._panel?.triggerPacketAnimation?.(id),

  switchTab: (tab, el) => {
    getPanel().then(m => {
      if (m.switchTab) m.switchTab(tab, el);
    });
  },
  
  toggleTheme, toggleSidebar, closeSidebar,
  showRenameModal, hideRenameModal, submitRename, resetRename,
  showToast,
  
  handleUserClick,
  updateUserUI,

  exportNodeCSV,
  exportNodeJSON,
  showLargeChart,
  hideChartModal,
  setRange,
  toggleSync,

  showNodeWidget: (id, type) => {
    getPanel().then(m => {
      if (m.showNodeWidget) m.showNodeWidget(id, type);
    });
  },
  hideNodeModal: () => {
    getPanel().then(m => {
      if (m.hideNodeModal) m.hideNodeModal();
    });
  },
  closeNodeModal: (e) => {
    if (e.target.id === 'nodeModal') {
      getPanel().then(m => {
        if (m.hideNodeModal) m.hideNodeModal();
      });
    }
  },

  showIpHistory: (ip) => {
    fetchIpHistory(ip).then(rows => {
      showToast("Historie IP", `Nalezeno ${rows.length} záznamů`, "info");
    });
  },
  clearLog: () => {
    import('./log.js').then(m => {
      m.clearLog();
      if (State.selectedId === 'SYSTEM') {
        getPanel().then(p => p.refreshSystemView());
      }
    });
  }
};

/* ─── Inicializace ──────────────────────────────────────────────── */
window.addEventListener('load', () => {
  try {
    const theme = localStorage.getItem('theme') || 'dark';
    applyTheme(theme);
    updateUserUI();

    import('./influx.js').then(m => m.fetchTotalStats()).then(stats => {
      State.setInitialCounts(stats.today, stats.total);
      const todayEl = document.getElementById('hdrTodayCount');
      if (todayEl) todayEl.textContent = stats.today;
      const totalEl = document.getElementById('hdrTotal');
      if (totalEl) totalEl.textContent = stats.total;
    });

    fetchNodeConfigs().then(configs => {
      Object.keys(configs).forEach(id => saveNodeConfig(id, configs[id].alias, configs[id].group));
      renderSensorSidebar();
      renderRouterSidebar();
    });
    
    fetchHistoricalNodes();
    
    getPanel().then(m => {
      m.showDetail('GATEWAY', 'gateway');
      window._panel = m;
    });
    
    setTimeout(connect, 500);

    // Pravidelná kontrola offline uzlů (Sidebar + Topologie)
    setInterval(() => {
      renderSensorSidebar();
      renderRouterSidebar();
      renderGwSidebar();
      
      // Pokud je na přehledu, překreslíme i topologii
      if (State.selectedType === 'overview' && window.App) {
        window.App.refreshNetworkOverview();
      }
    }, 15000); // Každých 15 sekund
  } catch (e) { console.error("App init crash:", e); }
});

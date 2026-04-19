/* ─── auth.js ────────────────────────────────────────────────────── */

import { currentUser, selectedId } from './state.js';
import { logVisit } from './influx.js';
import { t } from './i18n.js';

import { renderSensorSidebar, renderRouterSidebar } from './sidebar.js';

export function handleUserClick() {
}

export function showLoginModal() { /* Odstraněno */ }
export function hideLoginModal() { /* Odstraněno */ }
export async function attemptLogin() { /* Odstraněno */ }
export function logout() { /* Odstraněno */ }
export function showUserModal() { /* Odstraněno */ }
export function hideUserModal() { /* Odstraněno */ }
export async function submitUser() { /* Odstraněno */ }
export async function deleteUser() { /* Odstraněno */ }

export function updateUserUI() {
  const label       = document.getElementById('roleLabel');
  const actionLabel = document.getElementById('loginActionLabel');
  const sysItem     = document.getElementById('sysItem');
  const userPill    = document.getElementById('userPill');

  if (label)        label.textContent = `Administrátor`;
  if (actionLabel)  actionLabel.style.display = 'none'; // Skrýt tlačítko "Přihlásit"
  if (sysItem)      sysItem.style.display     = 'flex'; // Vždy vidět systém
  if (userPill)     userPill.style.cursor     = 'default';

  if (selectedId === 'SYSTEM' && window.App) window.App.refreshSystemView();
  
  renderSensorSidebar();
  renderRouterSidebar();
  logVisit(); // Logování návštěvy i bez přihlášení
}

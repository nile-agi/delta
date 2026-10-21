/* Runs before the app bundle, including on older system WebViews. */
(function () {
  // A first launch can wait on macOS checking the newly installed binaries; a crashed backend still fails at once.
  var STARTUP_MS = 90000;
  var splash = document.getElementById('app-loading');
  var auxiliary = document.documentElement.classList.contains('aux-window');
  var complete = false;
  var deadline = Date.now() + STARTUP_MS;
  var guard = { failure: null, mounted: function () { complete = true; clearTimeout(watchdog); } };
  window.__DELTA_STARTUP__ = guard;
  if (!window.location.hash) history.replaceState(null, '', window.location.pathname + window.location.search + '#/');
  if (auxiliary) { if (splash) splash.remove(); complete = true; return; }
  function fail(message, stage) {
    if (complete || guard.failure) return;
    guard.failure = { message: String(message), stage: stage || 'startup', timestamp: new Date().toISOString() };
    window.__DELTA_SERVER_ERROR__ = true;
    window.__DELTA_MODEL_BACKEND_ERROR__ = String(message);
    if (splash) {
      splash.style.opacity = '1';
      splash.textContent = '';
      var box = document.createElement('div');
      box.style.cssText = 'max-width:36rem;padding:2rem;font-family:system-ui,sans-serif';
      var title = document.createElement('h1'); title.textContent = 'Delta could not start';
      var text = document.createElement('p'); text.textContent = String(message);
      box.appendChild(title); box.appendChild(text); splash.appendChild(box);
      if (window.__DELTA_DIAGNOSTICS__) window.__DELTA_DIAGNOSTICS__.attach(box, guard.failure);
    }
    window.dispatchEvent(new CustomEvent('delta-server-error', { detail: guard.failure }));
  }
  guard.fail = fail;
  var watchdog = setTimeout(function () { fail('Delta could not finish starting. Restart the app or review the diagnostic report.', 'startup-timeout'); }, STARTUP_MS);
  window.addEventListener('error', function (event) { if (event.message) fail(event.message, 'frontend'); });
  window.addEventListener('unhandledrejection', function (event) {
    var reason = event.reason; fail(reason && reason.message ? reason.message : String(reason), 'frontend');
  });
  var label = document.querySelector('#app-loading p');
  if (!window.__TAURI_INTERNALS__) return;
  if (label) label.textContent = 'Starting...';
  function poll() {
    if (complete || guard.failure || Date.now() >= deadline) return;
    window.__TAURI_INTERNALS__.invoke('get_server_status').then(function (status) {
      if (complete || guard.failure) return;
      window.__DELTA_MODEL_BACKEND_ERROR__ = status[4] || '';
      if (status[3]) { fail(status[4] || 'The backend process could not start.', 'backend'); return; }
      if (status[2]) {
        window.__DELTA_PORT__ = status[0]; window.__DELTA_MODEL_API_PORT__ = status[1];
        window.dispatchEvent(new CustomEvent('delta-server-ready'));
      } else setTimeout(poll, 500);
    }).catch(function () { setTimeout(poll, 1000); });
  }
  setTimeout(poll, 300);
})();

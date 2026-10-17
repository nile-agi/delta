/* Shared by Svelte error screens and the pre-bundle startup fallback. No uploads. */
(function () {
  var api = {};
  var last = null;
  api.redact = function (value) {
    return String(value || '')
      .replace(/\b(?:hf_[A-Za-z0-9]{8,}|sk-[A-Za-z0-9_-]{8,})\b/g, '[redacted]')
      .replace(/https?:\/\/[^\s]+/gi, '[redacted]')
      .replace(/\b(?:authorization|bearer|api[_-]?key|token|password|secret)\b[^\r\n]*/gi, '[redacted]')
      .replace(/(?:[a-z]:[\\/]|\/)[^\r\n"'<>]+/gi, '[redacted]');
  };
  function printable(report) {
    var lines = ['# Delta diagnostic report', ''];
    ['version', 'releaseTag', 'commit', 'target', 'cpuPolicy', 'os', 'timestamp', 'stage', 'model', 'exitCode', 'signal', 'message'].forEach(function (key) {
      if (report[key] !== undefined && report[key] !== null) lines.push(key + ': ' + report[key]);
    });
    lines.push('CPU features: ' + (report.cpuFeatures || ['unavailable']).join(', '));
    lines.push('', 'Recent errors:', (report.logs || []).join('\n'));
    return api.redact(lines.join('\n')).slice(0, 90000);
  }
  api.collect = function (context) {
    context = context || {};
    var native = window.__TAURI_INTERNALS__;
    var timer;
    var request = Promise.race([new Promise(function (_, reject) { timer = setTimeout(function () { reject(new Error('Native diagnostics unavailable')); }, 2500); }), Promise.resolve().then(function () { return native ? native.invoke('get_diagnostic_report', {
      stage: context.stage || 'model-load', model: context.model || null,
      error: api.redact(context.message || 'Unknown failure'), exitCode: context.exitCode == null ? null : context.exitCode
    }) : Promise.reject(new Error('Native diagnostics unavailable')); })]);
    return request.then(function (report) { clearTimeout(timer); return report ? printable(report) : null; }).catch(function () {
      clearTimeout(timer);
      if (context.stage === 'previous') return null;
      return printable({ version: 'unavailable (native bridge did not respond)', os: navigator.userAgent,
        stage: context.stage || 'frontend', model: context.model || null,
        message: api.redact(context.message), cpuFeatures: ['unavailable'], logs: [] });
    });
  };
  api.issueUrl = function (text) {
    var body = api.redact(text).replace(/[\uD800-\uDFFF]/g, '?');
    var prefix = 'https://github.com/nile-agi/delta-release/issues/new?title=' + encodeURIComponent('Delta startup or model loading failure') + '&body=';
    var suffix = '\n\n[Full report available through Copy report or Download report in Delta.]';
    while (prefix.length + encodeURIComponent(body + suffix).length > 1800) body = body.slice(0, Math.max(0, body.length - 100));
    return prefix + encodeURIComponent(body + suffix);
  };
  api.openIssue = function (text) {
    var url = api.issueUrl(text);
    if (window.__TAURI_INTERNALS__) return window.__TAURI_INTERNALS__.invoke('plugin:shell|open', { path: url, with: null });
    window.open(url, '_blank', 'noopener,noreferrer');
    return Promise.resolve();
  };
  api.review = function (context) {
    var previousFocus = document.activeElement;
    var overlay = document.createElement('div');
    overlay.setAttribute('role', 'dialog'); overlay.setAttribute('aria-modal', 'true'); overlay.setAttribute('aria-label', 'Diagnostic report');
    overlay.style.cssText = 'position:fixed;inset:0;z-index:30000;background:rgba(0,0,0,.65);display:flex;align-items:center;justify-content:center;padding:1rem;color:var(--foreground,#eee);font:14px system-ui,sans-serif';
    var panel = document.createElement('div');
    panel.style.cssText = 'background:var(--background,#151515);border:1px solid #777;border-radius:12px;padding:24px;width:640px;max-width:100%;max-height:90vh;overflow:auto';
    var title = document.createElement('h2'); title.textContent = 'Review diagnostic report';
    var help = document.createElement('p'); help.textContent = 'Review and edit this report before sharing. GitHub will open for you to submit the issue.';
    var text = document.createElement('textarea'); text.setAttribute('aria-label', 'Diagnostic report contents'); text.value = 'Collecting system details…';
    text.style.cssText = 'width:100%;box-sizing:border-box;height:280px;background:transparent;color:inherit;border:1px solid #777;border-radius:6px;padding:12px;font:12px monospace';
    var status = document.createElement('p'); status.setAttribute('role','status');
    var buttons = document.createElement('div'); buttons.style.cssText = 'display:flex;flex-wrap:wrap;gap:8px;margin-top:16px';
    function button(label, click, disabled) {
      var node = document.createElement('button'); node.type = 'button'; node.textContent = label;
      node.style.cssText = 'border:1px solid #777;border-radius:6px;background:transparent;color:inherit;padding:8px 12px;cursor:pointer';
      node.disabled = !!disabled; node.onclick = click; buttons.appendChild(node); return node;
    }
    function close() { overlay.remove(); if (previousFocus && previousFocus.focus) previousFocus.focus(); }
    var copy = button('Copy report', function () {
      var content = api.redact(text.value); text.value = content;
      if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(content).then(function () { status.textContent = 'Report copied.'; }).catch(function () { text.select(); status.textContent = 'Select and copy the report, or download it.'; });
      else { text.select(); try { document.execCommand('copy'); status.textContent = 'Report copied.'; } catch (_) { status.textContent = 'Select and copy the report, or download it.'; } }
    }, true);
    var download = button('Download report', function () {
      var url = URL.createObjectURL(new Blob([api.redact(text.value)], {type:'text/plain;charset=utf-8'}));
      var link = document.createElement('a'); link.href = url; link.download = 'delta-diagnostic-report.txt'; panel.appendChild(link); link.click(); link.remove();
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
    }, true);
    var open = button('Open GitHub issue', function () {
      api.openIssue(text.value).catch(function () { status.textContent = 'Could not open your browser. Copy the report and visit github.com/nile-agi/delta-release/issues/new.'; });
    }, true);
    var dismiss = button('Close', close);
    overlay.onkeydown = function (event) {
      if (event.key === 'Escape') { event.preventDefault(); close(); }
      if (event.key === 'Tab') {
        var focusable = [text, copy, download, open, dismiss].filter(function (node) { return !node.disabled; });
        if (event.shiftKey && document.activeElement === focusable[0]) { event.preventDefault(); focusable[focusable.length-1].focus(); }
        else if (!event.shiftKey && document.activeElement === focusable[focusable.length-1]) { event.preventDefault(); focusable[0].focus(); }
      }
    };
    panel.appendChild(title); panel.appendChild(help); panel.appendChild(text); panel.appendChild(status); panel.appendChild(buttons); overlay.appendChild(panel); document.body.appendChild(overlay); dismiss.focus();
    api.collect(context).then(function (content) {
      if (content === null) { text.value = 'No saved failure report is available.'; return; }
      text.value = content; copy.disabled = download.disabled = open.disabled = false;
    });
  };
  api.attach = function (container, context) {
    var button = document.createElement('button'); button.textContent = 'Review report'; button.type = 'button'; button.style.cssText = 'padding:8px 12px;cursor:pointer';
    button.onclick = function () { api.review(context); }; container.appendChild(button);
    api.collect(context).then(function (content) { last = content; });
  };
  api.record = function (context) { return api.collect(context).then(function (content) { last = content; return content; }); };
  window.__DELTA_DIAGNOSTICS__ = api;
  window.addEventListener('error', function (event) { if (event.message) api.record({ stage:'frontend', message: event.message }); });
  window.addEventListener('unhandledrejection', function (event) { api.record({stage:'frontend', message: event.reason && event.reason.message || String(event.reason)}); });
})();

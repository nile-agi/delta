import { browser } from '$app/environment';

export async function openHardwareWindow() {
	if (!browser) return;
	const label = 'hardware-telemetry';

	// Lazy imports avoid SSR/circular issues
	const { hardwareWindow } = await import('$lib/stores/hardware-window.svelte');
	const { dockStore } = await import('$lib/stores/dock.svelte');

	const fallback = () => {
		hardwareWindow.open();
		dockStore.register(label, 'Hardware Telemetry');
	};

	if (!('__TAURI_INTERNALS__' in window)) {
		fallback(); // plain browser
		return;
	}

	// Catch the async permission rejection from the WebviewWindow constructor
	const onReject = (e: PromiseRejectionEvent) => {
		console.error('[hardware] OS window rejected:', e.reason);
		e.preventDefault();
		fallback();
	};
	window.addEventListener('unhandledrejection', onReject, { once: true });
	setTimeout(() => window.removeEventListener('unhandledrejection', onReject), 3000);

	try {
		// ✅ Correct module: WebviewWindow is exported from 'webviewWindow',
		// NOT from 'webview' (which only exports the Webview class).
		const { WebviewWindow } = await import('@tauri-apps/api/webviewWindow');

		const existing = await WebviewWindow.getByLabel(label);
		if (existing) {
			await existing.setFocus();
			return;
		}

		new WebviewWindow(label, {
			url: '/?window=hardware',
			title: 'Hardware Telemetry',
			width: 460,
			height: 780,
			minWidth: 380,
			minHeight: 520,
			center: true,
			resizable: true
		});
	} catch (e) {
		console.error('[hardware] OS window failed:', e);
		window.removeEventListener('unhandledrejection', onReject);
		fallback();
	}
}
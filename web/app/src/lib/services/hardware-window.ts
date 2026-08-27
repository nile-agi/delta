import { browser } from '$app/environment';

// Local structural type so we don't depend on the (sometimes broken) static typings
interface WebviewWindowCtor {
	new (
		label: string,
		options?: Record<string, unknown>
	): { setFocus(): Promise<void> };
	getByLabel(label: string): Promise<{ setFocus(): Promise<void> } | null>;
}

async function getWebviewWindowCtor(): Promise<WebviewWindowCtor> {
	// Dynamic import: SSR-safe and bypasses the d.ts export bug
	const mod = (await import('@tauri-apps/api/webview')) as unknown as {
		WebviewWindow: WebviewWindowCtor;
	};
	return mod.WebviewWindow;
}

export async function openHardwareWindow() {
	if (!browser) return;
	const label = 'hardware-telemetry';

	// Plain browser (no Tauri): open a tab instead
	if (!('__TAURI_INTERNALS__' in window)) {
		window.open('/?window=hardware', '_blank');
		return;
	}

	const WebviewWindow = await getWebviewWindowCtor();

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
}
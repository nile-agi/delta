import { browser } from '$app/environment';

export async function openSettingsWindow() {
	if (!browser) return;
	const label = 'settings';

	const fallback = async () => {
		const { settingsWindow } = await import('$lib/stores/settings-window.svelte');
		settingsWindow.open();
	};

	if (!('__TAURI_INTERNALS__' in window)) {
		await fallback();
		return;
	}

	try {
		const { WebviewWindow } = await import('@tauri-apps/api/webviewWindow');
		const existing = await WebviewWindow.getByLabel(label);
		if (existing) {
			await existing.setFocus();
			return;
		}

		new WebviewWindow(label, {
			url: '/?window=settings',
			title: 'Settings',
			width: 850,
			height: 700,
			minWidth: 600,
			minHeight: 500,
			center: true,
			resizable: true
		});
	} catch (e) {
		console.error('[settings] OS window failed:', e);
		await fallback();
	}
}

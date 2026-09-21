import { browser } from '$app/environment';

export async function openSettingsWindow() {
	console.log('[Settings] openSettingsWindow called');
	if (!browser) {
		console.warn('[Settings] Not in browser environment');
		return;
	}

	const label = 'settings';

	const fallback = async () => {
		console.log('[Settings] Using fallback (in-app window)');
		const { settingsWindow } = await import('$lib/stores/settings-window.svelte');
		settingsWindow.open();
	};

	if (!('__TAURI_INTERNALS__' in window)) {
		console.log('[Settings] Not in Tauri, using fallback');
		await fallback();
		return;
	}

	try {
		console.log('[Settings] Attempting to create Tauri OS window...');
		const { WebviewWindow } = await import('@tauri-apps/api/webviewWindow');
		const existing = await WebviewWindow.getByLabel(label);

		if (existing) {
			console.log('[Settings] Window exists, focusing...');
			await existing.setFocus();
			return;
		}

		console.log('[Settings] Creating new Tauri window...');
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
		console.error('[Settings] OS window creation failed:', e);
		await fallback();
	}
}

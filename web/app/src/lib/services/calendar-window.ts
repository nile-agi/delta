import { browser } from '$app/environment';

export async function openCalendarWindow() {
	if (!browser) return;
	const label = 'calendar';

	const fallback = async () => {
		const { calendarWindow } = await import('$lib/stores/calendar-window.svelte');
		calendarWindow.open();
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
			url: '/?window=calendar',
			title: 'Calendar',
			width: 900,
			height: 700,
			minWidth: 600,
			minHeight: 500,
			center: true,
			resizable: true
		});
	} catch (e) {
		console.error('[calendar] OS window failed:', e);
		await fallback();
	}
}
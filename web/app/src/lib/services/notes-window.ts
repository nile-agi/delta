import { browser } from '$app/environment';

export async function openNotesWindow() {
	if (!browser) return;
	const label = 'notes';

	const fallback = async () => {
		const { notesWindow } = await import('$lib/stores/notes-window.svelte');
		notesWindow.open();
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
			url: '/?window=notes',
			title: 'Notes',
			width: 800,
			height: 650,
			minWidth: 500,
			minHeight: 400,
			center: true,
			resizable: true
		});
	} catch (e) {
		console.error('[notes] OS window failed:', e);
		await fallback();
	}
}
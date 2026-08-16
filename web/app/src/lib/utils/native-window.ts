// web/app/src/lib/utils/native-window.ts
import { WebviewWindow } from '@tauri-apps/api/webviewWindow';

interface WindowConfig {
	label: string;
	title: string;
	url: string;
	width?: number;
	height?: number;
}

export async function openNativeWindow(config: WindowConfig) {
	const existing = await WebviewWindow.getByLabel(config.label);
	if (existing) {
		if (await existing.isMinimized()) await existing.unminimize();
		await existing.setFocus();
		return;
	}

	// ✅ Delta uses hash routing: the route must be in the hash → /#/notes
	const hashUrl = config.url.startsWith('#') ? config.url : `/#${config.url}`;

	const win = new WebviewWindow(config.label, {
		url: hashUrl,
		title: config.title,
		width: config.width || 800,
		height: config.height || 600,
		center: true,
		focus: true,
		decorations: true,
		resizable: true,
		minWidth: 400,
		minHeight: 300
	});

	win.once('tauri://error', (error) => {
		console.error(`Failed to open ${config.label}:`, error);
	});
}
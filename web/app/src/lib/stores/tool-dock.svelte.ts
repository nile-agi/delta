import { WebviewWindow } from '@tauri-apps/api/webviewWindow';

export const TOOL_WINDOWS = [
	{ label: 'calendar', title: 'Calendar' },
	{ label: 'notes', title: 'Notes' },
	{ label: 'monitor', title: 'System Monitor' }
];

class ToolDockStore {
	state = $state<Record<string, { open: boolean; minimized: boolean }>>({});

	async refresh() {
		for (const tool of TOOL_WINDOWS) {
			try {
				const win = await WebviewWindow.getByLabel(tool.label);
				if (!win) {
					this.state[tool.label] = { open: false, minimized: false };
					continue;
				}
				const minimized = await win.isMinimized();
				this.state[tool.label] = { open: true, minimized };
			} catch {
				this.state[tool.label] = { open: false, minimized: false };
			}
		}
	}

	minimizedTools() {
		return TOOL_WINDOWS.filter((t) => this.state[t.label]?.minimized);
	}

	async restore(label: string) {
		const win = await WebviewWindow.getByLabel(label);
		if (win) {
			await win.unminimize();
			await win.setFocus();
		}
	}
}

export const toolDock = new ToolDockStore();
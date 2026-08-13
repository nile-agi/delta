import { browser } from '$app/environment';

const DOCK_BOTTOM_MARGIN = 80;
const BAR_WIDTH = 200;
const BAR_GAP = 8;
const DOCK_EDGE_PADDING = 16;

class DockStore {
	windows = $state<Array<{ id: string; title: string }>>([]);
	viewportWidth = $state(browser ? window.innerWidth : 1920);

	updateViewport() {
		if (browser) this.viewportWidth = window.innerWidth;
	}

	register(id: string, title: string) {
		if (!this.windows.find((w) => w.id === id)) {
			this.windows = [...this.windows, { id, title }];
		}
	}

	unregister(id: string) {
		this.windows = this.windows.filter((w) => w.id !== id);
	}

	getPosition(id: string): { x: number; y: number } {
		const idx = this.windows.findIndex((w) => w.id === id);
		if (idx === -1) return { x: 120, y: 100 };

		const vw = this.viewportWidth;
		const vh = browser ? window.innerHeight : 900;
		const totalWidth = this.windows.length * BAR_WIDTH + (this.windows.length - 1) * BAR_GAP;
		const startX = Math.max(DOCK_EDGE_PADDING, (vw - totalWidth) / 2);

		return {
			x: startX + idx * (BAR_WIDTH + BAR_GAP),
			y: vh - DOCK_BOTTOM_MARGIN
		};
	}
}

export const dockStore = new DockStore();
import { browser } from '$app/environment';
import { dockStore } from './dock.svelte';

interface WindowState {
	open: boolean;
	minimized: boolean;
	x: number;
	y: number;
	width: number;
	height: number;
	preMinimizeX: number;
	preMinimizeY: number;
}

const DEFAULT_STATE: WindowState = {
	open: false,
	minimized: false,
	x: 140,
	y: 100,
	width: 400,
	height: 600,
	preMinimizeX: 140,
	preMinimizeY: 100
};

class MonitorWindowStore {
	state = $state<WindowState>(this.load());

	private load(): WindowState {
		if (!browser) return DEFAULT_STATE;

		try {
			const saved = localStorage.getItem('monitor-window-state');
			return saved
				? { ...DEFAULT_STATE, ...JSON.parse(saved) }
				: DEFAULT_STATE;
		} catch {
			return DEFAULT_STATE;
		}
	}

	private save() {
		if (!browser) return;
		localStorage.setItem(
			'monitor-window-state',
			JSON.stringify(this.state)
		);
	}

	open = () => {
		this.state.open = true;
		this.state.minimized = false;
		this.save();
	};

	close = () => {
		dockStore.unregister('monitor');
		this.state.open = false;
		this.state.minimized = false;
		this.save();
	};

	setPosition = (x: number, y: number, persist = true) => {
		this.state.x = x;
		this.state.y = y;
		if (persist) this.save();
	};

	setSize = (width: number, height: number, persist = true) => {
		this.state.width = width;
		this.state.height = height;
		if (persist) this.save();
	};

	commit = () => {
		this.save();
	};

	minimize = () => {
		this.state.preMinimizeX = this.state.x;
		this.state.preMinimizeY = this.state.y;
		this.state.minimized = true;

		dockStore.register('monitor', 'System Monitor');

		const pos = dockStore.getPosition('monitor');

		if (pos) {
			this.state.x = pos.x;
			this.state.y = pos.y;
		}

		this.save();
	};

	restore = () => {
		dockStore.unregister('monitor');
		this.state.minimized = false;
		this.state.x = this.state.preMinimizeX;
		this.state.y = this.state.preMinimizeY;
		this.save();
	};

	clampToViewport = () => {
		if (!browser) return;

		const maxX = window.innerWidth - this.state.width;
		const maxY = window.innerHeight - this.state.height;

		this.state.x = Math.max(0, Math.min(this.state.x, maxX));
		this.state.y = Math.max(0, Math.min(this.state.y, maxY));

		this.save();
	};
}

export const monitorWindow = new MonitorWindowStore();
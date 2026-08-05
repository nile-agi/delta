import { browser } from '$app/environment';

interface WindowState {
	open: boolean;
	minimized: boolean;
	x: number;
	y: number;
	width: number;
	height: number;
	docked: 'none' | 'left' | 'right';
	preDock: { x: number; y: number; width: number; height: number } | null;
}

const DEFAULT_STATE: WindowState = {
	open: false,
	minimized: false,
	x: 100,
	y: 80,
	width: 768,
	height: 600,
	docked: 'none',
	preDock: null
};

class SettingsWindowStore {
	state = $state<WindowState>({ ...DEFAULT_STATE });

	constructor() {
		if (browser) {
			this.load();
		}
	}

	private load() {
		try {
			const saved = localStorage.getItem('settingsWindow');
			if (saved) {
				const parsed = JSON.parse(saved);
				this.state = {
					...DEFAULT_STATE,
					...parsed,
					preDock: null
				};
			}
		} catch (e) {
			console.error('Failed to load settings window state', e);
		}
	}

	private save() {
		if (!browser) return;
		try {
			const { preDock, ...persist } = this.state;
			localStorage.setItem('settingsWindow', JSON.stringify(persist));
		} catch (e) {
			console.error('Failed to save settings window state', e);
		}
	}

	open() {
		this.state.open = true;
		this.state.minimized = false;
		this.save();
	}

	close() {
		this.state.open = false;
		this.state.minimized = false;
		this.state.docked = 'none';
		this.state.preDock = null;
		this.save();
	}

	minimize() {
		this.state.minimized = true;
		this.save();
	}

	restore() {
		this.state.minimized = false;
		this.save();
	}

	toggle() {
		if (this.state.open && !this.state.minimized && this.state.docked === 'none') {
			this.minimize();
		} else {
			this.open();
		}
	}

	dock(side: 'left' | 'right') {
		if (this.state.docked === 'none') {
			this.state.preDock = {
				x: this.state.x,
				y: this.state.y,
				width: this.state.width,
				height: this.state.height
			};
		}
		this.state.docked = side;
		this.state.minimized = false;
		this.save();
	}

	undock() {
		if (this.state.preDock) {
			this.state.x = this.state.preDock.x;
			this.state.y = this.state.preDock.y;
			this.state.width = this.state.preDock.width;
			this.state.height = this.state.preDock.height;
		}
		this.state.docked = 'none';
		this.save();
	}

	setPosition(x: number, y: number) {
		this.state.x = x;
		this.state.y = y;
		this.save();
	}

	setSize(width: number, height: number) {
		this.state.width = width;
		this.state.height = height;
		this.save();
	}
}

export const settingsWindow = new SettingsWindowStore();
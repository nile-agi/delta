import { browser } from '$app/environment';
import { dockStore } from './dock.svelte';

export const SETTINGS_WINDOW_MIN_WIDTH = 400;
export const SETTINGS_WINDOW_MIN_HEIGHT = 300;
export const SETTINGS_WINDOW_MIN_VISIBLE_X = 100;
export const SETTINGS_WINDOW_MIN_VISIBLE_Y = 40;

interface WindowState {
	open: boolean;
	minimized: boolean;
	x: number;
	y: number;
	width: number;
	height: number;
	preMinimizeX: number;
	preMinimizeY: number;
	docked: 'left' | 'right' | 'floating';
	pendingSection: string | null;
}

const MIN_WIDTH = 400;
const MIN_HEIGHT = 300;
const MIN_VISIBLE_X = 100;
const MIN_VISIBLE_Y = 40;

const DEFAULT_STATE: WindowState = {
	open: false,
	minimized: false,
	x: 120,
	y: 100,
	width: 720,
	height: 540,
	preMinimizeX: 120,
	preMinimizeY: 100,
	docked: 'floating',
	pendingSection: null
};

class SettingsWindowStore {
	state = $state({ ...DEFAULT_STATE });

	constructor() {
		if (browser) this.load();
	}

	private load() {
		try {
			const saved = localStorage.getItem('settingsWindow');
			if (saved) this.state = { ...DEFAULT_STATE, ...JSON.parse(saved) };
		} catch (e) {
			console.error('Failed to load settings window state', e);
		}
	}

	private save() {
		if (!browser) return;
		try {
			localStorage.setItem('settingsWindow', JSON.stringify(this.state));
		} catch (e) {
			console.error('Failed to save settings window state', e);
		}
	}

	get pendingSection() {
		return this.state.pendingSection;
	}

	set pendingSection(value: string | null) {
		this.state.pendingSection = value;
	}

	open() {
		this.state.open = true;
		this.state.minimized = false;
		this.save();
	}

	close() {
		dockStore.unregister('settings');
		this.state.open = false;
		this.state.minimized = false;
		this.save();
	}

	minimize() {
		this.state.preMinimizeX = this.state.x;
		this.state.preMinimizeY = this.state.y;
		this.state.minimized = true;
		dockStore.register('settings', 'Settings');
		const pos = dockStore.getPosition('settings');
		this.state.x = pos.x;
		this.state.y = pos.y;
		this.save();
	}

	restore() {
		dockStore.unregister('settings');
		this.state.minimized = false;
		this.state.x = this.state.preMinimizeX;
		this.state.y = this.state.preMinimizeY;
		this.save();
	}

	toggle() {
		if (this.state.open && !this.state.minimized) this.minimize();
		else this.open();
	}

	dock(side: 'left' | 'right') {
		this.state.docked = side;
		this.save();
	}

	undock() {
		this.state.docked = 'floating';
		this.save();
	}

	setPosition(x: number, y: number, persist = true) {
		this.state.x = x;
		this.state.y = y;
		if (persist) this.save();
	}

	setSize(width: number, height: number, persist = true) {
		this.state.width = width;
		this.state.height = height;
		if (persist) this.save();
	}

	commit() {
		this.save();
	}

	clampToViewport() {
		if (!browser) return;
		const vw = window.innerWidth;
		const vh = window.innerHeight;
		const w = Math.max(MIN_WIDTH, Math.min(this.state.width, vw));
		const h = Math.max(MIN_HEIGHT, Math.min(this.state.height, vh));
		const x = Math.max(0, Math.min(this.state.x, vw - MIN_VISIBLE_X));
		const y = Math.max(0, Math.min(this.state.y, vh - MIN_VISIBLE_Y));
		if (x !== this.state.x || y !== this.state.y || w !== this.state.width || h !== this.state.height) {
			this.state.x = x;
			this.state.y = y;
			this.state.width = w;
			this.state.height = h;
			this.save();
		}
	}
}

export const settingsWindow = new SettingsWindowStore();
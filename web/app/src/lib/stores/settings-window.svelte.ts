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

export const SETTINGS_WINDOW_MIN_WIDTH = 400;
export const SETTINGS_WINDOW_MIN_HEIGHT = 300;

/** Slivers of the window that must stay on-screen so the title bar remains grabbable. */
export const SETTINGS_WINDOW_MIN_VISIBLE_X = 100;
export const SETTINGS_WINDOW_MIN_VISIBLE_Y = 40;

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

	/**
	 * Section the dialog should switch to the next time it renders. Lives outside `state`
	 * because it is transient UI intent, not window geometry, and must not be persisted.
	 */
	pendingSection = $state<string | null>(null);

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

	/** Open (or restore) the window with a specific section selected. */
	openTo(section: string) {
		this.pendingSection = section;
		this.open();
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

	/**
	 * `persist` is opted out of during a drag/resize gesture: this runs once per pointer
	 * sample, and save() is a synchronous JSON.stringify + localStorage.setItem. Call
	 * commit() once the gesture ends.
	 */
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

	/** Persist the geometry accumulated by non-persisting setPosition/setSize calls. */
	commit() {
		this.save();
	}

	/**
	 * Geometry survives across sessions and viewport changes, so a frame saved on a wide
	 * display can end up oversized or entirely off-screen. Pull it back into view.
	 */
	clampToViewport() {
		if (!browser) return;

		const vw = window.innerWidth;
		const vh = window.innerHeight;

		const width = Math.max(SETTINGS_WINDOW_MIN_WIDTH, Math.min(this.state.width, vw));
		const height = Math.max(SETTINGS_WINDOW_MIN_HEIGHT, Math.min(this.state.height, vh));
		const x = Math.max(0, Math.min(this.state.x, vw - SETTINGS_WINDOW_MIN_VISIBLE_X));
		const y = Math.max(0, Math.min(this.state.y, vh - SETTINGS_WINDOW_MIN_VISIBLE_Y));

		if (
			x === this.state.x &&
			y === this.state.y &&
			width === this.state.width &&
			height === this.state.height
		) {
			return;
		}

		this.state.x = x;
		this.state.y = y;
		this.state.width = width;
		this.state.height = height;
		this.save();
	}
}

export const settingsWindow = new SettingsWindowStore();
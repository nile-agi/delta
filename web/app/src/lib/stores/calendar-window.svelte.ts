import { browser } from '$app/environment';

interface WindowState {
	open: boolean;
	minimized: boolean;
	x: number;
	y: number;
	width: number;
	height: number;
}

const MIN_WIDTH = 400;
const MIN_HEIGHT = 300;
const MIN_VISIBLE_X = 100;
const MIN_VISIBLE_Y = 40;

const DEFAULT_STATE: WindowState = {
	open: false,
	minimized: false,
	x: 160,
	y: 120,
	width: 520,
	height: 580
};

class CalendarWindowStore {
	state = $state({ ...DEFAULT_STATE });

	constructor() {
		if (browser) this.load();
	}

	private load() {
		try {
			const saved = localStorage.getItem('calendarWindow');
			if (saved) this.state = { ...DEFAULT_STATE, ...JSON.parse(saved) };
		} catch (e) {
			console.error('Failed to load calendar window state', e);
		}
	}

	private save() {
		if (!browser) return;
		try {
			localStorage.setItem('calendarWindow', JSON.stringify(this.state));
		} catch (e) {
			console.error('Failed to save calendar window state', e);
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
		if (this.state.open && !this.state.minimized) this.minimize();
		else this.open();
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

export const calendarWindow = new CalendarWindowStore();
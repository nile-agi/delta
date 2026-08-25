import { browser } from '$app/environment';

interface HardwareWindowState {
    open: boolean;
    minimized: boolean;
    x: number;
    y: number;
    width: number;
    height: number;
}

const STORAGE_KEY = 'delta-hardware-window';

class HardwareWindowStore {
    state = $state<HardwareWindowState>({
        open: false,
        minimized: false,
        x: 120,
        y: 100,
        width: 360,
        height: 420
    });

    constructor() {
        if (!browser) return;
        this.loadPersisted();
    }

    private loadPersisted() {
        try {
            const raw = localStorage.getItem(STORAGE_KEY);
            if (raw) {
                const saved = JSON.parse(raw);
                // Only restore position/size, never auto-open
                this.state.x = saved.x ?? 120;
                this.state.y = saved.y ?? 100;
                this.state.width = saved.width ?? 360;
                this.state.height = saved.height ?? 420;
            }
        } catch { /* ignore */ }
    }

    private persist() {
        if (!browser) return;
        try {
            localStorage.setItem(STORAGE_KEY, JSON.stringify({
                x: this.state.x,
                y: this.state.y,
                width: this.state.width,
                height: this.state.height
            }));
        } catch { /* ignore */ }
    }

    open() {
        this.state.open = true;
        this.state.minimized = false;
    }

    close() {
        this.state.open = false;
        this.state.minimized = false;
    }

    toggle() {
        if (this.state.open) {
            this.close();
        } else {
            this.open();
        }
    }

    minimize() {
        this.state.minimized = true;
    }

    restore() {
        this.state.open = true;
        this.state.minimized = false;
    }

    setPosition(x: number, y: number, persist = true) {
        this.state.x = x;
        this.state.y = y;
        if (persist) this.persist();
    }

    setSize(width: number, height: number, persist = true) {
        this.state.width = width;
        this.state.height = height;
        if (persist) this.persist();
    }

    commit() {
        this.persist();
    }

    clampToViewport() {
        if (!browser) return;
        const vw = window.innerWidth;
        const vh = window.innerHeight;
        const maxX = Math.max(0, vw - 100);
        const maxY = Math.max(0, vh - 60);
        this.state.x = Math.max(0, Math.min(this.state.x, maxX));
        this.state.y = Math.max(0, Math.min(this.state.y, maxY));
    }
}

export const hardwareWindow = new HardwareWindowStore();
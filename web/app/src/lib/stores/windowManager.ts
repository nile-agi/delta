import { writable, derived, get } from 'svelte/store';

interface WindowState {
	windowId: string;
	visible: boolean;
	minimized: boolean;
	position: { x: number; y: number };
}

const windows = writable<Map<string, WindowState>>(new Map());
const zStack = writable<string[]>([]);

export function registerWindow(windowId: string, defaultPosition = { x: 100, y: 100 }) {
	windows.update((map) => {
		if (!map.has(windowId)) {
			map.set(windowId, {
				windowId,
				visible: true,
				minimized: false,
				position: { ...defaultPosition }
			});
			zStack.update((stack) => [...stack.filter((id) => id !== windowId), windowId]);
		}
		return map;
	});
}

export function unregisterWindow(windowId: string) {
	windows.update((map) => {
		map.delete(windowId);
		return map;
	});
	zStack.update((stack) => stack.filter((id) => id !== windowId));
}

export function bringToFront(windowId: string) {
	zStack.update((stack) => [...stack.filter((id) => id !== windowId), windowId]);
}

export function getZIndex(windowId: string) {
	return derived(zStack, ($stack) => {
		const idx = $stack.indexOf(windowId);
		return idx >= 0 ? 100 + idx * 10 : 50;
	});
}

export function updateWindowState(windowId: string, patch: Partial<WindowState>) {
	windows.update((map) => {
		const existing = map.get(windowId);
		if (existing) {
			map.set(windowId, { ...existing, ...patch });
		}
		return map;
	});
}

export const windowStore = { subscribe: windows.subscribe };
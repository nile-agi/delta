import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { nextFrame } from './next-frame';

let frames: Array<() => void>;
beforeEach(() => {
	vi.useFakeTimers();
	frames = [];
	vi.stubGlobal('requestAnimationFrame', (cb: () => void) => frames.push(cb));
});
afterEach(() => {
	vi.useRealTimers();
	vi.unstubAllGlobals();
});

it('runs on the next frame, once', () => {
	const cb = vi.fn();
	nextFrame(cb);
	frames.forEach((f) => f());
	vi.advanceTimersByTime(500);
	expect(cb).toHaveBeenCalledTimes(1);
});

it('still runs when no frame ever comes (hidden window, stalled webview)', () => {
	const cb = vi.fn();
	nextFrame(cb);
	vi.advanceTimersByTime(99);
	expect(cb).not.toHaveBeenCalled();
	vi.advanceTimersByTime(1);
	expect(cb).toHaveBeenCalledTimes(1);
	frames.forEach((f) => f());
	expect(cb).toHaveBeenCalledTimes(1);
});

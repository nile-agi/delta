/**
 * Runs `callback` on the next animation frame, or after `fallbackMs` when no frame comes.
 * Frames stop while a window is hidden or a webview stalls; a stream must still finish then.
 */
export function nextFrame(callback: () => void, fallbackMs = 100): void {
	let ran = false;
	const run = () => {
		if (ran) return;
		ran = true;
		clearTimeout(timer);
		callback();
	};
	const timer = setTimeout(run, fallbackMs);
	if (typeof requestAnimationFrame === 'function') requestAnimationFrame(run);
}

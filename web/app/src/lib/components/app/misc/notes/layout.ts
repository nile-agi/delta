export type PaneWidths = [number, number, number];
export const DEFAULT_PROPORTIONS: PaneWidths = [0.2, 0.3, 0.5];
const minimums: PaneWidths = [160, 220, 320];

export function fitPanes(width: number, proportions: PaneWidths): PaneWidths {
	if (width < 560) return [0, 0, width];
	if (width < 720) {
		const list = Math.max(220, Math.min(width - 320, width * proportions[1] / (proportions[1] + proportions[2])));
		return [0, list, width - list];
	}
	const available = width - 700;
	const desired = proportions.map((p, i) => Math.max(0, width * p - minimums[i]));
	const total = desired.reduce((a, b) => a + b, 0);
	return minimums.map((min, i) => min + available * (total ? desired[i] / total : DEFAULT_PROPORTIONS[i])) as PaneWidths;
}

export function resizePanes(widths: PaneWidths, divider: 0 | 1, delta: number): PaneWidths {
	const next: PaneWidths = [...widths];
	const move = Math.max(minimums[divider] - widths[divider], Math.min(widths[divider + 1] - minimums[divider + 1], delta));
	next[divider] += move;
	next[divider + 1] -= move;
	return next;
}

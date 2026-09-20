import { describe, expect, it } from 'vitest';
import { fitPanes, resizePanes } from './layout';

describe('notes pane geometry', () => {
	it('fits desktop proportions without shrinking any pane below its minimum', () => {
		expect(fitPanes(1000, [0.2, 0.3, 0.5])).toEqual([200, 300, 500]);
		expect(fitPanes(720, [0.2, 0.3, 0.5])).toEqual([160, 220, 340]);
	});
	it('moves only adjacent panes and clamps excessive dragging', () => {
		expect(resizePanes([200, 300, 500], 0, 50)).toEqual([250, 250, 500]);
		expect(resizePanes([200, 300, 500], 0, 500)).toEqual([280, 220, 500]);
		expect(resizePanes([200, 300, 500], 1, 500)).toEqual([200, 480, 320]);
	});
	it('keeps the list and editor usable when folders are collapsed', () => {
		expect(fitPanes(600, [0.2, 0.3, 0.5])).toEqual([0, 225, 375]);
		expect(resizePanes([0, 225, 375], 1, -100)).toEqual([0, 220, 380]);
	});
});

<script lang="ts">
	import { onDestroy } from 'svelte';
	import { resizePanes, type PaneWidths } from './layout';
	let {
		widths,
		index,
		onchange,
		oncommit,
		onreset
	}: {
		widths: PaneWidths;
		index: 0 | 1;
		onchange: (widths: PaneWidths) => void;
		oncommit: () => void;
		onreset: () => void;
	} = $props();
	let dragging = $state(false);
	let origin = 0;
	let initial: PaneWidths;
	let oldCursor = '';
	let oldSelection = '';
	const left = $derived(index === 0 ? widths[0] : widths[0] + widths[1]);
	function restoreBody() {
		if (!dragging) return;
		document.body.style.cursor = oldCursor;
		document.body.style.userSelect = oldSelection;
		dragging = false;
	}
	function start(event: PointerEvent) {
		if (event.button !== 0) return;
		event.preventDefault();
		initial = [...widths];
		origin = event.clientX;
		oldCursor = document.body.style.cursor;
		oldSelection = document.body.style.userSelect;
		document.body.style.cursor = 'col-resize';
		document.body.style.userSelect = 'none';
		dragging = true;
		(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
	}
	function end(event: PointerEvent, cancel = false) {
		if (!dragging) return;
		if (cancel) onchange(initial);
		restoreBody();
		const target = event.currentTarget as HTMLElement;
		if (target.hasPointerCapture(event.pointerId)) target.releasePointerCapture(event.pointerId);
		oncommit();
	}
	function keydown(event: KeyboardEvent) {
		if (event.key === 'Enter' || event.key === 'Home') {
			event.preventDefault();
			onreset();
			return;
		}
		if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
		event.preventDefault();
		onchange(
			resizePanes(widths, index, (event.key === 'ArrowLeft' ? -1 : 1) * (event.shiftKey ? 50 : 10))
		);
		oncommit();
	}
	onDestroy(restoreBody);
</script>

<!-- svelte-ignore a11y_no_noninteractive_tabindex, a11y_no_noninteractive_element_interactions (An adjustable ARIA separator supports both keyboard and pointer input.) -->
<div
	class="divider"
	class:dragging
	style:left={`${left}px`}
	role="separator"
	aria-label={index === 0 ? 'Resize folders and notes' : 'Resize notes and editor'}
	aria-orientation="vertical"
	aria-valuenow={Math.round(widths[index])}
	aria-valuemin={index === 0 ? 160 : 220}
	aria-valuemax={Math.round(widths[index] + widths[index + 1] - (index === 0 ? 220 : 320))}
	tabindex="0"
	onpointerdown={start}
	onpointermove={(e) => {
		if (dragging) onchange(resizePanes(initial, index, e.clientX - origin));
	}}
	onpointerup={(e) => end(e)}
	onpointercancel={(e) => end(e, true)}
	onlostpointercapture={(e) => end(e, true)}
	onkeydown={keydown}
	ondblclick={onreset}
></div>

<style>
	.divider {
		position: absolute;
		top: 0;
		bottom: 0;
		width: 10px;
		margin-left: -5px;
		z-index: 10;
		cursor: col-resize;
		touch-action: none;
		outline: none;
	}
	.divider::after {
		content: '';
		position: absolute;
		left: 4px;
		top: 0;
		bottom: 0;
		width: 2px;
		background: transparent;
		transition: background-color 100ms;
	}
	.divider:hover::after,
	.divider:focus-visible::after,
	.divider.dragging::after {
		background: var(--primary);
	}
</style>

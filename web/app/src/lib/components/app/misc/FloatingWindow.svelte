<script lang="ts">
	import { Minus, X, GripVertical } from '@lucide/svelte';
	import { IsMobile } from '$lib/hooks/is-mobile.svelte';
	import { SETTINGS_WINDOW_FULLBLEED_BREAKPOINT } from '$lib/constants/viewport';
	import { untrack } from 'svelte';

	interface Props {
		title: string;
		store: {
			state: { open: boolean; minimized: boolean; x: number; y: number; width: number; height: number };
			setPosition(x: number, y: number, persist?: boolean): void;
			setSize(width: number, height: number, persist?: boolean): void;
			commit(): void;
			close(): void;
			minimize(): void;
			restore(): void;
			clampToViewport(): void;
		};
		minWidth?: number;
		minHeight?: number;
		minVisibleX?: number;
		minVisibleY?: number;
		children: import('svelte').Snippet;
	}

	let {
		title,
		store,
		minWidth = 400,
		minHeight = 300,
		minVisibleX = 100,
		minVisibleY = 40,
		children
	}: Props = $props();

	let isDragging = $state(false);
	let dragOffsetX = $state(0);
	let dragOffsetY = $state(0);

	let isResizing = $state(false);
	let resizeStartX = $state(0);
	let resizeStartY = $state(0);
	let resizeStartWidth = $state(0);
	let resizeStartHeight = $state(0);

	const fullBleed = new IsMobile(SETTINGS_WINDOW_FULLBLEED_BREAKPOINT);

	function releaseCapture(e: PointerEvent) {
		const el = e.currentTarget as HTMLElement | null;
		if (el?.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
	}

	function onDragStart(e: PointerEvent) {
		if (fullBleed.current || isResizing) return;
		if ((e.target as HTMLElement).closest('button')) return;
		e.preventDefault();
		isDragging = true;
		dragOffsetX = e.clientX - store.state.x;
		dragOffsetY = e.clientY - store.state.y;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onDragMove(e: PointerEvent) {
		if (!isDragging) return;
		const maxX = window.innerWidth - minVisibleX;
		const maxY = window.innerHeight - minVisibleY;
		const newX = Math.max(0, Math.min(e.clientX - dragOffsetX, maxX));
		const newY = Math.max(0, Math.min(e.clientY - dragOffsetY, maxY));
		store.setPosition(newX, newY, false);
	}

	function onDragEnd(e: PointerEvent) {
		if (!isDragging) return;
		isDragging = false;
		releaseCapture(e);
		store.commit();
	}

	function onResizeStart(e: PointerEvent) {
		if (fullBleed.current) return;
		e.preventDefault();
		isResizing = true;
		resizeStartX = e.clientX;
		resizeStartY = e.clientY;
		resizeStartWidth = store.state.width;
		resizeStartHeight = store.state.height;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onResizeMove(e: PointerEvent) {
		if (!isResizing) return;
		const newWidth = Math.max(minWidth, resizeStartWidth + (e.clientX - resizeStartX));
		const newHeight = Math.max(minHeight, resizeStartHeight + (e.clientY - resizeStartY));
		store.setSize(newWidth, newHeight, false);
	}

	function onResizeEnd(e: PointerEvent) {
		if (!isResizing) return;
		isResizing = false;
		releaseCapture(e);
		store.commit();
	}

	function clampIfFloating() {
		if (fullBleed.current) return;
		if (!store.state.open || store.state.minimized) return;
		if (isDragging || isResizing) return;
		store.clampToViewport();
	}

	function handleMinimize() {
		store.minimize();
	}

	function handleClose() {
		store.close();
	}

	$effect(() => {
		void fullBleed.current;
		void store.state.open;
		void store.state.minimized;
		untrack(clampIfFloating);
	});
</script>

<svelte:window onresize={clampIfFloating} />

{#if store.state.open && !store.state.minimized}
	<!-- svelte-ignore a11y_no_static_element_interactions -->
	<div
		class="floating-window"
		style="left: {store.state.x}px; top: {store.state.y}px; width: {store.state.width}px; height: {store.state.height}px;"
	>
		<!-- Title Bar -->
		<div
			class="floating-window-titlebar"
			class:draggable={!fullBleed.current}
			onpointerdown={fullBleed.current ? undefined : onDragStart}
			onpointermove={fullBleed.current ? undefined : onDragMove}
			onpointerup={fullBleed.current ? undefined : onDragEnd}
			onpointercancel={fullBleed.current ? undefined : onDragEnd}
		>
			<div class="flex items-center gap-2 select-none">
				<GripVertical class="h-4 w-4 text-muted-foreground" />
				<span class="text-sm font-semibold">{title}</span>
			</div>
			<div class="flex items-center gap-1">
				<button class="floating-window-btn" onclick={handleMinimize} aria-label="Minimize" title="Minimize">
					<Minus class="h-3.5 w-3.5" />
				</button>
				<button class="floating-window-btn" onclick={handleClose} aria-label="Close" title="Close">
					<X class="h-3.5 w-3.5" />
				</button>
			</div>
		</div>

		<!-- Body -->
		<div class="floating-window-body">
			{@render children()}
		</div>

		<!-- Resize Handle -->
		{#if !fullBleed.current}
			<div
				class="floating-window-resize"
				onpointerdown={onResizeStart}
				onpointermove={onResizeMove}
				onpointerup={onResizeEnd}
				onpointercancel={onResizeEnd}
				role="button"
				tabindex="0"
				aria-label="Resize"
			></div>
		{/if}
	</div>
{/if}

<style>
	.floating-window {
		position: fixed;
		z-index: 99999;
		display: flex;
		flex-direction: column;
		border-radius: 0.75rem;
		border: 1px solid color-mix(in oklch, var(--border) 30%, transparent);
		background-color: var(--background);
		box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
		overflow: hidden;
		min-width: 400px;
		min-height: 300px;
	}

	.floating-window-titlebar {
		flex-shrink: 0;
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 0.5rem 0.75rem;
		border-bottom: 1px solid color-mix(in oklch, var(--border) 30%, transparent);
		background-color: color-mix(in oklch, var(--muted) 50%, transparent);
		user-select: none;
		border-top-left-radius: 0.75rem;
		border-top-right-radius: 0.75rem;
	}

	.floating-window-titlebar.draggable {
		cursor: grab;
		touch-action: none;
	}

	.floating-window-titlebar.draggable:active {
		cursor: grabbing;
	}

	.floating-window-btn {
		display: flex;
		align-items: center;
		justify-content: center;
		width: 1.75rem;
		height: 1.75rem;
		border-radius: 0.375rem;
		color: var(--muted-foreground);
		transition: background-color 0.15s, color 0.15s;
	}

	.floating-window-btn:hover {
		background-color: var(--accent);
		color: var(--accent-foreground);
	}

	.floating-window-body {
		flex: 1;
		min-height: 0;
		overflow: hidden;
	}

	.floating-window-resize {
		position: absolute;
		bottom: 0;
		right: 0;
		width: 16px;
		height: 16px;
		cursor: nwse-resize;
		touch-action: none;
		z-index: 10;
	}

	.floating-window-resize::after {
		content: '';
		position: absolute;
		bottom: 3px;
		right: 3px;
		width: 8px;
		height: 8px;
		border-right: 2px solid color-mix(in oklch, var(--muted-foreground) 40%, transparent);
		border-bottom: 2px solid color-mix(in oklch, var(--muted-foreground) 40%, transparent);
		border-bottom-right-radius: 2px;
	}

	.floating-window-resize:hover::after {
		border-color: color-mix(in oklch, var(--muted-foreground) 70%, transparent);
	}
</style>
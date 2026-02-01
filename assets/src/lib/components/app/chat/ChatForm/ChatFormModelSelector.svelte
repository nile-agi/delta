<script lang="ts">
	import { onMount, tick } from 'svelte';
	import { ChevronDown, Loader2, Box, Search, X, Power } from '@lucide/svelte';
	import { cn } from '$lib/components/ui/utils';
	import { portalToBody } from '$lib/utils/portal-to-body';
	import { ModelsService } from '$lib/services/models';
	import {
		fetchModels,
		loadingModelId,
		modelOptions,
		modelsError,
		modelsLoading,
		modelsUpdating,
		selectModel,
		selectedModelId,
		unloadModel
	} from '$lib/stores/models.svelte';
	import type { ModelOption } from '$lib/types/models';

	interface Props {
		class?: string;
		/** When this number increments, the dropdown opens (e.g. when user presses Enter with no model). */
		openTrigger?: number;
	}

	let { class: className = '', openTrigger = 0 }: Props = $props();

	let options = $derived(modelOptions());
	let loading = $derived(modelsLoading());
	let updating = $derived(modelsUpdating());
	let loadingId = $derived(loadingModelId());
	let error = $derived(modelsError());
	let activeId = $derived(selectedModelId());

	let isMounted = $state(false);
	let isOpen = $state(false);
	let searchQuery = $state('');
	let container: HTMLDivElement | null = null;
	let triggerButton = $state<HTMLButtonElement | null>(null);
	let menuRef = $state<HTMLDivElement | null>(null);

	let filteredOptions = $derived(
		searchQuery.trim()
			? options.filter(
					(opt) =>
						opt.name.toLowerCase().includes(searchQuery.toLowerCase().trim()) ||
						(opt.description?.toLowerCase().includes(searchQuery.toLowerCase().trim()) ?? false)
				)
			: options
	);
	let menuPosition = $state<{
		top: number;
		left: number;
		width: number;
		placement: 'top' | 'bottom';
		maxHeight: number;
	} | null>(null);
	let lockedWidth: number | null = null;

	onMount(async () => {
		try {
			await fetchModels();
		} catch (error) {
			console.error('Unable to load models:', error);
		} finally {
			isMounted = true;
		}
	});

	function handlePointerDown(event: PointerEvent) {
		if (!container) return;

		const target = event.target as Node | null;

		if (target && !container.contains(target) && !(menuRef && menuRef.contains(target))) {
			closeMenu();
		}
	}

	function handleKeydown(event: KeyboardEvent) {
		if (event.key === 'Escape') {
			closeMenu();
		}
	}

	function handleResize() {
		if (isOpen) {
			updateMenuPosition();
		}
	}

	function handleScroll() {
		if (isOpen) {
			updateMenuPosition();
		}
	}

	async function handleSelect(value: string | undefined) {
		if (!value) return;

		const option = options.find((item) => item.id === value);
		if (!option) {
			console.error('Model is no longer available');
			return;
		}

		try {
			await selectModel(option.id);
		} catch (error) {
			console.error('Failed to switch model:', error);
		}
	}

	const VIEWPORT_GUTTER = 8;
	const MENU_OFFSET = 6;
	const MENU_MAX_WIDTH = 320;

	async function openMenu() {
		if (loading || updating) return;

		searchQuery = '';
		isOpen = true;
		await tick();
		updateMenuPosition();
		requestAnimationFrame(() => updateMenuPosition());
	}

	function toggleOpen() {
		if (loading || updating) return;

		if (isOpen) {
			closeMenu();
		} else {
			void openMenu();
		}
	}

	function closeMenu() {
		if (!isOpen) return;

		isOpen = false;
		menuPosition = null;
		lockedWidth = null;
	}

	async function handleOptionSelect(optionId: string) {
		try {
			await handleSelect(optionId);
		} finally {
			closeMenu();
		}
	}

	let lastOpenTrigger = $state(0);

	$effect(() => {
		if (loading || updating) {
			closeMenu();
		}
	});

	$effect(() => {
		const trigger = openTrigger;
		if (trigger > lastOpenTrigger && options.length > 0 && !loading && !updating) {
			lastOpenTrigger = trigger;
			void openMenu();
		}
	});

	$effect(() => {
		const optionCount = options.length;

		if (!isOpen || optionCount <= 0) return;

		queueMicrotask(() => updateMenuPosition());
	});

	async function handleUnloadModel(event: MouseEvent) {
		event.preventDefault();
		event.stopPropagation();
		try {
			await ModelsService.unload();
			unloadModel();
		} catch (e) {
			console.error('Failed to unload model / stop server:', e);
			unloadModel();
		}
		closeMenu();
	}

	function updateMenuPosition() {
		if (!isOpen || !triggerButton || !menuRef) return;

		const triggerRect = triggerButton.getBoundingClientRect();
		const viewportWidth = window.innerWidth;
		const viewportHeight = window.innerHeight;

		if (viewportWidth === 0 || viewportHeight === 0) return;

		const scrollWidth = menuRef.scrollWidth;
		const scrollHeight = menuRef.scrollHeight;

		const availableWidth = Math.max(0, viewportWidth - VIEWPORT_GUTTER * 2);
		const constrainedMaxWidth = Math.min(MENU_MAX_WIDTH, availableWidth || MENU_MAX_WIDTH);
		const safeMaxWidth =
			constrainedMaxWidth > 0 ? constrainedMaxWidth : Math.min(MENU_MAX_WIDTH, viewportWidth);
		const desiredMinWidth = Math.min(160, safeMaxWidth || 160);

		let width = lockedWidth;
		if (width === null) {
			const naturalWidth = Math.min(scrollWidth, safeMaxWidth);
			const baseWidth = Math.max(triggerRect.width, naturalWidth, desiredMinWidth);
			width = Math.min(baseWidth, safeMaxWidth || baseWidth);
			lockedWidth = width;
		} else {
			width = Math.min(Math.max(width, desiredMinWidth), safeMaxWidth || width);
		}

		if (width > 0) {
			menuRef.style.width = `${width}px`;
		}

		const availableBelow = Math.max(
			0,
			viewportHeight - VIEWPORT_GUTTER - triggerRect.bottom - MENU_OFFSET
		);
		const availableAbove = Math.max(0, triggerRect.top - VIEWPORT_GUTTER - MENU_OFFSET);
		const viewportAllowance = Math.max(0, viewportHeight - VIEWPORT_GUTTER * 2);
		const fallbackAllowance = Math.max(1, viewportAllowance > 0 ? viewportAllowance : scrollHeight);

		function computePlacement(placement: 'top' | 'bottom') {
			const available = placement === 'bottom' ? availableBelow : availableAbove;
			const allowedHeight =
				available > 0 ? Math.min(available, fallbackAllowance) : fallbackAllowance;
			const maxHeight = Math.min(scrollHeight, allowedHeight);
			const height = Math.max(0, maxHeight);

			let top: number;
			if (placement === 'bottom') {
				const rawTop = triggerRect.bottom + MENU_OFFSET;
				const minTop = VIEWPORT_GUTTER;
				const maxTop = viewportHeight - VIEWPORT_GUTTER - height;
				if (maxTop < minTop) {
					top = minTop;
				} else {
					top = Math.min(Math.max(rawTop, minTop), maxTop);
				}
			} else {
				const rawTop = triggerRect.top - MENU_OFFSET - height;
				const minTop = VIEWPORT_GUTTER;
				const maxTop = viewportHeight - VIEWPORT_GUTTER - height;
				if (maxTop < minTop) {
					top = minTop;
				} else {
					top = Math.max(Math.min(rawTop, maxTop), minTop);
				}
			}

			return { placement, top, height, maxHeight };
		}

		const belowMetrics = computePlacement('bottom');
		const aboveMetrics = computePlacement('top');

		let metrics = belowMetrics;
		if (scrollHeight > belowMetrics.maxHeight && aboveMetrics.maxHeight > belowMetrics.maxHeight) {
			metrics = aboveMetrics;
		}

		menuRef.style.maxHeight = metrics.maxHeight > 0 ? `${Math.round(metrics.maxHeight)}px` : '';

		let left = triggerRect.right - width;
		const maxLeft = viewportWidth - VIEWPORT_GUTTER - width;
		if (maxLeft < VIEWPORT_GUTTER) {
			left = VIEWPORT_GUTTER;
		} else {
			if (left > maxLeft) {
				left = maxLeft;
			}
			if (left < VIEWPORT_GUTTER) {
				left = VIEWPORT_GUTTER;
			}
		}

		menuPosition = {
			top: Math.round(metrics.top),
			left: Math.round(left),
			width: Math.round(width),
			placement: metrics.placement,
			maxHeight: Math.round(metrics.maxHeight)
		};
	}

	function getDisplayOption(): ModelOption | undefined {
		if (activeId) {
			return options.find((option) => option.id === activeId);
		}
		return undefined;
	}
</script>

<svelte:window onresize={handleResize} onscroll={handleScroll} />

<svelte:document onpointerdown={handlePointerDown} onkeydown={handleKeydown} />

<div
	class={cn('relative z-10 flex max-w-[200px] min-w-[120px] flex-col items-end gap-1', className)}
	bind:this={container}
>
	{#if loading && options.length === 0 && !isMounted}
		<div class="flex items-center gap-2 text-xs text-muted-foreground">
			<Loader2 class="h-4 w-4 animate-spin" />
			Loading models…
		</div>
	{:else if options.length === 0}
		<p class="text-xs text-muted-foreground">No models available.</p>
	{:else}
		{@const selectedOption = getDisplayOption()}

		<div class="relative w-full">
			<button
				type="button"
				class={cn(
					'flex w-full items-center justify-end gap-2 rounded-md px-2 py-1.5 text-sm text-muted-foreground transition hover:text-foreground focus:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 disabled:cursor-not-allowed disabled:opacity-60',
					isOpen ? 'text-foreground' : ''
				)}
				aria-haspopup="listbox"
				aria-expanded={isOpen}
				onclick={toggleOpen}
				bind:this={triggerButton}
				disabled={loading || updating}
			>
				<Box class="h-4 w-4 shrink-0 text-muted-foreground" aria-hidden="true" />
				<span class="max-w-[160px] truncate text-right font-medium">
					{selectedOption?.name || 'Select model'}
				</span>
				{#if (selectedOption || activeId) && !updating}
					<span class="h-2.5 w-2.5 shrink-0 rounded-full bg-emerald-500 dark:bg-emerald-400" aria-hidden="true" title="Model loaded"></span>
				{/if}
				{#if updating}
					<Loader2 class="h-3.5 w-3.5 shrink-0 animate-spin text-muted-foreground" />
				{:else}
					<ChevronDown
						class={cn(
							'h-4 w-4 shrink-0 text-muted-foreground transition-transform',
							isOpen ? 'rotate-180 text-foreground' : ''
						)}
					/>
				{/if}
			</button>

			{#if isOpen}
				<div
					bind:this={menuRef}
					use:portalToBody
					class={cn(
						'fixed z-[1000] flex flex-col overflow-hidden rounded-md border bg-popover shadow-lg transition-opacity',
						menuPosition ? 'opacity-100' : 'pointer-events-none opacity-0'
					)}
					role="listbox"
					style:top={menuPosition ? `${menuPosition.top}px` : undefined}
					style:left={menuPosition ? `${menuPosition.left}px` : undefined}
					style:width={menuPosition ? `${menuPosition.width}px` : undefined}
					data-placement={menuPosition?.placement ?? 'bottom'}
				>
					<div class="border-b border-border/50 p-2">
						<div class="relative flex items-center">
							<Search class="absolute left-2.5 h-4 w-4 text-muted-foreground" aria-hidden="true" />
							<input
								type="text"
								class="h-9 w-full rounded-md border bg-background py-1.5 pl-8 pr-8 text-sm placeholder:text-muted-foreground focus:outline-none focus:ring-2 focus:ring-ring"
								placeholder="Search models..."
								bind:value={searchQuery}
								onkeydown={(e) => e.stopPropagation()}
							/>
							{#if searchQuery}
								<button
									type="button"
									class="absolute right-2 rounded p-0.5 text-muted-foreground hover:bg-muted hover:text-foreground"
									aria-label="Clear search"
									onclick={() => (searchQuery = '')}
								>
									<X class="h-4 w-4" />
								</button>
							{/if}
						</div>
					</div>
					<div
						class="overflow-y-auto py-1"
						style:max-height={menuPosition && menuPosition.maxHeight > 0
							? `${menuPosition.maxHeight}px`
							: undefined}
					>
						{#each filteredOptions as option (option.id)}
							{@const isSelected = option.id === selectedOption?.id}
							{@const isLoadingThis = option.id === loadingId && updating}
							<div
								class={cn(
									'flex w-full items-center gap-2 px-3 py-2 text-left text-sm transition',
									isSelected
										? 'bg-emerald-500/10 text-foreground dark:bg-emerald-500/20'
										: 'hover:bg-muted'
								)}
							>
								<button
									type="button"
									class="flex min-w-0 flex-1 flex-col items-start gap-0.5 text-left focus:bg-muted focus:outline-none"
									role="option"
									aria-selected={isSelected}
									onclick={() => handleOptionSelect(option.id)}
									disabled={isLoadingThis}
								>
									<span class="block w-full truncate font-medium" title={option.name}>
										{option.name}
									</span>
									{#if option.description}
										<span class="text-xs text-muted-foreground">{option.description}</span>
									{/if}
									{#if isLoadingThis}
										<span class="mt-1 flex items-center gap-1.5 text-xs text-muted-foreground">
											<Loader2 class="h-3.5 w-3.5 animate-spin" />
											Loading model...
										</span>
									{/if}
								</button>
								{#if isSelected && !updating}
									<span class="flex h-8 w-8 shrink-0 items-center justify-center" aria-hidden="true" title="Loaded">
										<span class="h-2.5 w-2.5 rounded-full bg-emerald-500 dark:bg-emerald-400"></span>
									</span>
									<button
										type="button"
										class="flex h-8 w-8 shrink-0 items-center justify-center rounded-md text-destructive hover:bg-destructive/15 focus:outline-none focus:ring-2 focus:ring-ring"
										title="Unload model"
										aria-label="Unload model"
										onclick={handleUnloadModel}
									>
										<Power class="h-4 w-4" />
									</button>
								{:else if isLoadingThis}
									<span class="flex h-8 w-8 shrink-0 items-center justify-center text-muted-foreground" title="Loading model...">
										<Loader2 class="h-4 w-4 animate-spin" />
									</span>
								{:else}
									<span class="flex h-8 w-8 shrink-0 items-center justify-center" aria-hidden="true" title="Available">
										<span class="h-2 w-2 rounded-full bg-muted-foreground/50"></span>
									</span>
								{/if}
							</div>
						{/each}
						{#if filteredOptions.length === 0}
							<p class="px-3 py-4 text-center text-sm text-muted-foreground">No models match your search.</p>
						{/if}
					</div>
				</div>
			{/if}
		</div>
	{/if}

	{#if error}
		<p class="text-xs text-destructive">{error}</p>
	{/if}
</div>

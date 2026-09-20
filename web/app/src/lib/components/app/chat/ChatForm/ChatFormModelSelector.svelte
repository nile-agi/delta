<script lang="ts">
	import { onMount, tick } from 'svelte';
	import { ChevronDown, Loader2, Box, Search, X, Power, AlertTriangle, Check, Settings, Zap } from '@lucide/svelte';
	import { cn } from '$lib/components/ui/utils';
	import { portalToBody } from '$lib/utils/portal-to-body';
	import { ModelsService } from '$lib/services/models';
	import { Button } from '$lib/components/ui/button';
	import ModelEfficiencyPanel from './ModelEfficiencyPanel.svelte';
	import {
		fetchModels,
		loadingModelId,
		modelDropdownTrigger,
		modelLoadedOnServer,
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
	let loadedOnServer = $derived(modelLoadedOnServer());

	let isMounted = $state(false);
	let isOpen = $state(false);
	let searchQuery = $state('');
	let container: HTMLDivElement | null = null;
	let triggerButton = $state<HTMLButtonElement | null>(null);
	let menuRef = $state<HTMLDivElement | null>(null);

	// DHATS: State for efficiency panel and context dialog
	let showEfficiencyPanel = $state(false);
	let modelEfficiency = $state<any>(null);
	let showContextDialog = $state(false);
	let contextDialogData = $state<any>(null);

	// DHATS: Pre-loaded compatibility map (model name -> compat data)
	let compatMap = $state<Map<string, any>>(new Map());
	let compatLoading = $state(false);

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

	// Load compatibility data for all installed models (runs in background)
	async function loadCompatibility() {
		if (compatLoading) return;
		compatLoading = true;
		try {
			const res = await fetch('/api/v1/models/list');
			if (!res.ok) return;
			const data = await res.json();
			const m = new Map<string, any>();
			for (const model of data.models || []) {
				m.set(model.name, {
					can_run: model.can_run ?? true,
					efficient: model.efficient ?? true,
					suggested_context: model.suggested_context ?? 0,
					warning: model.compat_warning ?? '',
					display_name: model.display_name ?? model.name,
					size_bytes: model.size_bytes ?? 0
				});
			}
			compatMap = m;
		} catch (e) {
			// Fail open — if compat data can't be loaded, allow all models
			console.error('Failed to load compatibility data:', e);
		} finally {
			compatLoading = false;
		}
	}

	onMount(async () => {
		try {
			await fetchModels();
		} catch (error) {
			console.error('Unable to load models:', error);
		} finally {
			isMounted = true;
		}
		// Load compatibility in the background
		void loadCompatibility();
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
			if (showContextDialog) {
				showContextDialog = false;
			} else if (showEfficiencyPanel) {
				showEfficiencyPanel = false;
			} else {
				closeMenu();
			}
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

		// Refresh compatibility data each time the menu opens
		void loadCompatibility();

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

	let lastOpenTrigger = $state(0);
	let lastGlobalTrigger = $state(0);

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
		const trigger = modelDropdownTrigger();
		if (trigger > lastGlobalTrigger && options.length > 0 && !loading && !updating) {
			lastGlobalTrigger = trigger;
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

	// DHATS: Check model compatibility — uses pre-loaded map first, falls back to API
	async function checkModelCompatibility(modelName: string): Promise<{
		canRun: boolean;
		efficient: boolean;
		suggestedCtx?: number;
		warning?: string;
		recommendation?: string;
	}> {
		// Try pre-loaded data first (instant, no network call)
		const cached = compatMap.get(modelName);
		if (cached) {
			if (!cached.can_run) {
				return {
					canRun: false,
					efficient: false,
					suggestedCtx: cached.suggested_context,
					warning: cached.warning,
					recommendation: cached.suggested_context > 0
						? `Reduce context to ${cached.suggested_context} or less. Open Model Management to adjust.`
						: 'Use a smaller model or lower quantization.'
				};
			}
			return {
				canRun: true,
				efficient: cached.efficient,
				warning: cached.warning,
				recommendation: cached.efficient
					? 'Runs well on your hardware'
					: 'May run slower — close other apps for best performance'
			};
		}

		// Fallback: live API check (for models not in the installed list yet)
		try {
			const res = await fetch(`/api/v1/models/${encodeURIComponent(modelName)}/compatibility?ctx=8192`);
			if (!res.ok) {
				return { canRun: true, efficient: true };
			}

			const data = await res.json();

			if (!data.can_run) {
				return {
					canRun: false,
					efficient: false,
					suggestedCtx: data.suggested_context,
					warning: data.warning,
					recommendation: data.recommendation
				};
			}

			return {
				canRun: true,
				efficient: data.efficient,
				warning: data.warning,
				recommendation: data.recommendation
			};
		} catch (e) {
			console.error('Failed to check compatibility:', e);
			return { canRun: true, efficient: true };
		}
	}

	// Handle model selection with DHATS compatibility check
	async function handleOptionSelect(optionId: string) {
		const option = options.find((opt) => opt.id === optionId);
		if (!option) return;

		// Check compatibility before selecting
		const check = await checkModelCompatibility(option.name);

		if (!check.canRun) {
			// Model cannot run — show context adjustment dialog if context is the issue
			if (check.suggestedCtx && check.suggestedCtx > 0) {
				contextDialogData = {
					model: option,
					suggestedCtx: check.suggestedCtx,
					warning: check.warning,
					recommendation: check.recommendation
				};
				showContextDialog = true;
			} else {
				// No context fix available — show efficiency panel with all models
				modelEfficiency = {
					model_name: option.name,
					display_name: option.name,
					can_run: false,
					warning: check.warning,
					recommendation: check.recommendation
				};
				showEfficiencyPanel = true;
			}
			closeMenu();
			return;
		}

		// Show warning if inefficient but allow selection
		if (!check.efficient && check.warning) {
			const proceed = confirm(
				`Performance Warning:\n\n${check.warning}\n\n${check.recommendation}\n\nDo you want to proceed?`
			);
			if (!proceed) {
				closeMenu();
				return;
			}
		}

		// Proceed with model selection
		try {
			await handleSelect(optionId);
		} finally {
			closeMenu();
		}
	}

	// Handle context adjustment from dialog
	async function handleContextAdjustment() {
		if (!contextDialogData) return;

		const { model, suggestedCtx } = contextDialogData;

		try {
			// Set the context override
			await fetch('/api/models/context', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({
					model: model.name,
					ctx_size: suggestedCtx
				})
			});

			// Refresh compatibility data since context changed
			void loadCompatibility();

			// Now try to select the model again
			await handleSelect(model.id);
			showContextDialog = false;
			closeMenu();
		} catch (e) {
			console.error('Failed to adjust context:', e);
			alert('Failed to adjust context size. Please try again.');
		}
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
				{#if updating}
					<span class="h-2.5 w-2.5 shrink-0 flex items-center justify-center" title="Loading model...">
						<Loader2 class="h-2.5 w-2.5 animate-spin text-muted-foreground" />
					</span>
				{:else if (selectedOption || activeId) && loadedOnServer}
					<span
						class="h-2.5 w-2.5 shrink-0 rounded-full bg-emerald-500 dark:bg-emerald-400"
						aria-hidden="true"
						title="Model loaded"
					></span>
				{/if}
				{#if !updating}
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
								class="h-9 w-full rounded-md border bg-background py-1.5 pr-8 pl-8 text-sm placeholder:text-muted-foreground focus:ring-2 focus:ring-ring focus:outline-none"
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
						class="min-h-0 flex-1 overflow-y-auto py-1"
						style:max-height={menuPosition && menuPosition.maxHeight > 0
							? `${menuPosition.maxHeight}px`
							: undefined}
					>
						{#each filteredOptions as option (option.id)}
							{@const isSelected = option.id === selectedOption?.id}
							{@const isLoadingThis = option.id === loadingId && updating}
							{@const compat = compatMap.get(option.name)}
							{@const cannotRun = compat ? compat.can_run === false : false}
							{@const isSlow = compat ? (compat.can_run !== false && !compat.efficient) : false}
							{@const runsWell = compat ? (compat.can_run !== false && compat.efficient) : false}
							<div
								class={cn(
									'flex w-full items-center gap-2 px-3 py-2 text-left text-sm transition',
									isSelected
										? 'bg-emerald-500/10 text-foreground dark:bg-emerald-500/20'
										: cannotRun
											? 'hover:bg-muted/50 opacity-60'
											: 'hover:bg-muted'
								)}
							>
								<button
									type="button"
									class={cn(
										'flex min-w-0 flex-1 flex-col items-start gap-0.5 text-left focus:outline-none',
										cannotRun ? 'cursor-not-allowed' : 'focus:bg-muted'
									)}
									role="option"
									aria-selected={isSelected}
									aria-disabled={cannotRun}
									onclick={() => handleOptionSelect(option.id)}
									disabled={isLoadingThis}
								>
									<div class="flex w-full items-center gap-2">
										<span class="block truncate font-medium" title={option.name}>
											{option.name}
										</span>
										{#if cannotRun}
											<span class="shrink-0 inline-flex items-center gap-0.5 rounded-full border border-red-500/40 bg-red-500/15 px-1.5 py-0.5 text-[10px] font-medium text-red-500 dark:text-red-400">
												<X class="h-2.5 w-2.5" />
												Can't run
											</span>
										{:else if isSlow}
											<span class="shrink-0 inline-flex items-center gap-0.5 rounded-full border border-amber-500/40 bg-amber-500/15 px-1.5 py-0.5 text-[10px] font-medium text-amber-600 dark:text-amber-400">
												<Zap class="h-2.5 w-2.5" />
												Slow
											</span>
										{:else if runsWell}
											<span class="shrink-0 inline-flex items-center gap-0.5 rounded-full border border-emerald-500/40 bg-emerald-500/15 px-1.5 py-0.5 text-[10px] font-medium text-emerald-600 dark:text-emerald-400">
												<Check class="h-2.5 w-2.5" />
												Runs well
											</span>
										{/if}
									</div>
									{#if cannotRun && compat?.warning}
										<span class="line-clamp-1 text-xs text-red-400/80 dark:text-red-300/70">
											{compat.warning}
										</span>
									{:else if isSlow && compat?.warning}
										<span class="line-clamp-1 text-xs text-amber-500/80 dark:text-amber-300/70">
											{compat.warning}
										</span>
									{:else if option.description}
										<span class="line-clamp-2 text-xs leading-snug text-muted-foreground" title={option.description}>
											{option.description}
										</span>
									{/if}
									{#if isLoadingThis}
										<span class="mt-1 flex items-center gap-1.5 text-xs text-muted-foreground">
											<Loader2 class="h-3.5 w-3.5 animate-spin" />
											Loading model...
										</span>
									{/if}
								</button>
								{#if isSelected && loadedOnServer && !updating}
									<span
										class="flex h-8 w-8 shrink-0 items-center justify-center"
										aria-hidden="true"
										title="Loaded"
									>
										<span class="h-2.5 w-2.5 rounded-full bg-emerald-500 dark:bg-emerald-400"></span>
									</span>
									<button
										type="button"
										class="flex h-8 w-8 shrink-0 items-center justify-center rounded-md text-destructive hover:bg-destructive/15 focus:ring-2 focus:ring-ring focus:outline-none"
										title="Unload model"
										aria-label="Unload model"
										onclick={handleUnloadModel}
									>
										<Power class="h-4 w-4" />
									</button>
								{:else if isLoadingThis}
									<span
										class="flex h-8 w-8 shrink-0 items-center justify-center text-muted-foreground"
										title="Loading model..."
									>
										<Loader2 class="h-4 w-4 animate-spin" />
									</span>
								{:else}
									<span
										class="flex h-8 w-8 shrink-0 items-center justify-center"
										aria-hidden="true"
										title={cannotRun ? 'Cannot run' : 'Available'}
									>
										<span class={cn(
											'h-2 w-2 rounded-full',
											cannotRun ? 'bg-red-500/50' : 'bg-muted-foreground/50'
										)}></span>
									</span>
								{/if}
							</div>
						{/each}
						{#if filteredOptions.length === 0}
							<p class="px-3 py-4 text-center text-sm text-muted-foreground">
								No models match your search.
							</p>
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

<!-- DHATS: Efficiency Warning Banner -->
{#if modelEfficiency && !modelEfficiency.can_run}
	<div class="fixed bottom-4 left-1/2 -translate-x-1/2 z-[1100] max-w-md w-full mx-4">
		<div class="rounded-lg border border-red-500/30 bg-red-500/10 backdrop-blur-sm shadow-xl p-4">
			<div class="flex items-start gap-3">
				<AlertTriangle class="h-5 w-5 text-red-500 shrink-0 mt-0.5" />
				<div class="flex-1 min-w-0">
					<h3 class="font-semibold text-red-700 dark:text-red-400 text-sm">
						Model Cannot Run
					</h3>
					<p class="text-xs text-muted-foreground mt-1">
						{modelEfficiency.warning || 'Insufficient resources'}
					</p>
					<div class="flex gap-2 mt-3">
						<Button
							size="sm"
							variant="ghost"
							class="h-7 text-xs"
							onclick={() => { showEfficiencyPanel = true; modelEfficiency = null; }}
						>
							View All Models
						</Button>
						<Button
							size="sm"
							variant="ghost"
							class="h-7 text-xs"
							onclick={() => { modelEfficiency = null; }}
						>
							Dismiss
						</Button>
					</div>
				</div>
				<button
					type="button"
					class="text-muted-foreground hover:text-foreground shrink-0"
					onclick={() => { modelEfficiency = null; }}
					aria-label="Close"
				>
					<X class="h-4 w-4" />
				</button>
			</div>
		</div>
	</div>
{/if}

<!-- DHATS: Context Adjustment Dialog -->
{#if showContextDialog && contextDialogData}
	<div class="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center z-[1200] p-4"
		onkeydown={(e) => e.key === 'Escape' && (showContextDialog = false)}
		role="dialog"
		aria-modal="true"
		aria-label="Context Size Adjustment"
	>
		<div class="bg-background rounded-lg max-w-lg w-full shadow-2xl">
			<div class="border-b p-4 flex items-center justify-between">
				<h2 class="text-lg font-semibold flex items-center gap-2">
					<AlertTriangle class="h-5 w-5 text-amber-500" />
					Context Size Adjustment Required
				</h2>
				<Button size="icon" variant="ghost" onclick={() => showContextDialog = false}>
					<X class="h-4 w-4" />
				</Button>
			</div>

			<div class="p-6 space-y-4">
				<div>
					<h3 class="font-medium mb-2">{contextDialogData.model.name}</h3>
					<p class="text-sm text-muted-foreground">
						{contextDialogData.warning}
					</p>
				</div>

				<div class="bg-amber-500/10 border border-amber-500/30 rounded-lg p-4">
					<p class="text-sm">
						<strong>Recommendation:</strong> {contextDialogData.recommendation}
					</p>
				</div>

				<div class="bg-blue-500/10 border border-blue-500/30 rounded-lg p-4">
					<p class="text-sm">
						Would you like to reduce the context size to
						<strong>{contextDialogData.suggestedCtx}</strong> tokens and try again?
					</p>
				</div>
			</div>

			<div class="border-t p-4 flex justify-end gap-2">
				<Button variant="outline" onclick={() => showContextDialog = false}>
					Cancel
				</Button>
				<Button onclick={handleContextAdjustment}>
					<Settings class="h-4 w-4 mr-2" />
					Adjust & Load Model
				</Button>
			</div>
		</div>
	</div>
{/if}

<!-- DHATS: Efficiency Panel Modal -->
{#if showEfficiencyPanel}
	<div class="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center z-[1200] p-4"
		onkeydown={(e) => e.key === 'Escape' && (showEfficiencyPanel = false)}
		role="dialog"
		aria-modal="true"
		aria-label="Model Compatibility & Resource Analysis"
	>
		<div class="bg-background rounded-lg max-w-2xl w-full max-h-[80vh] overflow-hidden flex flex-col shadow-2xl">
			<div class="sticky top-0 bg-background border-b p-4 flex items-center justify-between shrink-0">
				<h2 class="text-lg font-semibold">Model Compatibility & Resource Analysis</h2>
				<Button
					size="icon"
					variant="ghost"
					onclick={() => showEfficiencyPanel = false}
					aria-label="Close panel"
				>
					<X class="h-4 w-4" />
				</Button>
			</div>
			<div class="p-4 overflow-y-auto">
				<ModelEfficiencyPanel />
			</div>
		</div>
	</div>
{/if}
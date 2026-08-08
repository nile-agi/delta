<script lang="ts">
	import {
		Settings,
		Funnel,
		AlertTriangle,
		Code,
		Monitor,
		Sun,
		Moon,
		Layout,
		ChevronLeft,
		ChevronRight,
		Database,
		Package,
		User,
		Minus,
		X,
		GripVertical,
		RotateCcw
	} from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import { ChatSettingsFields } from '$lib/components/app';
	import ImportExportTab from './ImportExportTab.svelte';
	import ModelManagementTab from '../ModelManagement/ModelManagementTab.svelte';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import { config, updateConfig, updateMultipleConfig } from '$lib/stores/settings.svelte';
	import {
		settingsWindow,
		SETTINGS_WINDOW_MIN_WIDTH,
		SETTINGS_WINDOW_MIN_HEIGHT,
		SETTINGS_WINDOW_MIN_VISIBLE_X,
		SETTINGS_WINDOW_MIN_VISIBLE_Y
	} from '$lib/stores/settings-window.svelte';
	import { IsMobile } from '$lib/hooks/is-mobile.svelte';
	import { downloads } from '$lib/stores/downloads.svelte';
	import { SETTINGS_WINDOW_FULLBLEED_BREAKPOINT } from '$lib/constants/viewport';
	import { setMode } from 'mode-watcher';
	import { untrack, type Component } from 'svelte';

	interface Props {
		onOpenChange?: (open: boolean) => void;
		open?: boolean;
	}

	let { onOpenChange, open: _open = false }: Props = $props();

	const settingSections: Array<{
		fields: SettingsFieldConfig[];
		icon: Component;
		title: string;
	}> = [
		{
			title: 'You',
			icon: User,
			fields: [
				{ key: 'userName', label: 'What should Delta call you?', type: 'input' },
				{
					key: 'replyStyle',
					label: 'Reply style',
					type: 'select',
					options: [
						{ value: 'concise', label: 'Concise' },
						{ value: 'balanced', label: 'Balanced' },
						{ value: 'detailed', label: 'Detailed' }
					]
				},
				{
					key: 'calendarWeekStart',
					label: 'Week starts on',
					type: 'select',
					options: [
						{ value: 'monday', label: 'Monday' },
						{ value: 'sunday', label: 'Sunday' }
					]
				}
			]
		},
		{
			title: 'General',
			icon: Settings,
			fields: [
				{ key: 'apiKey', label: 'API Key', type: 'input' },
				{
					key: 'systemMessage',
					label: 'System Message (will be disabled if left empty)',
					type: 'textarea'
				},
				{
					key: 'theme',
					label: 'Theme',
					type: 'select',
					options: [
						{ value: 'system', label: 'System', icon: Monitor },
						{ value: 'light', label: 'Light', icon: Sun },
						{ value: 'dark', label: 'Dark', icon: Moon }
					]
				},
				{
					key: 'askForTitleConfirmation',
					label: 'Ask for confirmation before changing conversation title',
					type: 'checkbox'
				},
				{
					key: 'pasteLongTextToFileLen',
					label: 'Paste long text to file length',
					type: 'input'
				},
				{
					key: 'copyTextAttachmentsAsPlainText',
					label: 'Copy text attachments as plain text',
					type: 'checkbox'
				},
				{
					key: 'enableContinueButton',
					label: 'Enable "Continue" button',
					type: 'checkbox'
				},
				{
					key: 'pdfAsImage',
					label: 'Parse PDF as image',
					type: 'checkbox'
				}
			]
		},
		{
			title: 'Display',
			icon: Layout,
			fields: [
				{
					key: 'showMessageStats',
					label: 'Show message generation statistics',
					type: 'checkbox'
				},
				{
					key: 'showThoughtInProgress',
					label: 'Show thought in progress',
					type: 'checkbox'
				},
				{
					key: 'keepStatsVisible',
					label: 'Keep stats visible after generation',
					type: 'checkbox'
				},
				{
					key: 'showMicrophoneOnEmptyInput',
					label: 'Show microphone on empty input',
					type: 'checkbox'
				},
				{
					key: 'renderUserContentAsMarkdown',
					label: 'Render user content as Markdown',
					type: 'checkbox'
				},
				{
					key: 'disableAutoScroll',
					label: 'Disable automatic scroll',
					type: 'checkbox'
				},
				{
					key: 'alwaysShowSidebar',
					label: 'Always show sidebar on desktop',
					type: 'checkbox'
				},
				{
					key: 'autoShowSidebarOnNewChat',
					label: 'Auto-show sidebar on new chat',
					type: 'checkbox'
				}
			]
		},
		{
			title: 'Sampling',
			icon: Funnel,
			fields: [
				{
					key: 'backendSampling',
					label: 'Backend sampling',
					type: 'checkbox'
				},
				{
					key: 'temperature',
					label: 'Temperature',
					type: 'input'
				},
				{
					key: 'dynatemp_range',
					label: 'Dynamic temperature range',
					type: 'input'
				},
				{
					key: 'dynatemp_exponent',
					label: 'Dynamic temperature exponent',
					type: 'input'
				},
				{
					key: 'top_k',
					label: 'Top K',
					type: 'input'
				},
				{
					key: 'top_p',
					label: 'Top P',
					type: 'input'
				},
				{
					key: 'min_p',
					label: 'Min P',
					type: 'input'
				},
				{
					key: 'xtc_probability',
					label: 'XTC probability',
					type: 'input'
				},
				{
					key: 'xtc_threshold',
					label: 'XTC threshold',
					type: 'input'
				},
				{
					key: 'typ_p',
					label: 'Typical P',
					type: 'input'
				},
				{
					key: 'max_tokens',
					label: 'Max tokens',
					type: 'input'
				},
				{
					key: 'samplers',
					label: 'Samplers',
					type: 'input'
				}
			]
		},
		{
			title: 'Penalties',
			icon: AlertTriangle,
			fields: [
				{
					key: 'repeat_last_n',
					label: 'Repeat last N',
					type: 'input'
				},
				{
					key: 'repeat_penalty',
					label: 'Repeat penalty',
					type: 'input'
				},
				{
					key: 'presence_penalty',
					label: 'Presence penalty',
					type: 'input'
				},
				{
					key: 'frequency_penalty',
					label: 'Frequency penalty',
					type: 'input'
				},
				{
					key: 'dry_multiplier',
					label: 'DRY multiplier',
					type: 'input'
				},
				{
					key: 'dry_base',
					label: 'DRY base',
					type: 'input'
				},
				{
					key: 'dry_allowed_length',
					label: 'DRY allowed length',
					type: 'input'
				},
				{
					key: 'dry_penalty_last_n',
					label: 'DRY penalty last N',
					type: 'input'
				}
			]
		},
		{
			title: 'Import/Export',
			icon: Database,
			fields: []
		},
		{
			title: 'Model Management',
			icon: Package,
			fields: []
		},
		{
			title: 'Developer',
			icon: Code,
			fields: [
				{
					key: 'showToolCallLabels',
					label: 'Show tool call labels',
					type: 'checkbox',
					help: 'Display tool call labels and payloads from Harmony-compatible delta.tool_calls data below assistant messages.'
				},
				{
					key: 'disableReasoningFormat',
					label: 'Show raw LLM output',
					type: 'checkbox',
					help: 'Show raw LLM output without backend parsing and frontend Markdown rendering to inspect streaming across different models.'
				},
				{
					key: 'custom',
					label: 'Custom JSON',
					type: 'textarea'
				}
			]
		}
	];

	let activeSection = $state('General');
	let currentSection = $derived(
		settingSections.find((section) => section.title === activeSection) || settingSections[0]
	);
	let localConfig: SettingsConfigType = $state({ ...config() });
	let originalTheme: string = $state('');

	let canScrollLeft = $state(false);
	let canScrollRight = $state(false);
	let scrollContainer: HTMLDivElement | undefined = $state();

	// Drag state
	let isDragging = $state(false);
	let dragOffsetX = $state(0);
	let dragOffsetY = $state(0);

	// Resize state
	let isResizing = $state(false);
	let resizeStartX = $state(0);
	let resizeStartY = $state(0);
	let resizeStartWidth = $state(0);
	let resizeStartHeight = $state(0);

	// Below this width the window drops its floating frame entirely. Dragging and resizing
	// are meaningless there, so the handlers and the resize grip come off with it rather
	// than being left as controls that silently do nothing.
	const fullBleed = new IsMobile(SETTINGS_WINDOW_FULLBLEED_BREAKPOINT);

	function handleThemeChange(newTheme: string) {
		localConfig.theme = newTheme;
		setMode(newTheme as 'light' | 'dark' | 'system');
	}

	function handleConfigChange(key: string, value: string | boolean) {
		localConfig[key] = value;
	}

	function handleClose() {
		if (localConfig.theme !== originalTheme) {
			setMode(originalTheme as 'light' | 'dark' | 'system');
		}
		onOpenChange?.(false);
		settingsWindow.close();
	}

	function handleMinimize() {
		settingsWindow.minimize();
	}

	function runSetupAgain() {
		updateConfig('onboardingCompleted', false);
		handleClose();
	}

	function handleReset() {
		localConfig = { ...config() };
		setMode(localConfig.theme as 'light' | 'dark' | 'system');
		originalTheme = localConfig.theme as string;
	}

	function handleSave() {
		if (localConfig.custom && typeof localConfig.custom === 'string' && localConfig.custom.trim()) {
			try {
				JSON.parse(localConfig.custom);
			} catch (error) {
				alert('Invalid JSON in custom parameters. Please check the format and try again.');
				console.error(error);
				return;
			}
		}

		const processedConfig = { ...localConfig };
		const numericFields = [
			'temperature',
			'top_k',
			'top_p',
			'min_p',
			'max_tokens',
			'pasteLongTextToFileLen',
			'dynatemp_range',
			'dynatemp_exponent',
			'typ_p',
			'xtc_probability',
			'xtc_threshold',
			'repeat_last_n',
			'repeat_penalty',
			'presence_penalty',
			'frequency_penalty',
			'dry_multiplier',
			'dry_base',
			'dry_allowed_length',
			'dry_penalty_last_n'
		];

		for (const field of numericFields) {
			if (processedConfig[field] !== undefined && processedConfig[field] !== '') {
				const numValue = Number(processedConfig[field]);
				if (!isNaN(numValue)) {
					processedConfig[field] = numValue;
				} else {
					alert(`Invalid numeric value for ${field}. Please enter a valid number.`);
					return;
				}
			}
		}

		updateMultipleConfig(processedConfig);
		originalTheme = localConfig.theme as string;
	}

	function scrollToCenter(element: HTMLElement) {
		if (!scrollContainer) return;
		const containerRect = scrollContainer.getBoundingClientRect();
		const elementRect = element.getBoundingClientRect();
		const elementCenter = elementRect.left + elementRect.width / 2;
		const containerCenter = containerRect.left + containerRect.width / 2;
		const scrollOffset = elementCenter - containerCenter;
		scrollContainer.scrollBy({ left: scrollOffset, behavior: 'smooth' });
	}

	function scrollLeft() {
		if (!scrollContainer) return;
		scrollContainer.scrollBy({ left: -250, behavior: 'smooth' });
	}

	function scrollRight() {
		if (!scrollContainer) return;
		scrollContainer.scrollBy({ left: 250, behavior: 'smooth' });
	}

	function updateScrollButtons() {
		if (!scrollContainer) return;
		const { scrollLeft, scrollWidth, clientWidth } = scrollContainer;
		canScrollLeft = scrollLeft > 0;
		canScrollRight = scrollLeft < scrollWidth - clientWidth - 1;
	}

	// Drag and resize both use pointer capture rather than document-level listeners: the
	// capturing element keeps receiving events even when the pointer leaves the viewport, so
	// a button released outside the window still ends the gesture instead of leaving the
	// frame stuck to the cursor.
	function releaseCapture(e: PointerEvent) {
		const el = e.currentTarget as HTMLElement | null;
		if (el?.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
	}

	// Drag handlers
	function onDragStart(e: PointerEvent) {
		if (fullBleed.current || isResizing) return;
		// Minimize and close sit inside the title bar; their clicks must not become drags.
		if ((e.target as HTMLElement).closest('button')) return;
		// Without this Gecko starts its own selection/native-drag session on the title bar.
		e.preventDefault();
		isDragging = true;
		dragOffsetX = e.clientX - settingsWindow.state.x;
		dragOffsetY = e.clientY - settingsWindow.state.y;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onDragMove(e: PointerEvent) {
		if (!isDragging) return;
		const maxX = window.innerWidth - SETTINGS_WINDOW_MIN_VISIBLE_X;
		const maxY = window.innerHeight - SETTINGS_WINDOW_MIN_VISIBLE_Y;
		const newX = Math.max(0, Math.min(e.clientX - dragOffsetX, maxX));
		const newY = Math.max(0, Math.min(e.clientY - dragOffsetY, maxY));
		// Don't persist per pointer sample — save() is a synchronous localStorage write.
		settingsWindow.setPosition(newX, newY, false);
	}

	function onDragEnd(e: PointerEvent) {
		if (!isDragging) return;
		isDragging = false;
		releaseCapture(e);
		settingsWindow.commit();
	}

	// Resize handlers
	function onResizeStart(e: PointerEvent) {
		if (fullBleed.current) return;
		e.preventDefault();
		isResizing = true;
		resizeStartX = e.clientX;
		resizeStartY = e.clientY;
		resizeStartWidth = settingsWindow.state.width;
		resizeStartHeight = settingsWindow.state.height;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onResizeMove(e: PointerEvent) {
		if (!isResizing) return;
		const newWidth = Math.max(
			SETTINGS_WINDOW_MIN_WIDTH,
			resizeStartWidth + (e.clientX - resizeStartX)
		);
		const newHeight = Math.max(
			SETTINGS_WINDOW_MIN_HEIGHT,
			resizeStartHeight + (e.clientY - resizeStartY)
		);
		settingsWindow.setSize(newWidth, newHeight, false);
	}

	function onResizeEnd(e: PointerEvent) {
		if (!isResizing) return;
		isResizing = false;
		releaseCapture(e);
		settingsWindow.commit();
	}

	// Persisted geometry outlives the viewport it was saved in, so a frame from a wider
	// display (or one left behind by full-bleed mode) has to be pulled back into view.
	function clampIfFloating() {
		if (fullBleed.current) return;
		if (!settingsWindow.state.open || settingsWindow.state.minimized) return;
		// Never fight a gesture in progress — onDragMove/onResizeMove own the geometry then.
		if (isDragging || isResizing) return;
		settingsWindow.clampToViewport();
	}

	$effect(() => {
		if (settingsWindow.state.open && !settingsWindow.state.minimized) {
			localConfig = { ...config() };
			originalTheme = config().theme as string;
			setTimeout(updateScrollButtons, 100);
		}
	});

	$effect(() => {
		if (scrollContainer) {
			updateScrollButtons();
		}
	});

	// Lets other surfaces (e.g. the download pill) open settings straight to a section.
	$effect(() => {
		const pending = settingsWindow.pendingSection;
		if (!pending) return;
		activeSection = pending;
		settingsWindow.pendingSection = null;
	});

	// Runs on open and whenever the window crosses back above the full-bleed breakpoint.
	// untrack keeps clampToViewport's own reads of x/y/width/height out of the dependency
	// set: it writes those, so tracking them would re-run this effect on every pointer
	// sample of a drag.
	$effect(() => {
		void fullBleed.current;
		void settingsWindow.state.open;
		void settingsWindow.state.minimized;
		untrack(clampIfFloating);
	});
</script>

<svelte:window onresize={clampIfFloating} />

<!-- Nudge on the Model Management entry while downloads are running. -->
{#snippet sectionBadge(title: string, extraClass: string)}
	{#if title === 'Model Management' && downloads.activeCount > 0}
		<span
			class="flex h-5 min-w-5 items-center justify-center rounded-full bg-primary px-1.5 text-[0.6875rem] font-semibold text-primary-foreground {extraClass}"
			aria-label="{downloads.activeCount} download{downloads.activeCount === 1 ? '' : 's'} in progress"
		>
			{downloads.activeCount}
		</span>
	{/if}
{/snippet}

{#if settingsWindow.state.open && !settingsWindow.state.minimized}
	<!-- svelte-ignore a11y_no_static_element_interactions -->
	<div
		class="settings-floating-window"
		class:full-bleed={fullBleed.current}
		style={fullBleed.current
			? ''
			: `left: ${settingsWindow.state.x}px; top: ${settingsWindow.state.y}px; width: ${settingsWindow.state.width}px; height: ${settingsWindow.state.height}px;`}
	>
		<!-- Window Title Bar (draggable above the full-bleed breakpoint) -->
		<div
			class="settings-window-titlebar"
			class:draggable={!fullBleed.current}
			onpointerdown={fullBleed.current ? undefined : onDragStart}
			onpointermove={fullBleed.current ? undefined : onDragMove}
			onpointerup={fullBleed.current ? undefined : onDragEnd}
			onpointercancel={fullBleed.current ? undefined : onDragEnd}
		>
			<div class="flex items-center gap-2 select-none">
				<GripVertical class="h-4 w-4 text-muted-foreground" />
				<span class="text-sm font-semibold">Settings</span>
			</div>
			<div class="flex items-center gap-1">
				<button
					class="settings-window-btn"
					onclick={handleMinimize}
					aria-label="Minimize"
					title="Minimize"
				>
					<Minus class="h-3.5 w-3.5" />
				</button>
				<button
					class="settings-window-btn"
					onclick={handleClose}
					aria-label="Close"
					title="Close"
				>
					<X class="h-3.5 w-3.5" />
				</button>
			</div>
		</div>

		<!-- Window Body -->
		<div class="settings-window-body">
			<div class="flex h-full flex-col overflow-hidden md:flex-row">
				<!-- Desktop Sidebar -->
				<div class="hidden w-56 shrink-0 border-r border-border/30 p-4 md:block overflow-y-auto">
					<nav class="space-y-1 py-2">
						{#each settingSections as section (section.title)}
							<button
								class="flex w-full cursor-pointer items-center gap-3 rounded-lg px-3 py-2 text-left text-sm transition-colors hover:bg-accent {activeSection ===
								section.title
									? 'bg-accent text-accent-foreground'
									: 'text-muted-foreground'}"
								onclick={() => (activeSection = section.title)}
							>
								<section.icon class="h-4 w-4" />
								<span class="ml-2">{section.title}</span>
								{@render sectionBadge(section.title, 'ml-auto')}
							</button>
						{/each}
					</nav>
				</div>

				<!-- Mobile Header with Horizontal Scrollable Menu -->
				<div class="flex flex-col md:hidden">
					<div class="border-b border-border/30 py-4">
						<h3 class="mb-4 flex items-center gap-2 px-4 text-lg font-semibold">Settings</h3>

						<div class="relative flex items-center" style="scroll-padding: 1rem;">
							<button
								class="absolute left-2 z-10 flex h-6 w-6 items-center justify-center rounded-full bg-muted shadow-md backdrop-blur-sm transition-opacity hover:bg-accent {canScrollLeft
									? 'opacity-100'
									: 'pointer-events-none opacity-0'}"
								onclick={scrollLeft}
								aria-label="Scroll left"
							>
								<ChevronLeft class="h-4 w-4" />
							</button>

							<div
								class="scrollbar-hide overflow-x-auto py-2"
								bind:this={scrollContainer}
								onscroll={updateScrollButtons}
							>
								<div class="flex min-w-max gap-2">
									{#each settingSections as section (section.title)}
										<button
											class="flex cursor-pointer items-center gap-2 rounded-lg px-3 py-2 text-sm whitespace-nowrap transition-colors first:ml-4 last:mr-4 hover:bg-accent {activeSection ===
											section.title
												? 'bg-accent text-accent-foreground'
												: 'text-muted-foreground'}"
											onclick={(e: MouseEvent) => {
												activeSection = section.title;
												scrollToCenter(e.currentTarget as HTMLElement);
											}}
										>
											<section.icon class="h-4 w-4 flex-shrink-0" />
											<span>{section.title}</span>
											{@render sectionBadge(section.title, '')}
										</button>
									{/each}
								</div>
							</div>

							<button
								class="absolute right-2 z-10 flex h-6 w-6 items-center justify-center rounded-full bg-muted shadow-md backdrop-blur-sm transition-opacity hover:bg-accent {canScrollRight
									? 'opacity-100'
									: 'pointer-events-none opacity-0'}"
								onclick={scrollRight}
								aria-label="Scroll right"
							>
								<ChevronRight class="h-4 w-4" />
							</button>
						</div>
					</div>
				</div>

				{#if currentSection.title === 'Model Management'}
					<div class="flex min-h-0 flex-1 flex-col overflow-hidden p-4 md:p-6">
						<div class="mb-4 hidden shrink-0 items-center gap-2 border-b border-border/30 pb-4 md:flex">
							<currentSection.icon class="h-5 w-5" />
							<h3 class="text-lg font-semibold">{currentSection.title}</h3>
						</div>
						<p class="mb-4 shrink-0 text-sm text-muted-foreground">
							Manage your installed models and download new ones. Use the model selector in the chat
							input to choose models in the chat interface.
						</p>
						<ModelManagementTab />
					</div>
				{:else}
					<div class="flex min-h-0 flex-1 flex-col overflow-hidden">
						<div class="hidden shrink-0 items-center gap-2 border-b border-border/30 px-4 pt-4 pb-4 md:flex md:px-6 md:pt-6">
							<currentSection.icon class="h-5 w-5" />
							<h3 class="text-lg font-semibold">{currentSection.title}</h3>
						</div>
						<ScrollArea class="min-h-0 flex-1">
							<div class="space-y-6 p-4 md:p-6">
								{#if currentSection.title === 'Import/Export'}
									<ImportExportTab />
								{:else if currentSection.title === 'Developer'}
									<div class="space-y-6">
										<ChatSettingsFields
											fields={currentSection.fields}
											{localConfig}
											onConfigChange={handleConfigChange}
											onThemeChange={handleThemeChange}
										/>
									</div>
								{:else}
									<div class="space-y-6">
										<ChatSettingsFields
											fields={currentSection.fields}
											{localConfig}
											onConfigChange={handleConfigChange}
											onThemeChange={handleThemeChange}
										/>
									</div>
								{/if}

								{#if currentSection.title === 'You'}
									<div class="border-t pt-6">
										<button
											class="text-sm text-muted-foreground underline-offset-4 hover:text-foreground hover:underline"
											onclick={runSetupAgain}
										>
											Run setup again
										</button>
										<p class="mt-1 text-xs text-muted-foreground">
											Walks you back through the questions from your first launch.
										</p>
									</div>
								{/if}

								<div class="mt-8 border-t pt-6">
									<p class="text-xs text-muted-foreground">
										Settings are saved in browser's localStorage
									</p>
								</div>
							</div>
						</ScrollArea>
					</div>
				{/if}
			</div>
		</div>

		<!-- Footer -->
		<div class="settings-window-footer">
			<Button variant="outline" onclick={handleReset}>
				<RotateCcw class="mr-2 h-4 w-4" />
				Reset to default
			</Button>
			<Button variant="default" onclick={handleSave}>
				Save settings
			</Button>
		</div>

		<!-- Resize Handle -->
		{#if !fullBleed.current}
			<div
				class="settings-resize-handle"
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
	.settings-floating-window {
		position: fixed;
		z-index: 99999;
		display: flex;
		flex-direction: column;
		border-radius: 0.75rem;
		border: 1px solid color-mix(in oklch, var(--border) 30%, transparent);
		background-color: var(--background);
		box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
		overflow: hidden;
	}

	/* Minimums apply to the floating frame only — full-bleed follows the viewport. */
	.settings-floating-window:not(.full-bleed) {
		min-width: 400px;
		min-height: 300px;
	}

	/*
	 * Below SETTINGS_WINDOW_FULLBLEED_BREAKPOINT the component stops writing inline
	 * left/top/width/height, so this needs no !important to win — and nothing here can
	 * silently discard a drag the way the old media query did.
	 */
	.settings-floating-window.full-bleed {
		inset: 0.5rem;
		width: auto;
		height: auto;
	}

	.settings-window-titlebar {
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

	/* Show the grab affordance only where dragging actually does something. */
	.settings-window-titlebar.draggable {
		cursor: grab;
		touch-action: none;
	}

	.settings-window-titlebar.draggable:active {
		cursor: grabbing;
	}

	.settings-window-btn {
		display: flex;
		align-items: center;
		justify-content: center;
		width: 1.75rem;
		height: 1.75rem;
		border-radius: 0.375rem;
		color: var(--muted-foreground);
		transition: background-color 0.15s, color 0.15s;
	}

	.settings-window-btn:hover {
		background-color: var(--accent);
		color: var(--accent-foreground);
	}

	.settings-window-body {
		flex: 1;
		min-height: 0;
		overflow: hidden;
	}

	.settings-window-footer {
		flex-shrink: 0;
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.75rem;
		padding: 0.875rem 1.25rem;
		border-top: 1px solid color-mix(in oklch, var(--border) 30%, transparent);
		background-color: var(--background);
	}

	.settings-resize-handle {
		position: absolute;
		bottom: 0;
		right: 0;
		width: 16px;
		height: 16px;
		cursor: nwse-resize;
		touch-action: none;
		z-index: 10;
	}

	.settings-resize-handle::after {
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

	.settings-resize-handle:hover::after {
		border-color: color-mix(in oklch, var(--muted-foreground) 70%, transparent);
	}
</style>
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
		Waypoints,
		User,
		Wrench,
		ArrowLeft,
		RotateCcw
	} from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import { agentService } from '$lib/services/agent';
	import { ChatSettingsFields } from '$lib/components/app';
	import { CORE_AGENT_TOOL_KEYS } from '$lib/constants/settings-config';
	import ImportExportTab from './ImportExportTab.svelte';
	import ModelManagementTab from '../ModelManagement/ModelManagementTab.svelte';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import { config, updateConfig, updateMultipleConfig } from '$lib/stores/settings.svelte';
	import { downloads } from '$lib/stores/downloads.svelte';
	import { setMode } from 'mode-watcher';
	import { onDestroy, type Component } from 'svelte';
	import { toast } from 'svelte-sonner';

	interface Props {
		section?: string;
		onBack: () => void;
	}

	let { section = 'General', onBack }: Props = $props();

	const settingSections: Array<{
		fields: SettingsFieldConfig[];
		icon: Component;
		title: string;
	}> = [
		{
			title: 'You',
			icon: User,
			fields: [
				{
					key: 'userName',
					label: 'What should Delta call you?',
					type: 'input'
				},
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
			title: 'Agent tools',
			icon: Wrench,
			fields: [
				{
					key: 'useAgentTools',
					label: 'Enable agent tools',
					type: 'checkbox'
				},
				{
					key: 'useCalendarTools',
					label: 'Calendar and tasks',
					type: 'checkbox'
				},
				{
					key: 'useNotesTools',
					label: 'Notes',
					type: 'checkbox'
				},
				{
					key: 'useTaskTools',
					label: 'Planning and self-tracking',
					type: 'checkbox'
				},
				{
					key: 'useMemoryTools',
					label: 'Long-term memory',
					type: 'checkbox'
				},
				{
					key: 'useFileTools',
					label: 'Read and write files',
					type: 'checkbox'
				},
				{
					key: 'useShellTools',
					label: 'Run shell commands',
					type: 'checkbox'
				},
				{
					key: 'resetToolPolicies',
					label: 'Remembered approval answers',
					type: 'action',
					actionLabel: 'Forget all',
					help: 'Answering "Always" or "Never" to a tool approval is remembered across conversations. Forgetting them makes destructive tools ask again.',
					action: async () => {
						const before = Object.keys(await agentService.getToolPolicies()).length;
						await agentService.resetToolPolicies();
						return before === 0
							? 'Nothing was remembered.'
							: `Forgot ${before} remembered answer${before === 1 ? '' : 's'}.`;
					}
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
			icon: Waypoints,
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

	let canScrollLeft = $state(false);
	let canScrollRight = $state(false);
	let scrollContainer: HTMLDivElement | undefined = $state();

	function handleThemeChange(newTheme: string) {
		localConfig.theme = newTheme;
		setMode(newTheme as 'light' | 'dark' | 'system');
	}

	function handleConfigChange(key: string, value: string | boolean) {
		localConfig[key] = value;
		if (key === 'useAgentTools' && value === true) {
			for (const tool of CORE_AGENT_TOOL_KEYS) localConfig[tool] = true;
		}
	}

	onDestroy(() => {
		setMode(config().theme as 'light' | 'dark' | 'system');
	});

	function runSetupAgain() {
		if (!updateConfig('onboardingCompleted', false)) {
			toast.error('Could not restart setup. Please try again.');
			return;
		}
		toast.success('Setup restarted.');
		onBack();
	}

	function handleReset() {
		localConfig = { ...config() };
		setMode(localConfig.theme as 'light' | 'dark' | 'system');
		toast.success('Saved settings restored. Unsaved changes discarded.');
	}

	function handleSave() {
		if (localConfig.custom && typeof localConfig.custom === 'string' && localConfig.custom.trim()) {
			try {
				JSON.parse(localConfig.custom);
			} catch (error) {
				toast.error('Invalid JSON in custom parameters. Please check the format and try again.');
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
				if (Number.isFinite(numValue)) {
					processedConfig[field] = numValue;
				} else {
					toast.error(`Invalid numeric value for ${field}. Please enter a valid number.`);
					return;
				}
			}
		}

		if (updateMultipleConfig(processedConfig)) {
			localConfig = { ...config() };
			toast.success('Settings saved.');
		} else {
			toast.error('Could not save settings. Your changes are still available to retry.');
		}
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

	$effect(() => {
		if (scrollContainer) {
			updateScrollButtons();
		}
	});

	// Route links can open directly to a category; ignore unknown section names.
	$effect(() => {
		activeSection = settingSections.some((item) => item.title === section) ? section : 'General';
	});

	// DHATS: Block status tracking
	interface BlockStatus {
		blocked: boolean;
		model: string;
		reason: string;
		recommendation: string;
		suggested_context: number;
	}

	let blockStatus = $state<BlockStatus | null>(null);

	async function fetchBlockStatus() {
		try {
			const res = await fetch('/api/v1/dhats/block-status');
			if (!res.ok) return;
			const data = await res.json();
			blockStatus = data.blocked ? data : null;
		} catch (e) {
			console.error('Failed to fetch block status:', e);
		}
	}

	async function applySuggestedCtx(status: BlockStatus) {
		if (!status.suggested_context || !status.model) return;
		try {
			const response = await fetch('/api/models/context', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({
					model: status.model,
					ctx_size: status.suggested_context
				})
			});
			if (!response.ok) throw new Error(`Context update failed: ${response.status}`);
			toast.success(`Context updated to ${status.suggested_context}.`);
			// Refresh block status — should now be cleared
			await fetchBlockStatus();
		} catch (e) {
			console.error('Failed to apply context:', e);
			toast.error('Could not update model context. Please try again.');
		}
	}

	// Poll for block status while the Settings page is mounted
	$effect(() => {
		fetchBlockStatus();
		const interval = setInterval(fetchBlockStatus, 2000);
		return () => clearInterval(interval);
	});
</script>

<!-- Nudge on the Model Management entry while downloads are running. -->
{#snippet sectionBadge(title: string, extraClass: string)}
	{#if title === 'Model Management' && downloads.activeCount > 0}
		<span
			class="flex h-5 min-w-5 items-center justify-center rounded-full bg-primary px-1.5 text-[0.6875rem] font-semibold text-primary-foreground {extraClass}"
			aria-label="{downloads.activeCount} download{downloads.activeCount === 1
				? ''
				: 's'} in progress"
		>
			{downloads.activeCount}
		</span>
	{/if}
{/snippet}

<main
	aria-label="Settings"
	class="flex h-full min-h-0 w-full flex-col overflow-hidden bg-background"
>
	<header class="flex shrink-0 items-center gap-4 border-b border-border/30 px-4 py-3 md:px-6">
		<Button
			variant="ghost"
			size="icon"
			aria-label="Back to Home"
			title="Back to Home"
			onclick={onBack}
		>
			<ArrowLeft class="h-4 w-4" />
		</Button>
		<h1 class="text-lg font-semibold">Settings</h1>
	</header>

	<!-- Settings content -->
	<div class="min-h-0 flex-1 overflow-hidden">
		<div class="flex h-full flex-col overflow-hidden md:flex-row">
			<!-- Desktop Sidebar -->
			<div class="hidden w-56 shrink-0 border-r border-border/30 p-4 md:block overflow-y-auto">
				<nav aria-label="Settings categories" class="space-y-1 py-2">
					{#each settingSections as section (section.title)}
						<button
							class="flex w-full cursor-pointer items-center gap-3 rounded-lg px-3 py-2 text-left text-sm transition-colors hover:bg-accent {activeSection ===
							section.title
								? 'bg-accent text-accent-foreground'
								: 'text-muted-foreground'}"
							aria-current={activeSection === section.title ? 'page' : undefined}
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
					<div
						class="mb-4 hidden shrink-0 items-center gap-2 border-b border-border/30 pb-4 md:flex"
					>
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
					<div
						class="hidden shrink-0 items-center gap-2 border-b border-border/30 px-4 pt-4 pb-4 md:flex md:px-6 md:pt-6"
					>
						<currentSection.icon class="h-5 w-5" />
						<h3 class="text-lg font-semibold">{currentSection.title}</h3>
					</div>
					<ScrollArea class="min-h-0 flex-1">
						<div class="space-y-6 p-4 md:p-6">
							<!-- DHATS: Block status banner at the TOP of settings content -->
							{#if blockStatus?.blocked}
								<div class="mb-6 rounded-lg border border-red-500/30 bg-red-500/10 p-4">
									<div class="flex items-start gap-3">
										<AlertTriangle class="h-5 w-5 text-red-500 shrink-0 mt-0.5" />
										<div class="flex-1 min-w-0">
											<h3 class="font-semibold text-red-700 dark:text-red-400 text-sm">
												Model blocked: {blockStatus.model}
											</h3>
											<p class="text-sm text-muted-foreground mt-1">
												{blockStatus.reason}
											</p>
											<p class="text-xs text-muted-foreground mt-1 italic">
												{blockStatus.recommendation}
											</p>
											{#if blockStatus.suggested_context > 0}
												<button
													class="mt-3 inline-flex items-center gap-1.5 rounded-md bg-red-500/20 border border-red-500/40 px-3 py-1.5 text-sm font-medium text-red-700 dark:text-red-300 hover:bg-red-500/30 transition-colors"
													onclick={() => applySuggestedCtx(blockStatus!)}
												>
													<Settings class="h-3.5 w-3.5" />
													Use {blockStatus.suggested_context} ctx instead
												</button>
											{/if}
										</div>
									</div>
								</div>
							{/if}

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
							{:else if currentSection.title === 'Agent tools'}
								<div class="max-w-3xl space-y-6">
									<ChatSettingsFields
										fields={currentSection.fields.filter((field) => field.key === 'useAgentTools')}
										{localConfig}
										onConfigChange={handleConfigChange}
									/>
									<fieldset
										disabled={!localConfig.useAgentTools}
										class="ml-2 space-y-5 border-l border-border pl-5 disabled:opacity-50 md:ml-5"
									>
										<legend class="mb-4 text-sm font-medium">Core tools</legend>
										<ChatSettingsFields
											fields={currentSection.fields.filter((field) =>
												CORE_AGENT_TOOL_KEYS.includes(field.key)
											)}
											{localConfig}
											onConfigChange={handleConfigChange}
										/>
									</fieldset>
									<fieldset
										disabled={!localConfig.useAgentTools}
										class="space-y-5 border-t border-border pt-5 disabled:opacity-50"
									>
										<legend class="text-sm font-medium">Computer access</legend>
										<p class="text-xs text-muted-foreground">
											Choose file and shell access separately. Enabling core tools keeps these
											choices.
										</p>
										<ChatSettingsFields
											fields={currentSection.fields.filter(
												(field) => field.key === 'useFileTools' || field.key === 'useShellTools'
											)}
											{localConfig}
											onConfigChange={handleConfigChange}
										/>
									</fieldset>
									<section
										aria-label="Approval preferences"
										class="space-y-4 border-t border-border pt-5"
									>
										<h3 class="text-sm font-medium">Approval preferences</h3>
										<ChatSettingsFields
											fields={currentSection.fields.filter((field) => field.type === 'action')}
											{localConfig}
											onConfigChange={handleConfigChange}
										/>
									</section>
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
								<p class="text-xs text-muted-foreground">Settings are saved on this device.</p>
							</div>
						</div>
					</ScrollArea>
				</div>
			{/if}
		</div>
	</div>

	<!-- Footer -->
	<footer
		class="flex shrink-0 items-center justify-between gap-3 border-t border-border/30 bg-background px-4 py-3 md:px-6"
	>
		<Button variant="outline" onclick={handleReset}>
			<RotateCcw class="mr-2 h-4 w-4" />
			Restore saved settings
		</Button>
		<Button variant="default" onclick={handleSave}>Save settings</Button>
	</footer>
</main>

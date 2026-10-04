<script lang="ts">
	import {
		Settings,
		Info,
		Funnel,
		AlertTriangle,
		Code,
		Monitor,
		Sun,
		Moon,
		Layout,
		Database,
		Waypoints,
		User,
		Wrench,
		ArrowLeft,
		RotateCcw
	} from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import { agentService } from '$lib/services/agent';
	import ChatSettingsFields from './ChatSettingsFields.svelte';
	import { ParameterSyncService } from '$lib/services/parameter-sync';
	import { APP_VERSION } from '$lib/constants/app-version';
	import {
		SETTINGS_VIEWS,
		GROUP_LABELS,
		FIELD_LABELS,
		FIELD_HELP,
		resolveSettingsLocation
	} from './settings-navigation';
	import {
		NUMERIC_SETTING_RULES,
		parseCustomParameters,
		validateGenerationSettings
	} from '$lib/services/generation-settings';
	import { CORE_AGENT_TOOL_KEYS } from '$lib/constants/settings-config';
	import ImportExportTab from './ImportExportTab.svelte';
	import ModelManagementTab from '../ModelManagement/ModelManagementTab.svelte';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import {
		config,
		updateConfig,
		updateMultipleConfig,
		settingsStore,
		forceSyncWithServerDefaults
	} from '$lib/stores/settings.svelte';
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
						{ value: 'concise', label: 'Short' },
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
			title: 'Display',
			icon: Layout,
			fields: [
				{
					key: 'theme',
					label: 'Theme',
					type: 'select',
					options: [
						{ value: 'system', label: 'System', icon: Monitor },
						{ value: 'light', label: 'Light', icon: Sun },
						{ value: 'dark', label: 'Dark', icon: Moon }
					]
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
					label: 'Saved permission choices',
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

	settingSections.push({
		title: 'Connection',
		icon: Code,
		fields: [{ key: 'apiKey', label: 'API access key', type: 'input' }]
	});
	const developerFields = settingSections.find((group) => group.title === 'Developer')!.fields;
	developerFields.push({
		key: 'showMessageStats',
		label: 'Show response statistics',
		type: 'checkbox'
	});
	developerFields.push({
		key: 'pasteLongTextToFileLen',
		label: 'Long text attachment threshold',
		type: 'input'
	});

	const personalFields = settingSections.find((group) => group.title === 'You')!.fields;
	personalFields.push(...settingSections.find((group) => group.title === 'Display')!.fields);

	for (const group of settingSections) {
		for (const field of group.fields) {
			field.label = FIELD_LABELS[field.key] ?? field.label;
			field.help = FIELD_HELP[field.key] ?? field.help;
			field.numeric = NUMERIC_SETTING_RULES[field.key];
			if (
				['Sampling', 'Penalties', 'Connection', 'Developer'].includes(group.title) &&
				(field.numeric || field.key === 'samplers' || field.key === 'custom')
			)
				field.technicalName = field.key;
		}
	}
	const views = SETTINGS_VIEWS.map((view) => ({
		...view,
		icon: view.title === 'Info' ? Info : view.title === 'General' ? Settings : Code
	}));
	let activeView = $state('General');
	let activeSection = $state('You');
	const visibleGroups = $derived(
		((views.find((view) => view.title === activeView)?.groups as readonly string[]) ?? []).map(
			(title) => settingSections.find((group) => group.title === title)!
		)
	);
	function selectView(title: string) {
		activeView = title;
		activeSection = title === 'Developer' ? 'Sampling' : 'You';
	}
	function resetGenerationDefaults() {
		if (forceSyncWithServerDefaults()) {
			for (const key of ParameterSyncService.getSyncableParameterKeys())
				localConfig[key] = config()[key];
			toast.success('Response controls reset to model defaults.');
		} else toast.error('Could not save the reset. Please try again.');
	}
	const appVersion = APP_VERSION;

	let currentSection = $derived(
		settingSections.find((section) => section.title === activeSection) || settingSections[0]
	);
	let localConfig: SettingsConfigType = $state({ ...config() });
	const hasChanges = $derived(
		Object.entries(localConfig).some(([key, value]) => {
			const saved = config()[key];
			return NUMERIC_SETTING_RULES[key] && value !== ''
				? Number(value) !== Number(saved)
				: value !== saved;
		})
	);
	const validationErrors = $derived.by(() => {
		const errors = validateGenerationSettings(localConfig);
		try {
			parseCustomParameters(localConfig.custom);
		} catch (error) {
			errors.custom = error instanceof Error ? error.message : 'Check extra request settings.';
		}
		return errors;
	});

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
		const errors = validateGenerationSettings(processedConfig);
		if (Object.keys(errors).length) {
			toast.error(Object.values(errors)[0]);
			return;
		}
		try {
			parseCustomParameters(processedConfig.custom);
		} catch (error) {
			toast.error(String(error));
			return;
		}
		const numericFields = [
			'temperature',
			'top_k',
			'top_p',
			'min_p',
			'max_tokens',
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

		const changes = Object.fromEntries(
			Object.entries(processedConfig).filter(([key, value]) => value !== config()[key])
		);
		if (updateMultipleConfig(changes)) {
			localConfig = { ...config() };
			toast.success('Settings saved.');
		} else {
			toast.error('Could not save settings. Your changes are still available to retry.');
		}
	}

	// Route links can open directly to a category; ignore unknown section names.
	$effect(() => {
		const location = resolveSettingsLocation(section);
		activeView = location.view;
		activeSection = location.group;
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
			<nav
				aria-label="Settings views"
				class="flex shrink-0 gap-1 border-b border-border/30 p-3 md:w-56 md:flex-col md:border-r md:border-b-0 md:p-4"
			>
				{#each views as view}
					<button
						class="flex min-w-0 flex-1 items-center justify-center gap-2 rounded-lg px-3 py-3 text-sm font-medium transition-colors hover:bg-accent md:flex-none md:justify-start {activeView ===
						view.title
							? 'bg-accent text-accent-foreground'
							: 'text-muted-foreground'}"
						aria-current={activeView === view.title ? 'page' : undefined}
						onclick={() => selectView(view.title)}
					>
						<view.icon class="h-4 w-4 shrink-0" /><span>{view.title}</span>
					</button>
				{/each}
			</nav>
			<div class="flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden md:flex-row">
				{#if activeView !== 'Info'}
					<div
						class="shrink-0 border-b border-border/30 px-4 py-3 md:w-56 md:overflow-y-auto md:border-r md:border-b-0 md:p-4"
					>
						<p class="mb-3 text-sm text-muted-foreground">
							{activeView === 'General' ? 'Your preferences' : 'Advanced controls'}
						</p>
						<nav aria-label="Settings groups" class="hidden flex-col gap-1 md:flex">
							{#each visibleGroups as group}
								<button
									class="flex items-center gap-2 rounded-md px-3 py-2 text-left text-sm hover:bg-accent {activeSection ===
									group.title
										? 'bg-accent font-medium text-accent-foreground'
										: 'text-muted-foreground'}"
									aria-current={activeSection === group.title ? 'page' : undefined}
									onclick={() => (activeSection = group.title)}
								>
									<group.icon class="h-4 w-4 shrink-0" /><span class="min-w-0"
										>{GROUP_LABELS[group.title]}</span
									>{@render sectionBadge(group.title, '')}
								</button>
							{/each}
						</nav>
						<select
							aria-label="Settings group"
							bind:value={activeSection}
							class="w-full rounded-md border border-input bg-background px-3 py-2 text-sm md:hidden"
						>
							{#each visibleGroups as group}<option value={group.title}
									>{GROUP_LABELS[group.title]}</option
								>{/each}
						</select>
					</div>
				{/if}
				<div class="flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden">
					{#if settingsStore.repairedSettings.length}
						<div role="status" class="shrink-0 border-b bg-muted/50 px-4 py-3 text-sm md:px-6">
							Fixed saved values that this model cannot use: {settingsStore.repairedSettings
								.map((key) => FIELD_LABELS[key] ?? key)
								.join(', ')}. Valid preferences were kept.
							<button class="ml-2 underline" onclick={() => (settingsStore.repairedSettings = [])}
								>Dismiss</button
							>
						</div>
					{/if}
					{#if activeView === 'Info'}
						<div class="min-h-0 flex-1 overflow-y-auto p-4 md:p-6">
							<section class="max-w-2xl space-y-6 rounded-xl border border-border/50 p-5 md:p-8">
								<div>
									<h2 class="text-2xl font-semibold">About Delta</h2>
									<p class="mt-2 text-sm text-muted-foreground">
										Your assistant for conversations, notes, and everyday work.
									</p>
								</div>
								<dl class="text-sm">
									<dt class="text-muted-foreground">App version</dt>
									<dd class="mt-1 font-medium">{appVersion}</dd>
								</dl>
								<div class="flex flex-wrap gap-3">
									<a
										class="rounded-md border px-4 py-2 text-sm hover:bg-accent"
										href="https://github.com/nile-agi/delta"
										target="_blank"
										rel="noreferrer">Project & documentation</a
									><a
										class="rounded-md border px-4 py-2 text-sm hover:bg-accent"
										href="https://github.com/nile-agi/delta/issues/new/choose"
										target="_blank"
										rel="noreferrer">Get help or report a problem</a
									>
								</div>
							</section>
						</div>
					{:else if currentSection.title === 'Model Management'}
						<div class="flex min-h-0 flex-1 flex-col overflow-hidden p-4 md:p-6">
							<div class="mb-4 shrink-0 items-center gap-2 border-b border-border/30 pb-4 md:flex">
								<currentSection.icon class="h-5 w-5" />
								<h2 class="text-lg font-semibold">{GROUP_LABELS[currentSection.title]}</h2>
							</div>
							<p class="mb-4 shrink-0 text-sm text-muted-foreground">
								Manage your installed models and download new ones. Use the model selector in the
								chat input to choose models in the chat interface.
							</p>
							<ModelManagementTab />
						</div>
					{:else}
						<div class="flex min-h-0 flex-1 flex-col overflow-hidden">
							<div
								class="flex shrink-0 items-center gap-2 border-b border-border/30 px-4 pt-4 pb-4 md:flex md:px-6 md:pt-6"
							>
								<currentSection.icon class="h-5 w-5" />
								<h2 class="text-lg font-semibold">{GROUP_LABELS[currentSection.title]}</h2>
							</div>
							<ScrollArea class="min-h-0 flex-1">
								<div class="max-w-3xl space-y-6 p-4 md:p-6">
									<!-- DHATS: Block status banner at the TOP of settings content -->
									{#if blockStatus?.blocked}
										<div class="mb-6 rounded-lg border border-red-500/30 bg-red-500/10 p-4">
											<div class="flex items-start gap-3">
												<AlertTriangle class="h-5 w-5 text-red-500 shrink-0 mt-0.5" />
												<div class="flex-1 min-w-0">
													<h2 class="font-semibold text-red-700 dark:text-red-400 text-sm">
														Model blocked: {blockStatus.model}
													</h2>
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
															Use a {blockStatus.suggested_context}-token context
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
												fields={currentSection.fields.filter(
													(field) => field.key === 'useAgentTools'
												)}
												{localConfig}
												onConfigChange={handleConfigChange}
											/>
											<fieldset
												disabled={!localConfig.useAgentTools}
												class="ml-2 space-y-5 border-l border-border pl-5 disabled:opacity-50 md:ml-5"
											>
												<legend class="mb-4 text-sm font-medium">Everyday assistance</legend>
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
													Choose file access and command execution separately. Turning on everyday
													tools keeps these choices.
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
												<h2 class="text-sm font-medium">Approval preferences</h2>
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
								</div>
							</ScrollArea>
						</div>
					{/if}
				</div>
			</div>
		</div>
	</div>

	<!-- Footer -->
	{#if activeView !== 'Info'}
		<footer
			class="flex shrink-0 flex-wrap items-center justify-end gap-3 border-t border-border/30 bg-background px-4 py-3 md:px-6"
		>
			<div class="flex flex-wrap gap-2">
				{#if hasChanges}<Button variant="outline" onclick={handleReset}>
						<RotateCcw class="mr-2 h-4 w-4" />
						Discard changes
					</Button>{/if}
				{#if activeView === 'Developer'}<Button variant="ghost" onclick={resetGenerationDefaults}
						>Reset to model defaults</Button
					>{/if}
			</div>
			<Button
				variant="default"
				disabled={!hasChanges || Object.keys(validationErrors).length > 0}
				onclick={handleSave}>Save settings</Button
			>
		</footer>
	{/if}
</main>

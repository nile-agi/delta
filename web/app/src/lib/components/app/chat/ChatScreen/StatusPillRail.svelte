<script lang="ts">
	import { Settings, X, Loader2 } from '@lucide/svelte';
	import { fade } from 'svelte/transition';
	import { settingsWindow } from '$lib/stores/settings-window.svelte';
	import { downloads } from '$lib/stores/downloads.svelte';
	import { findModelByName } from '$lib/data/models_catalog';

	const MODEL_MANAGEMENT_SECTION = 'Model Management';

	const showSettingsPill = $derived(
		settingsWindow.state.open && settingsWindow.state.minimized
	);
	const activeDownloads = $derived(downloads.active);
	const hasAnything = $derived(showSettingsPill || activeDownloads.length > 0);

	function label(modelName: string): string {
		return findModelByName(modelName)?.display_name || modelName;
	}
</script>

<!--
	Rail of background-task pills that sits above the chat input. Extracted from ChatScreen so
	the conversation and welcome branches share one copy instead of duplicating the markup.
-->
{#if hasAnything}
	<div
		class="pointer-events-auto mx-auto mb-2 flex w-fit flex-wrap items-center justify-center gap-2"
	>
		{#if showSettingsPill}
			<div
				class="flex items-center gap-1 rounded-full border border-border/30 bg-background/80 px-2 py-1 shadow-sm backdrop-blur-md"
				transition:fade={{ duration: 150 }}
			>
				<button
					type="button"
					class="flex h-6 items-center gap-1.5 rounded-full px-2 text-xs font-medium text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
					onclick={() => settingsWindow.restore()}
					aria-label="Restore Settings"
					title="Restore Settings"
				>
					<Settings class="h-3.5 w-3.5" />
					<span>Settings</span>
				</button>
				<div class="h-3 w-px bg-border/50" aria-hidden="true"></div>
				<button
					type="button"
					class="flex h-5 w-5 items-center justify-center rounded-full text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
					onclick={() => settingsWindow.close()}
					aria-label="Close Settings"
					title="Close Settings"
				>
					<X class="h-3 w-3" />
				</button>
			</div>
		{/if}

		{#each activeDownloads as download (download.model)}
			<div
				class="flex items-center gap-1 rounded-full border border-border/30 bg-background/80 px-2 py-1 shadow-sm backdrop-blur-md"
				transition:fade={{ duration: 150 }}
			>
				<button
					type="button"
					class="flex h-6 items-center gap-1.5 rounded-full px-2 text-xs font-medium text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
					onclick={() => settingsWindow.openTo(MODEL_MANAGEMENT_SECTION)}
					aria-label="Downloading {label(download.model)}, {download.progress.toFixed(
						0
					)} percent. Open model management"
					title="Downloading {label(download.model)} — open Model Management"
				>
					<Loader2 class="h-3.5 w-3.5 animate-spin" />
					<span class="tabular-nums">{download.progress.toFixed(0)}%</span>
					<span class="max-w-[7rem] truncate">{label(download.model)}</span>
				</button>
				<div class="h-3 w-px bg-border/50" aria-hidden="true"></div>
				<button
					type="button"
					class="flex h-5 w-5 items-center justify-center rounded-full text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
					onclick={() => downloads.cancel(download.model)}
					aria-label="Cancel download of {label(download.model)}"
					title="Cancel download"
				>
					<X class="h-3 w-3" />
				</button>
			</div>
		{/each}
	</div>
{/if}

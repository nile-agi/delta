<script lang="ts">
	import { base } from '$app/paths';
	import { X } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { findModelByName, getFamilyIconForModelName, isLogoPath } from '$lib/data/models_catalog';
	import type { DownloadState } from '$lib/stores/downloads.svelte';
	import DownloadProgressBar from './DownloadProgressBar.svelte';

	interface Props {
		download: DownloadState;
		onCancel?: (modelName: string) => void;
	}

	let { download, onCancel }: Props = $props();

	// A downloading model isn't installed yet, so there is no ModelInfo for it — fall back to
	// the catalog for a friendly name and icon, and to the raw name if it isn't catalogued.
	const catalogModel = $derived(findModelByName(download.model));
	const displayName = $derived(catalogModel?.display_name || download.model);
	const icon = $derived(getFamilyIconForModelName(download.model));
</script>

<div
	class="flex flex-col overflow-hidden rounded-lg border border-primary/40 bg-muted transition-all duration-200"
>
	<div class="flex min-h-0 items-center gap-4 px-4 py-3">
		{#if icon}
			{#if isLogoPath(icon)}
				<img src="{base}{icon}" alt="" class="h-8 w-8 shrink-0 object-contain" />
			{:else}
				<span class="shrink-0 text-2xl" aria-hidden="true">{icon}</span>
			{/if}
		{/if}

		<div class="min-w-0 flex-1">
			<div class="mb-1 flex items-center gap-2">
				<h4 class="truncate text-sm font-semibold text-foreground">{displayName}</h4>
				<span class="shrink-0 rounded bg-primary/20 px-2 py-0.5 text-xs text-primary">
					Downloading
				</span>
			</div>
			<DownloadProgressBar
				progress={download.progress}
				currentBytes={download.currentBytes}
				totalBytes={download.totalBytes}
			/>
		</div>

		{#if onCancel}
			<Button
				variant="ghost"
				size="icon"
				class="h-8 w-8 shrink-0 rounded-full border border-border bg-muted text-muted-foreground hover:bg-accent hover:text-destructive"
				onclick={() => onCancel(download.model)}
				aria-label="Cancel download of {displayName}"
				title="Cancel download"
			>
				<X class="h-4 w-4" />
			</Button>
		{/if}
	</div>
</div>

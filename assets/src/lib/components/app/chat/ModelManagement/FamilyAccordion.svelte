<script lang="ts">
	import { ChevronDown, ChevronRight } from '@lucide/svelte';
	import type { ModelFamily, ModelCatalogModel } from '$lib/data/models_catalog';
	import ModelCard from './ModelCard.svelte';

	interface Props {
		family: ModelFamily;
		expanded?: boolean;
		systemRAMGB: number;
		installedModelNames: Set<string>;
		onModelDownload?: (modelName: string) => void;
		onModelRemove?: (modelName: string) => void;
		downloadingModel?: string | null;
		removingModel?: string | null;
		downloadProgress?: {
			progress: number;
			current_bytes: number;
			total_bytes: number;
			completed: boolean;
			failed: boolean;
			error_message?: string;
		} | null;
	}

	let {
		family,
		expanded = false,
		systemRAMGB,
		installedModelNames,
		onModelDownload,
		onModelRemove,
		downloadingModel,
		removingModel,
		downloadProgress
	}: Props = $props();

	let isExpanded = $state(expanded);

	function toggleExpanded() {
		isExpanded = !isExpanded;
	}
</script>

<div class="family-accordion rounded-lg border border-border/30 overflow-hidden">
	<button
		class="w-full flex items-center justify-between p-4 hover:bg-accent/50 transition-colors cursor-pointer"
		onclick={toggleExpanded}
		type="button"
	>
		<div class="flex items-center gap-3 flex-1">
			<div class="text-2xl">{family.icon}</div>
			<div class="flex-1 text-left">
				<h3 class="font-semibold text-base">{family.name}</h3>
				<p class="text-sm text-muted-foreground mt-1">{family.description}</p>
			</div>
		</div>
		<div class="ml-4">
			{#if isExpanded}
				<ChevronDown class="h-5 w-5 text-muted-foreground" />
			{:else}
				<ChevronRight class="h-5 w-5 text-muted-foreground" />
			{/if}
		</div>
	</button>

	{#if isExpanded}
		<div class="border-t border-border/30 p-4 space-y-3">
			{#each family.models as model (model.name)}
				<ModelCard
					{model}
					{systemRAMGB}
					isInstalled={installedModelNames.has(model.name)}
					onDownload={onModelDownload}
					onRemove={onModelRemove}
					downloading={downloadingModel === model.name}
					removing={removingModel === model.name}
					downloadProgress={downloadingModel === model.name ? downloadProgress : null}
				/>
			{/each}
		</div>
	{/if}
</div>

<style>
	.family-accordion {
		background: rgba(0, 31, 63, 0.3);
		backdrop-filter: blur(10px);
	}
</style>

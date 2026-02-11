<script lang="ts">
	import { base } from '$app/paths';
	import { ChevronDown, ChevronRight } from '@lucide/svelte';
	import type { ModelFamily } from '$lib/data/models_catalog';
	import { isLogoPath } from '$lib/data/models_catalog';
	import ModelCard from './ModelCard.svelte';
	import { fly } from 'svelte/transition';

	interface Props {
		family: ModelFamily;
		expanded?: boolean;
		systemRAMGB: number;
		installedModelNames: Set<string>;
		onModelDownload?: (modelName: string) => void;
		onModelRemove?: (modelName: string) => void;
		onModelStopDownload?: (modelName: string) => void;
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
		onModelStopDownload,
		downloadingModel,
		removingModel,
		downloadProgress
	}: Props = $props();

	let isExpanded = $state(expanded);

	function toggleExpanded() {
		isExpanded = !isExpanded;
	}
</script>

<!-- 
	Family accordion (Option A: app design tokens)
	- Icon + name + description header
	- Smooth expand/collapse animation
	- Models shown as sub-items when expanded
-->
<div
	class="family-accordion overflow-hidden rounded-lg border border-border bg-muted transition-all duration-200 hover:border-border"
>
	<!-- Family Header - Clickable to expand/collapse -->
	<button
		class="flex w-full cursor-pointer items-center justify-between px-4 py-4 text-left transition-colors hover:bg-accent/50"
		onclick={toggleExpanded}
		type="button"
	>
		<div class="flex min-w-0 flex-1 items-center gap-4">
			<!-- Family Icon -->
			<div
				class="flex h-10 w-10 flex-shrink-0 items-center justify-center overflow-hidden rounded-full bg-background text-xl"
			>
				{#if isLogoPath(family.icon)}
					<img
						src={`${base}/${encodeURIComponent(family.icon)}`}
						alt=""
						class="h-7 w-7 object-contain"
					/>
				{:else}
					{family.icon}
				{/if}
			</div>

			<!-- Family Name and Description -->
			<div class="min-w-0 flex-1">
				<h3 class="mb-1 text-base font-semibold text-foreground">{family.name}</h3>
				<p class="line-clamp-1 text-sm text-muted-foreground">{family.description}</p>
			</div>
		</div>

		<!-- Chevron Icon -->
		<div class="ml-4 flex-shrink-0">
			{#if isExpanded}
				<ChevronDown class="h-5 w-5 text-muted-foreground" />
			{:else}
				<ChevronRight class="h-5 w-5 text-muted-foreground" />
			{/if}
		</div>
	</button>

	<!-- Expanded Content - Models List -->
	{#if isExpanded}
		<div
			class="border-t border-border bg-background"
			transition:fly={{ y: -10, duration: 200 }}
		>
			<div class="space-y-3 p-4">
				{#each family.models as model (model.name)}
					<ModelCard
						{model}
						familyIcon={family.icon}
						{systemRAMGB}
						isInstalled={installedModelNames.has(model.name)}
						onDownload={onModelDownload}
						onRemove={onModelRemove}
						onStopDownload={onModelStopDownload}
						downloading={downloadingModel === model.name}
						removing={removingModel === model.name}
						downloadProgress={downloadingModel === model.name ? downloadProgress : null}
					/>
				{/each}
			</div>
		</div>
	{/if}
</div>

<!-- Option A: uses app design tokens -->

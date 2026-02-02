<script lang="ts">
	import { ChevronDown, ChevronRight } from '@lucide/svelte';
	import type { ModelFamily, ModelCatalogModel } from '$lib/data/models_catalog';
	import ModelCard from './ModelCard.svelte';
	import { fly } from 'svelte/transition';

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

<!-- 
	LlamaBarn-style family accordion
	- Dark blue card background (#11243a)
	- Icon + name + description header
	- Smooth expand/collapse animation
	- Models shown as sub-items when expanded
-->
<div
	class="family-accordion rounded-lg border border-[#1a2b44]/50 overflow-hidden transition-all duration-200 hover:border-[#1a2b44]"
>
	<!-- Family Header - Clickable to expand/collapse -->
	<button
		class="w-full flex items-center justify-between px-4 py-4 hover:bg-[#1a2b44]/30 transition-colors cursor-pointer text-left"
		onclick={toggleExpanded}
		type="button"
	>
		<div class="flex items-center gap-4 flex-1 min-w-0">
			<!-- Family Icon -->
			<div class="flex-shrink-0 w-10 h-10 rounded-full bg-[#1a2b44] flex items-center justify-center text-xl">
				{family.icon}
			</div>
			
			<!-- Family Name and Description -->
			<div class="flex-1 min-w-0">
				<h3 class="font-semibold text-base text-[#e0e0ff] mb-1">{family.name}</h3>
				<p class="text-sm text-[#d0d8ff]/70 line-clamp-1">{family.description}</p>
			</div>
		</div>
		
		<!-- Chevron Icon -->
		<div class="ml-4 flex-shrink-0">
			{#if isExpanded}
				<ChevronDown class="h-5 w-5 text-[#d0d8ff]/50" />
			{:else}
				<ChevronRight class="h-5 w-5 text-[#d0d8ff]/50" />
			{/if}
		</div>
	</button>

	<!-- Expanded Content - Models List -->
	{#if isExpanded}
		<div
			class="border-t border-[#1a2b44]/50 bg-[#0a1421]"
			transition:fly={{ y: -10, duration: 200 }}
		>
			<div class="p-4 space-y-3">
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
		</div>
	{/if}
</div>

<style>
	/* 
		Family accordion styling
		- Card background: #11243a (slightly lighter than main background)
		- Hover: subtle lighten
		- Border: subtle dark blue
	*/
	.family-accordion {
		background: #11243a;
	}

	.family-accordion:hover {
		box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
	}
</style>

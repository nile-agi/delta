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
	class="family-accordion overflow-hidden rounded-lg border border-[#1a2b44]/50 transition-all duration-200 hover:border-[#1a2b44]"
>
	<!-- Family Header - Clickable to expand/collapse -->
	<button
		class="flex w-full cursor-pointer items-center justify-between px-4 py-4 text-left transition-colors hover:bg-[#1a2b44]/30"
		onclick={toggleExpanded}
		type="button"
	>
		<div class="flex min-w-0 flex-1 items-center gap-4">
			<!-- Family Icon -->
			<div
				class="flex h-10 w-10 flex-shrink-0 items-center justify-center overflow-hidden rounded-full bg-[#1a2b44] text-xl"
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
				<h3 class="mb-1 text-base font-semibold text-[#e0e0ff]">{family.name}</h3>
				<p class="line-clamp-1 text-sm text-[#d0d8ff]/70">{family.description}</p>
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
			<div class="space-y-3 p-4">
				{#each family.models as model (model.name)}
					<ModelCard
						{model}
						familyIcon={family.icon}
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

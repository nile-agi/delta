<script lang="ts">
	import { Download, Loader2, AlertCircle } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import type { ModelCatalogModel } from '$lib/data/models_catalog';
	import { getQuantizationSuggestions } from '$lib/data/models_catalog';

	interface Props {
		model: ModelCatalogModel;
		systemRAMGB: number;
		isInstalled: boolean;
		onDownload?: (modelName: string) => void;
		onRemove?: (modelName: string) => void;
		downloading?: boolean;
		removing?: boolean;
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
		model,
		systemRAMGB,
		isInstalled,
		onDownload,
		onRemove,
		downloading = false,
		removing = false,
		downloadProgress
	}: Props = $props();

	const isCompatible = $derived(model.required_ram_gb <= systemRAMGB);
	const ramMismatch = $derived(systemRAMGB < model.required_ram_gb);
	const quantizationSuggestions = $derived(
		ramMismatch ? getQuantizationSuggestions(model, systemRAMGB) : []
	);

	function formatContextSize(contextSize: number): string {
		if (contextSize >= 1000000) {
			return `${(contextSize / 1000000).toFixed(1)}M ctx`;
		} else if (contextSize >= 1000) {
			return `${(contextSize / 1000).toFixed(0)}k ctx`;
		}
		return `${contextSize} ctx`;
	}

	function formatFileSize(sizeGB: number): string {
		if (sizeGB < 1) {
			return `${(sizeGB * 1024).toFixed(0)} MB`;
		}
		return `${sizeGB.toFixed(1)} GB`;
	}
</script>

<!-- 
	LlamaBarn-style model card for catalog items
	- Horizontal layout: icon + info + action button
	- Compatible: normal styling with Install button
	- Incompatible: grayed out with RAM warning
	- Hover: subtle lighten + border glow
-->
<div
	class="model-card group flex items-center gap-4 px-4 py-3 rounded-lg border transition-all duration-200 {isCompatible
		? 'bg-[#11243a] border-[#1a2b44]/50 hover:bg-[#1a2b44] hover:border-[#4cc9f0]/20'
		: 'bg-[#0a1421] border-[#1a2b44]/30 opacity-60'}"
>
	<!-- Model Icon (if available) -->
	{#if model.icon}
		<div class="flex-shrink-0 w-8 h-8 rounded-full bg-[#1a2b44] flex items-center justify-center text-lg">
			{model.icon}
		</div>
	{/if}

	<!-- Model Info -->
	<div class="flex-1 min-w-0">
		<div class="flex items-center gap-2 mb-1">
			<h4 class="font-semibold text-sm {isCompatible ? 'text-[#e0e0ff]' : 'text-[#d0d8ff]/50'}">
				{model.display_name}
			</h4>
			{#if isInstalled}
				<span class="text-xs px-2 py-0.5 rounded bg-[#4cc9f0]/20 text-[#4cc9f0]">Installed</span>
			{/if}
		</div>

		{#if isCompatible}
			<div class="flex items-center gap-3 text-xs text-[#d0d8ff]/70">
				<span>{formatFileSize(model.file_size_gb)}</span>
				<span>•</span>
				<span>{formatContextSize(model.context_size)}</span>
				{#if model.quantization}
					<span>•</span>
					<span>{model.quantization}</span>
				{/if}
			</div>
		{:else}
			<Tooltip.Provider>
				<Tooltip.Root>
					<Tooltip.Trigger>
						<div class="text-xs text-red-400/80 cursor-help inline-flex items-center gap-1">
							<AlertCircle class="h-3 w-3" />
							<span>Requires Mac with {model.required_ram_gb} GB+ of memory</span>
						</div>
					</Tooltip.Trigger>
					<Tooltip.Content>
						<div class="space-y-1 text-sm">
							<p>This model requires {model.required_ram_gb} GB of RAM.</p>
							<p>Your system has {systemRAMGB} GB available.</p>
							{#if quantizationSuggestions.length > 0}
								<p class="mt-2 font-medium">Consider these smaller variants:</p>
								<ul class="list-disc list-inside text-xs">
									{#each quantizationSuggestions as suggestion}
										<li>{suggestion.display_name}</li>
									{/each}
								</ul>
							{/if}
						</div>
					</Tooltip.Content>
				</Tooltip.Root>
			</Tooltip.Provider>
		{/if}

		<!-- Download Progress -->
		{#if downloading && downloadProgress}
			<div class="mt-3 space-y-1.5">
				<div class="flex items-center justify-between text-xs text-[#d0d8ff]/70">
					<span class="font-medium">Downloading: {downloadProgress.progress.toFixed(1)}%</span>
					<span>
						{(downloadProgress.current_bytes / (1024 * 1024)).toFixed(1)} MB /{' '}
						{(downloadProgress.total_bytes / (1024 * 1024)).toFixed(1)} MB
					</span>
				</div>
				<div class="w-full bg-[#0a1421] rounded-full h-2 overflow-hidden">
					<div
						class="h-full bg-[#4cc9f0] transition-all duration-300 ease-out rounded-full"
						style="width: {Math.max(0, Math.min(100, downloadProgress.progress))}%"
					></div>
				</div>
				{#if downloadProgress.failed && downloadProgress.error_message}
					<p class="text-xs text-red-400">{downloadProgress.error_message}</p>
				{/if}
			</div>
		{/if}
	</div>

	<!-- Action Button -->
	<div class="flex items-center gap-2 flex-shrink-0">
		{#if isInstalled}
			<!-- Installed models don't show install button in catalog -->
		{:else if isCompatible}
			{#if onDownload}
				<Tooltip.Provider>
					<Tooltip.Root>
						<Tooltip.Trigger>
							<Button
								variant="default"
								size="sm"
								class="bg-[#4cc9f0] hover:bg-[#00b4d8] text-white border-0 shadow-lg shadow-[#4cc9f0]/20"
								onclick={() => onDownload(model.name)}
								disabled={downloading}
							>
								{#if downloading}
									<Loader2 class="h-4 w-4 mr-2 animate-spin" />
									Downloading...
								{:else}
									<Download class="h-4 w-4 mr-2" />
									Install
								{/if}
							</Button>
						</Tooltip.Trigger>
						<Tooltip.Content>
							<p>Download {model.display_name} ({formatFileSize(model.file_size_gb)})</p>
						</Tooltip.Content>
					</Tooltip.Root>
				</Tooltip.Provider>
			{/if}
		{:else}
			<Tooltip.Provider>
				<Tooltip.Root>
					<Tooltip.Trigger>
						<Button
							variant="outline"
							size="sm"
							class="border-[#1a2b44] text-[#d0d8ff]/40 cursor-not-allowed"
							disabled
						>
							<AlertCircle class="h-4 w-4 mr-2" />
							Incompatible
						</Button>
					</Tooltip.Trigger>
					<Tooltip.Content>
						<div class="space-y-1 text-sm">
							<p>Requires {model.required_ram_gb} GB RAM</p>
							<p>You have {systemRAMGB} GB available</p>
						</div>
					</Tooltip.Content>
				</Tooltip.Root>
			</Tooltip.Provider>
		{/if}
	</div>
</div>

<style>
	.model-card {
		cursor: default;
	}

	.model-card:hover {
		box-shadow: 0 0 0 1px rgba(76, 201, 240, 0.2);
	}
</style>

<script lang="ts">
	import { base } from '$app/paths';
	import { Download, Loader2, AlertCircle, X } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import type { ModelCatalogModel } from '$lib/data/models_catalog';
	import { getQuantizationSuggestions, isLogoPath } from '$lib/data/models_catalog';

	interface Props {
		model: ModelCatalogModel;
		systemRAMGB: number;
		isInstalled: boolean;
		familyIcon?: string;
		onDownload?: (modelName: string) => void;
		onRemove?: (modelName: string) => void;
		onStopDownload?: (modelName: string) => void;
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

	/* eslint-disable @typescript-eslint/no-unused-vars -- onRemove, removing: parent passes for API consistency; remove lives in InstalledModelRow */
	let {
		model,
		systemRAMGB,
		isInstalled,
		familyIcon,
		onDownload,
		onRemove,
		onStopDownload,
		downloading = false,
		removing,
		downloadProgress
	}: Props = $props();

	const displayIcon = $derived(model.icon ?? familyIcon ?? '');
	/* eslint-enable @typescript-eslint/no-unused-vars */

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
	Model card for catalog items (Option A: app design tokens)
	- Horizontal layout: icon + info + action button
	- Compatible: normal styling with Install button
	- Incompatible: grayed out with RAM warning
-->
<div
	class="model-card group flex items-center gap-4 rounded-lg border border-border px-4 py-3 transition-all duration-200 {isCompatible
		? 'bg-muted hover:bg-accent/50 hover:border-border'
		: 'bg-background opacity-60'}"
>
	<!-- Model Icon (family or model icon) -->
	{#if displayIcon}
		<div
			class="flex h-8 w-8 flex-shrink-0 items-center justify-center overflow-hidden rounded-full bg-muted text-lg"
		>
			{#if isLogoPath(displayIcon)}
				<img
					src={`${base}/${encodeURIComponent(displayIcon)}`}
					alt=""
					class="h-6 w-6 object-contain"
				/>
			{:else}
				{displayIcon}
			{/if}
		</div>
	{/if}

	<!-- Model Info -->
	<div class="min-w-0 flex-1">
		<div class="mb-1 flex items-center gap-2">
			<h4 class="text-sm font-semibold {isCompatible ? 'text-foreground' : 'text-muted-foreground'}">
				{model.display_name}
			</h4>
			{#if isInstalled}
				<span class="rounded bg-primary/20 px-2 py-0.5 text-xs text-primary">Installed</span>
			{/if}
		</div>

		{#if isCompatible}
			<div class="flex items-center gap-3 text-xs text-muted-foreground">
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
						<div class="inline-flex cursor-help items-center gap-1 text-xs text-destructive">
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
								<ul class="list-inside list-disc text-xs">
									{#each quantizationSuggestions as suggestion (suggestion.name)}
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
				<div class="flex items-center justify-between text-xs text-muted-foreground">
					<span class="font-medium">
						<!-- Downloading:  -->
						{downloadProgress.progress.toFixed(1)}%
					</span>
					<span>
						{(downloadProgress.current_bytes / (1024 * 1024)).toFixed(1)} MB / {(downloadProgress.total_bytes / (1024 * 1024)).toFixed(1)} MB
					</span>
				</div>
				<div class="h-2 w-full overflow-hidden rounded-full bg-muted">
					<div
						class="h-full rounded-full bg-primary transition-all duration-300 ease-out"
						style="width: {Math.max(0, Math.min(100, downloadProgress.progress))}%;"
					></div>
				</div>
				{#if downloadProgress.failed && downloadProgress.error_message}
					<p class="text-xs text-destructive">{downloadProgress.error_message}</p>
				{/if}
			</div>
		{/if}
	</div>

	<!-- Action Button -->
	<div class="flex flex-shrink-0 items-center gap-2">
		{#if isInstalled}
			<!-- Installed models don't show install button in catalog -->
		{:else if isCompatible}
			{#if onDownload}
				<Tooltip.Provider>
					<Tooltip.Root>
						<Tooltip.Trigger>
							{#if downloading && onStopDownload}
								<Button
									variant="ghost"
									size="icon"
									class="h-8 w-8 rounded-full border border-border bg-muted text-muted-foreground hover:bg-accent hover:text-destructive"
									onclick={() => onStopDownload(model.name)}
								>
									<X class="h-4 w-4" />
								</Button>
							{/if}
							<Button
								variant="default"
								size="sm"
								onclick={() => onDownload(model.name)}
								disabled={downloading}
							>
								{#if downloading}
									<Loader2 class="h-4 w-4 animate-spin" />
								{:else}
									<Download class="h-4 w-4" />
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
							class="cursor-not-allowed text-muted-foreground"
							disabled
						>
							<AlertCircle class="mr-2 h-4 w-4" />
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
</style>

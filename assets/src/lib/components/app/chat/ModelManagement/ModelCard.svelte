<script lang="ts">
	import { Download, Trash2, Loader2, AlertCircle, CheckCircle2 } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
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

<div
	class="model-card rounded-lg p-4 border transition-all {isCompatible
		? 'border-border/30 bg-card/50'
		: 'border-destructive/30 bg-card/30 opacity-60'}"
>
	<div class="flex items-start justify-between gap-4">
		<div class="flex-1 min-w-0">
			<div class="flex items-center gap-2 mb-2">
				{#if model.icon}
					<span class="text-xl">{model.icon}</span>
				{/if}
				<h4 class="font-semibold text-sm {isCompatible ? '' : 'text-muted-foreground'}">
					{model.display_name}
				</h4>
				{#if isInstalled}
					<Badge variant="secondary" class="text-xs">
						<CheckCircle2 class="h-3 w-3 mr-1" />
						Installed
					</Badge>
				{/if}
			</div>

			{#if isCompatible}
				<div class="text-xs text-muted-foreground space-y-1">
					<div>
						<span class="font-medium">Size:</span> {formatFileSize(model.file_size_gb)} •{' '}
						<span class="font-medium">Context:</span> {formatContextSize(model.context_size)}
					</div>
					{#if model.quantization}
						<div>
							<span class="font-medium">Quantization:</span> {model.quantization}
						</div>
					{/if}
				</div>
			{:else}
				<Tooltip.Provider>
					<Tooltip.Root>
						<Tooltip.Trigger>
							<div class="text-xs text-destructive/80 cursor-help inline-flex items-center">
								<AlertCircle class="h-3 w-3 inline mr-1" />
								Requires {model.required_ram_gb} GB+ RAM (you have {systemRAMGB} GB)
							</div>
						</Tooltip.Trigger>
						<Tooltip.Content>
							<div class="space-y-1">
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

			{#if downloading && downloadProgress}
				<div class="mt-3 space-y-1.5">
					<div class="flex items-center justify-between text-xs">
						<span class="text-muted-foreground font-medium">
							Downloading: {downloadProgress.progress.toFixed(1)}%
						</span>
						<span class="text-muted-foreground">
							{(downloadProgress.current_bytes / (1024 * 1024)).toFixed(1)} MB /{' '}
							{(downloadProgress.total_bytes / (1024 * 1024)).toFixed(1)} MB
						</span>
					</div>
					<div class="w-full bg-secondary rounded-full h-2 overflow-hidden">
						<div
							class="h-full bg-primary transition-all duration-300 ease-out rounded-full"
							style="width: {Math.max(0, Math.min(100, downloadProgress.progress))}%"
						></div>
					</div>
					{#if downloadProgress.failed && downloadProgress.error_message}
						<p class="text-xs text-destructive">{downloadProgress.error_message}</p>
					{/if}
				</div>
			{/if}
		</div>

		<div class="flex items-center gap-2 flex-shrink-0">
			{#if isInstalled}
				{#if onRemove}
					<Button
						variant="destructive"
						size="sm"
						onclick={() => onRemove(model.name)}
						disabled={removing}
					>
						{#if removing}
							<Loader2 class="h-4 w-4 animate-spin" />
						{:else}
							<Trash2 class="h-4 w-4" />
						{/if}
					</Button>
				{/if}
			{:else if isCompatible}
				{#if onDownload}
					<Tooltip.Provider>
						<Tooltip.Root>
							<Tooltip.Trigger>
								<Button
									variant="default"
									size="sm"
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
								Download {model.display_name} ({formatFileSize(model.file_size_gb)})
							</Tooltip.Content>
						</Tooltip.Root>
					</Tooltip.Provider>
				{/if}
			{:else}
				<Tooltip.Provider>
					<Tooltip.Root>
						<Tooltip.Trigger>
							<Button variant="outline" size="sm" disabled>
								<AlertCircle class="h-4 w-4 mr-2" />
								Incompatible
							</Button>
						</Tooltip.Trigger>
						<Tooltip.Content>
							<div class="space-y-1">
								<p>Requires {model.required_ram_gb} GB RAM</p>
								<p>You have {systemRAMGB} GB available</p>
							</div>
						</Tooltip.Content>
					</Tooltip.Root>
				</Tooltip.Provider>
			{/if}
		</div>
	</div>
</div>

<style>
	.model-card {
		transition: all 0.2s ease;
	}

	.model-card:hover {
		background: rgba(0, 31, 63, 0.4);
	}
</style>

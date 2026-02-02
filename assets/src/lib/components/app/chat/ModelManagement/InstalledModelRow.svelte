<script lang="ts">
	import { Trash2, Copy, Loader2, CheckCircle2 } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import type { ModelInfo } from '$lib/services/models';
	import { modelsCatalog, findModelByName } from '$lib/data/models_catalog';

	interface Props {
		model: ModelInfo;
		onRemove?: (modelName: string) => void;
		onSelect?: (modelName: string) => void;
		removing?: boolean;
		selected?: boolean;
	}

	let { model, onRemove, onSelect, removing = false, selected = false }: Props = $props();

	/**
	 * Get family icon for the model
	 * Fixed: Use $derived.by() instead of $derived(() => ...) to avoid returning a function
	 */
	const familyIcon = $derived.by(() => {
		// First try to find the model in catalog to get exact icon
		const catalogModel = findModelByName(model.name);
		if (catalogModel) {
			// Find which family this model belongs to
			for (const family of modelsCatalog) {
				if (family.models.some((m) => m.name === model.name)) {
					return family.icon;
				}
			}
		}
		
		// Fallback: determine icon based on model name patterns
		const nameLower = model.name.toLowerCase();
		if (nameLower.includes('gemma')) return '◇';
		if (nameLower.includes('qwen')) return '∇';
		if (nameLower.includes('ministral') || nameLower.includes('mistral')) return 'M';
		if (nameLower.includes('glm')) return 'Z';
		if (nameLower.includes('devstral')) return 'D';
		if (nameLower.includes('nemotron')) return 'N';
		if (nameLower.includes('gpt')) return 'G';
		
		// Default icon
		return '●';
	});

	/**
	 * Format context size from model info
	 * Try to extract from description or use a reasonable default
	 */
	function getContextSize(): string {
		// Try to find context from catalog model
		const catalogModel = findModelByName(model.name);
		if (catalogModel && catalogModel.context_size) {
			const ctx = catalogModel.context_size;
			if (ctx >= 1000000) {
				return `${(ctx / 1000000).toFixed(1)}M ctx`;
			} else if (ctx >= 1000) {
				return `${(ctx / 1000).toFixed(0)}k ctx`;
			}
			return `${ctx} ctx`;
		}
		// Default fallback
		return '32k ctx';
	}

	function copyModelPath(e: MouseEvent) {
		e.stopPropagation(); // Prevent row selection when copying
		navigator.clipboard.writeText(model.name).then(() => {
			// Could show toast here if needed
		});
	}

	function handleDelete(e: MouseEvent) {
		e.stopPropagation(); // Prevent row selection when deleting
		if (onRemove) {
			onRemove(model.name);
		}
	}
</script>

<!-- 
	LlamaBarn-style horizontal model row for installed models
	- Deep blue card background (#11243a) with subtle border
	- Icon on left, model info in center, actions on right
	- Hover: subtle lighten + border glow
	- Selected: highlighted background with cyan border
-->
<div
	class="installed-model-row group flex items-center gap-4 px-4 py-3 rounded-lg transition-all duration-200 cursor-pointer {selected
		? 'bg-[#1a2b44] border border-[#4cc9f0]/40 shadow-lg shadow-[#4cc9f0]/10'
		: 'bg-[#11243a] border border-[#1a2b44]/50 hover:bg-[#1a2b44] hover:border-[#4cc9f0]/20'}"
	role="button"
	tabindex="0"
	onclick={() => onSelect?.(model.name)}
	onkeydown={(e) => {
		if (e.key === 'Enter' || e.key === ' ') {
			e.preventDefault();
			onSelect?.(model.name);
		}
	}}
>
	<!-- Family Icon - Left side -->
	<div class="flex-shrink-0 w-10 h-10 rounded-full bg-[#1a2b44] flex items-center justify-center text-xl text-[#e0e0ff]">
		{familyIcon}
	</div>

	<!-- Model Info - Center -->
	<div class="flex-1 min-w-0">
		<div class="flex items-center gap-2 mb-1">
			<h4 class="font-semibold text-sm text-[#e0e0ff] truncate">{model.display_name || model.name}</h4>
			{#if selected}
				<CheckCircle2 class="h-4 w-4 text-[#4cc9f0] flex-shrink-0" />
			{/if}
		</div>
		<div class="flex items-center gap-2 text-xs text-[#d0d8ff]/70 flex-wrap">
			{#if model.size_str}
				<span>{model.size_str}</span>
			{/if}
			{#if model.size_str && (model.quantization || getContextSize())}
				<span class="text-[#d0d8ff]/40">•</span>
			{/if}
			{#if model.quantization}
				<span>{model.quantization}</span>
			{/if}
			{#if model.quantization && getContextSize()}
				<span class="text-[#d0d8ff]/40">•</span>
			{/if}
			<span>{getContextSize()}</span>
		</div>
	</div>

	<!-- Actions - Right side -->
	<div class="flex items-center gap-2 flex-shrink-0">
		<!-- Copy Path Button -->
		<Tooltip.Provider>
			<Tooltip.Root>
				<Tooltip.Trigger>
					<Button
						variant="ghost"
						size="sm"
						class="h-8 w-8 p-0 text-[#d0d8ff]/60 hover:text-[#4cc9f0] hover:bg-[#1a2b44] transition-colors"
						onclick={copyModelPath}
					>
						<Copy class="h-4 w-4" />
					</Button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Copy model path</p>
				</Tooltip.Content>
			</Tooltip.Root>
		</Tooltip.Provider>

		<!-- Delete Button -->
		{#if onRemove}
			<Tooltip.Provider>
				<Tooltip.Root>
					<Tooltip.Trigger>
						<Button
							variant="ghost"
							size="sm"
							class="h-8 w-8 p-0 text-[#d0d8ff]/60 hover:text-red-400 hover:bg-red-500/10 transition-colors"
							onclick={handleDelete}
							disabled={removing}
						>
							{#if removing}
								<Loader2 class="h-4 w-4 animate-spin" />
							{:else}
								<Trash2 class="h-4 w-4" />
							{/if}
						</Button>
					</Tooltip.Trigger>
					<Tooltip.Content>
						<p>Delete model</p>
					</Tooltip.Content>
				</Tooltip.Root>
			</Tooltip.Provider>
		{/if}
	</div>
</div>

<style>
	.installed-model-row {
		cursor: pointer;
	}

	.installed-model-row:hover {
		box-shadow: 0 0 0 1px rgba(76, 201, 240, 0.2);
	}

	.installed-model-row:focus {
		outline: none;
		box-shadow: 0 0 0 2px rgba(76, 201, 240, 0.3);
	}
</style>

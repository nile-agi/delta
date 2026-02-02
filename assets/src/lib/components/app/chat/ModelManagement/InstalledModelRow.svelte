<script lang="ts">
	import { Trash2, Copy, Loader2 } from '@lucide/svelte';
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

	// Find the model in catalog to get icon
	const catalogModel = $derived(findModelByName(model.name));
	const familyIcon = $derived(() => {
		if (catalogModel) {
			// Find which family this model belongs to
			for (const family of modelsCatalog) {
				if (family.models.some((m) => m.name === model.name)) {
					return family.icon;
				}
			}
		}
		// Default icons based on model name patterns
		if (model.name.toLowerCase().includes('gemma')) return '◇';
		if (model.name.toLowerCase().includes('qwen')) return '∇';
		return '●';
	});

	function formatContextSize(sizeStr?: string): string {
		if (!sizeStr) return '';
		// Try to extract context from description or use default
		return '32k ctx'; // Default, could be enhanced
	}

	function copyModelPath() {
		// Copy model path/name to clipboard
		navigator.clipboard.writeText(model.name).then(() => {
			// Could show toast here
		});
	}
</script>

<!-- 
	LlamaBarn-style horizontal model row for installed models
	- Deep blue card background with subtle border
	- Icon on left, model info in center, actions on right
	- Hover: subtle lighten + border glow
	- Selected: highlighted background
-->
<div
	class="installed-model-row group flex items-center gap-4 px-4 py-3 rounded-lg transition-all duration-200 {selected
		? 'bg-[#1a2b44] border border-[#4cc9f0]/30'
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
	<!-- Family Icon -->
	<div class="flex-shrink-0 w-10 h-10 rounded-full bg-[#1a2b44] flex items-center justify-center text-xl">
		{familyIcon}
	</div>

	<!-- Model Info -->
	<div class="flex-1 min-w-0">
		<div class="flex items-center gap-2 mb-1">
			<h4 class="font-semibold text-sm text-[#e0e0ff] truncate">{model.display_name}</h4>
		</div>
		<div class="flex items-center gap-3 text-xs text-[#d0d8ff]/70">
			{#if model.size_str}
				<span>{model.size_str}</span>
			{/if}
			{#if model.quantization}
				<span>•</span>
				<span>{model.quantization}</span>
			{/if}
			{#if model.description}
				<span>•</span>
				<span class="truncate">{model.description}</span>
			{/if}
		</div>
	</div>

	<!-- Actions -->
	<div class="flex items-center gap-2 flex-shrink-0">
		<Tooltip.Provider>
			<Tooltip.Root>
				<Tooltip.Trigger>
					<Button
						variant="ghost"
						size="sm"
						class="h-8 w-8 p-0 text-[#d0d8ff]/60 hover:text-[#4cc9f0] hover:bg-[#1a2b44]"
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

		{#if onRemove}
			<Tooltip.Provider>
				<Tooltip.Root>
					<Tooltip.Trigger>
						<Button
							variant="ghost"
							size="sm"
							class="h-8 w-8 p-0 text-[#d0d8ff]/60 hover:text-red-400 hover:bg-red-500/10"
							onclick={() => onRemove(model.name)}
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
</style>

<script lang="ts">
	import { Server, Eye, Mic } from '@lucide/svelte';
	import { Badge } from '$lib/components/ui/badge';
	import { serverStore } from '$lib/stores/server.svelte';
	import { selectedModelName, selectedModelOption } from '$lib/stores/models.svelte';
	import { config } from '$lib/stores/settings.svelte';

	let modalities = $derived(serverStore.supportedModalities);
	let serverModel = $derived(serverStore.modelName);
	let props = $derived(serverStore.serverProps);
	let currentConfig = $derived(config());
	
	// Show selected model from dropdown if model selector is enabled, otherwise show server model
	let selectedModel = $derived(selectedModelOption());
	let selectedModelNameValue = $derived(selectedModelName());
	
	// Determine which model to display
	let displayModel = $derived(() => {
		// If model selector is enabled and we have a selected model, show it
		if (currentConfig.modelSelectorEnabled) {
			if (selectedModel) {
				// Prefer the display name from the model option
				const modelName = selectedModel.name;
				console.log('[ServerInfo] Displaying selected model:', modelName, 'from option:', selectedModel);
				return modelName;
			}
			if (selectedModelNameValue) {
				// Fall back to the stored model name/path
				// Extract just the filename if it's a path, remove .gguf extension
				let name = selectedModelNameValue;
				const parts = selectedModelNameValue.split(/[/\\]/);
				if (parts.length > 1) {
					name = parts[parts.length - 1];
				}
				// Remove .gguf extension if present
				if (name.endsWith('.gguf')) {
					name = name.slice(0, -5);
				}
				console.log('[ServerInfo] Displaying selected model name:', name, 'from value:', selectedModelNameValue);
				return name || selectedModelNameValue;
			}
		}
		// Otherwise show the server's model
		if (serverModel) {
			console.log('[ServerInfo] Displaying server model:', serverModel);
		}
		return serverModel;
	});
</script>

{#if props}
	<div class="flex flex-wrap items-center justify-center gap-4 text-sm text-muted-foreground">
		{#if displayModel()}
			<Badge variant="outline" class="text-xs">
				<Server class="mr-1 h-3 w-3" />

				<span class="block max-w-[50vw] truncate">{displayModel()}</span>
			</Badge>
		{/if}

		<div class="flex gap-4">
			{#if props.default_generation_settings.n_ctx}
				<Badge variant="secondary" class="text-xs">
					context: {props.default_generation_settings.n_ctx.toLocaleString()}
				</Badge>
			{/if}

			{#if modalities.length > 0}
				{#each modalities as modality (modality)}
					<Badge variant="secondary" class="text-xs">
						{#if modality === 'vision'}
							<Eye class="mr-1 h-3 w-3" />
						{:else if modality === 'audio'}
							<Mic class="mr-1 h-3 w-3" />
						{/if}

						{modality}
					</Badge>
				{/each}
			{/if}
		</div>
	</div>
{/if}

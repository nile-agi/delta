<script lang="ts">
	import { Server, Eye, Mic } from '@lucide/svelte';
	import { Badge } from '$lib/components/ui/badge';
	import { serverStore } from '$lib/stores/server.svelte';
	import { selectedModelName, selectedModelOption } from '$lib/stores/models.svelte';

	let modalities = $derived(serverStore.supportedModalities);
	let serverModel = $derived(serverStore.modelName);
	let props = $derived(serverStore.serverProps);
	let selectedModel = $derived(selectedModelOption());
	let selectedModelNameValue = $derived(selectedModelName());

	let displayModel = $derived(() => {
		if (selectedModel) return selectedModel.name;
		if (selectedModelNameValue) {
			let name = selectedModelNameValue;
			const parts = selectedModelNameValue.split(/[/\\]/);
			if (parts.length > 1) name = parts[parts.length - 1];
			if (name.endsWith('.gguf')) name = name.slice(0, -5);
			return name || selectedModelNameValue;
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

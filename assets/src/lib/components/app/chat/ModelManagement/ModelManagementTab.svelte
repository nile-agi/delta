<script lang="ts">
	import { ModelsService, type ModelInfo } from '$lib/services/models';
	import { Button } from '$lib/components/ui/button';
	import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '$lib/components/ui/card';
	import { Badge } from '$lib/components/ui/badge';
	import { Download, Trash2, RefreshCw, CheckCircle2, XCircle, Loader2 } from '@lucide/svelte';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import { onMount } from 'svelte';

	type TabType = 'available' | 'installed';

	let activeTab: TabType = $state('installed');
	let availableModels: ModelInfo[] = $state([]);
	let installedModels: ModelInfo[] = $state([]);
	let loading = $state(false);
	let error = $state<string | null>(null);
	let downloadingModel: string | null = $state(null);
	let removingModel: string | null = $state(null);
	let confirmDeleteModel: string | null = $state(null);

	async function loadModels() {
		loading = true;
		error = null;
		try {
			if (activeTab === 'available') {
				const response = await ModelsService.listAvailable();
				availableModels = response.models;
			} else {
				const response = await ModelsService.listInstalled();
				installedModels = response.models;
			}
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to load models';
			console.error('Error loading models:', e);
		} finally {
			loading = false;
		}
	}

	async function handleDownload(modelName: string) {
		downloadingModel = modelName;
		error = null;
		try {
			await ModelsService.download(modelName);
			// Reload both lists to update status
			await loadModels();
			if (activeTab === 'installed') {
				const response = await ModelsService.listInstalled();
				installedModels = response.models;
			}
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to download model';
			console.error('Error downloading model:', e);
		} finally {
			downloadingModel = null;
		}
	}

	async function handleRemove(modelName: string) {
		removingModel = modelName;
		error = null;
		try {
			await ModelsService.remove(modelName);
			// Reload lists
			await loadModels();
			if (activeTab === 'available') {
				const response = await ModelsService.listAvailable();
				availableModels = response.models;
			}
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to remove model';
			console.error('Error removing model:', e);
		} finally {
			removingModel = null;
			confirmDeleteModel = null;
		}
	}

	async function handleUse(modelName: string) {
		error = null;
		try {
			const result = await ModelsService.use(modelName);
			alert(
				result.message ||
					'Model switched successfully. Please restart the server with the new model.'
			);
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to switch model';
			console.error('Error switching model:', e);
		}
	}

	$effect(() => {
		loadModels();
	});
</script>

<div class="space-y-4">
	<div class="flex items-center justify-between">
		<div class="flex gap-2">
			<button
				class="px-4 py-2 rounded-lg text-sm font-medium transition-colors {activeTab === 'installed'
					? 'bg-accent text-accent-foreground'
					: 'text-muted-foreground hover:bg-accent'}"
				onclick={() => {
					activeTab = 'installed';
					loadModels();
				}}
			>
				Installed ({installedModels.length})
			</button>
			<button
				class="px-4 py-2 rounded-lg text-sm font-medium transition-colors {activeTab === 'available'
					? 'bg-accent text-accent-foreground'
					: 'text-muted-foreground hover:bg-accent'}"
				onclick={() => {
					activeTab = 'available';
					loadModels();
				}}
			>
				Available ({availableModels.length})
			</button>
		</div>
		<Button variant="outline" size="sm" onclick={loadModels} disabled={loading}>
			<RefreshCw class="h-4 w-4 mr-2 {loading ? 'animate-spin' : ''}" />
			Refresh
		</Button>
	</div>

	{#if error}
		<div class="bg-destructive/10 text-destructive px-4 py-3 rounded-lg text-sm">{error}</div>
	{/if}

	{#if loading && (activeTab === 'installed' ? installedModels.length === 0 : availableModels.length === 0)}
		<div class="flex items-center justify-center py-8">
			<Loader2 class="h-6 w-6 animate-spin text-muted-foreground" />
		</div>
	{:else if activeTab === 'installed'}
		{#if installedModels.length === 0}
			<div class="text-center py-8 text-muted-foreground">
				<p>No models installed.</p>
				<p class="text-sm mt-2">Switch to the "Available" tab to download models.</p>
			</div>
		{:else}
			<div class="space-y-3">
				{#each installedModels as model (model.name)}
					<Card>
						<CardHeader>
							<div class="flex items-start justify-between">
								<div class="flex-1">
									<CardTitle class="text-base">{model.display_name}</CardTitle>
									<CardDescription class="mt-1">{model.description}</CardDescription>
								</div>
								<Badge variant="secondary" class="ml-2">
									<CheckCircle2 class="h-3 w-3 mr-1" />
									Installed
								</Badge>
							</div>
						</CardHeader>
						<CardContent>
							<div class="flex items-center justify-between">
								<div class="text-sm text-muted-foreground">
									<span class="font-medium">Size:</span> {model.size_str} •{' '}
									<span class="font-medium">Quantization:</span> {model.quantization}
								</div>
								<div class="flex gap-2">
									<Button
										variant="outline"
										size="sm"
										onclick={() => handleUse(model.name)}
									>
										Use Model
									</Button>
									<Button
										variant="destructive"
										size="sm"
										onclick={() => (confirmDeleteModel = model.name)}
										disabled={removingModel === model.name}
									>
										{removingModel === model.name ? (
											<Loader2 class="h-4 w-4 animate-spin" />
										) : (
											<Trash2 class="h-4 w-4" />
										)}
									</Button>
								</div>
							</div>
						</CardContent>
					</Card>
				{/each}
			</div>
		{/if}
	{:else}
		{#if availableModels.length === 0}
			<div class="text-center py-8 text-muted-foreground">
				<Loader2 class="h-6 w-6 animate-spin mx-auto mb-2" />
				<p>Loading available models...</p>
			</div>
		{:else}
			<div class="space-y-3">
				{#each availableModels as model (model.name)}
					<Card>
						<CardHeader>
							<div class="flex items-start justify-between">
								<div class="flex-1">
									<CardTitle class="text-base">{model.display_name}</CardTitle>
									<CardDescription class="mt-1">{model.description}</CardDescription>
								</div>
								{#if model.installed}
									<Badge variant="secondary" class="ml-2">
										<CheckCircle2 class="h-3 w-3 mr-1" />
										Installed
									</Badge>
								{:else}
									<Badge variant="outline" class="ml-2">
										<XCircle class="h-3 w-3 mr-1" />
										Not Installed
									</Badge>
								{/if}
							</div>
						</CardHeader>
						<CardContent>
							<div class="flex items-center justify-between">
								<div class="text-sm text-muted-foreground">
									<span class="font-medium">Size:</span> {model.size_str} •{' '}
									<span class="font-medium">Quantization:</span> {model.quantization}
								</div>
								{#if !model.installed}
									<Button
										variant="default"
										size="sm"
										onclick={() => handleDownload(model.name)}
										disabled={downloadingModel === model.name}
									>
										{downloadingModel === model.name ? (
											<>
												<Loader2 class="h-4 w-4 mr-2 animate-spin" />
												Downloading...
											</>
										) : (
											<>
												<Download class="h-4 w-4 mr-2" />
												Download
											</>
										)}
									</Button>
								{:else}
									<Button variant="outline" size="sm" onclick={() => handleUse(model.name)}>
										Use Model
									</Button>
								{/if}
							</div>
						</CardContent>
					</Card>
				{/each}
			</div>
		{/if}
	{/if}
</div>

<!-- Delete Confirmation Dialog -->
<AlertDialog.Root open={confirmDeleteModel !== null}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title>Delete Model</AlertDialog.Title>
			<AlertDialog.Description>
				Are you sure you want to delete "{confirmDeleteModel}"? This action cannot be undone.
			</AlertDialog.Description>
		</AlertDialog.Header>
		<AlertDialog.Footer>
			<AlertDialog.Cancel onclick={() => (confirmDeleteModel = null)}>Cancel</AlertDialog.Cancel>
			<AlertDialog.Action
				onclick={() => {
					if (confirmDeleteModel) {
						handleRemove(confirmDeleteModel);
					}
				}}
			>
				Delete
			</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>


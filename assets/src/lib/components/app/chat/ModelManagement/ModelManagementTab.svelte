<script lang="ts">
	import { ModelsService, type ModelInfo } from '$lib/services/models';
	import { Button } from '$lib/components/ui/button';
	import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '$lib/components/ui/card';
	import { Badge } from '$lib/components/ui/badge';
	import { Download, Trash2, RefreshCw, CheckCircle2, XCircle, Loader2 } from '@lucide/svelte';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import { onMount, onDestroy } from 'svelte';

	type TabType = 'available' | 'installed';

	let activeTab: TabType = $state('installed');
	let availableModels: ModelInfo[] = $state([]);
	let installedModels: ModelInfo[] = $state([]);
	let loading = $state(false);
	let error = $state<string | null>(null);
	let downloadingModel = $state<string | null>(null);
	let downloadProgress = $state<{
		progress: number;
		current_bytes: number;
		total_bytes: number;
		completed: boolean;
		failed: boolean;
		error_message?: string;
	} | null>(null);
	let progressPollInterval: ReturnType<typeof setInterval> | null = null;
	let removingModel = $state<string | null>(null);
	let confirmDeleteModel = $state<string | null>(null);

	async function loadModels() {
		loading = true;
		error = null;
		try {
			if (activeTab === 'available') {
				const response = await ModelsService.listAvailable();
				// Handle both formats: {models: [...]} or [...] (for backward compatibility)
				availableModels = Array.isArray(response)
					? response
					: response.models || [];
				console.log('Loaded available models:', availableModels.length);
			} else {
				const response = await ModelsService.listInstalled();
				// Handle both formats: {models: [...]} or [...] (for backward compatibility)
				installedModels = Array.isArray(response)
					? response
					: response.models || [];
				console.log('Loaded installed models:', installedModels.length);
			}
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to load models';
			console.error('Error loading models:', e);
			// Set empty arrays on error
			if (activeTab === 'available') {
				availableModels = [];
			} else {
				installedModels = [];
			}
		} finally {
			loading = false;
		}
	}

	async function handleDownload(modelName: string) {
		console.log('[Download] Starting download for:', modelName);
		downloadingModel = modelName;
		downloadProgress = { progress: 0, current_bytes: 0, total_bytes: 0, completed: false, failed: false };
		error = null;
		
		try {
			// Start download (returns immediately)
			console.log('[Download] Calling download API...');
			await ModelsService.download(modelName);
			console.log('[Download] Download API returned, starting progress polling...');
			
			// Poll for progress immediately, then every 500ms
			const pollProgress = async () => {
				try {
					const progress = await ModelsService.getDownloadProgress(modelName);
					console.log('[Download] Progress update for', modelName, ':', progress);
					
					// Always update progress, even if 0
					if (progress) {
						downloadProgress = progress;
					}
					
					if (progress.completed || progress.failed) {
						console.log('[Download] Download finished:', progress.completed ? 'completed' : 'failed');
						if (progressPollInterval) {
							clearInterval(progressPollInterval);
							progressPollInterval = null;
						}
						
						if (progress.completed) {
							// Reload both lists to update status
							await loadModels();
							if (activeTab === 'installed') {
								const response = await ModelsService.listInstalled();
								installedModels = response.models;
							}
							// Clear progress after a short delay
							setTimeout(() => {
								downloadProgress = null;
								downloadingModel = null;
							}, 2000);
						} else if (progress.failed) {
							error = progress.error_message || 'Download failed';
							downloadProgress = null;
							downloadingModel = null;
						}
					}
				} catch (e) {
					console.error('[Download] Error polling download progress:', e);
					// Don't stop polling on error, might be temporary
				}
			};
			
			// Poll immediately
			await pollProgress();
			
			// Then poll every 500ms
			progressPollInterval = setInterval(pollProgress, 500);
		} catch (e) {
			error = e instanceof Error ? e.message : 'Failed to download model';
			console.error('[Download] Error downloading model:', e);
			if (progressPollInterval) {
				clearInterval(progressPollInterval);
				progressPollInterval = null;
			}
			downloadProgress = null;
			downloadingModel = null;
		}
	}
	
	onDestroy(() => {
		if (progressPollInterval) {
			clearInterval(progressPollInterval);
		}
	});

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

	onMount(() => {
		console.log('ModelManagementTab mounted');
		loadModels();
	});
</script>

<div class="space-y-4" data-model-management="true" style="display: block;">
	<!-- Debug: Component is rendering -->
	<div class="mb-2 text-xs text-green-600 dark:text-green-400">✓ Model Management Component Loaded</div>
	
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
										{#if removingModel === model.name}
											<Loader2 class="h-4 w-4 animate-spin" />
										{:else}
											<Trash2 class="h-4 w-4" />
										{/if}
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
							<div class="space-y-2">
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
											{#if downloadingModel === model.name}
												<Loader2 class="h-4 w-4 mr-2 animate-spin" />
												Downloading...
											{:else}
												<Download class="h-4 w-4 mr-2" />
												Download
											{/if}
										</Button>
									{:else}
										<Button variant="outline" size="sm" onclick={() => handleUse(model.name)}>
											Use Model
										</Button>
									{/if}
								</div>
								{#if downloadingModel === model.name && downloadProgress !== null}
									<div class="space-y-1.5 mt-2 pt-2 border-t border-border">
										<div class="flex items-center justify-between text-xs">
											<span class="text-muted-foreground font-medium">
												Downloading: {downloadProgress.progress.toFixed(1)}%
											</span>
											<span class="text-muted-foreground">
												{(downloadProgress.current_bytes / (1024 * 1024)).toFixed(1)} MB /{' '}
												{(downloadProgress.total_bytes / (1024 * 1024)).toFixed(1)} MB
											</span>
										</div>
										<div class="w-full bg-secondary rounded-full h-2.5 overflow-hidden shadow-inner">
											<div
												class="h-full bg-primary transition-all duration-300 ease-out rounded-full"
												style="width: {Math.max(0, Math.min(100, downloadProgress.progress))}%"
											></div>
										</div>
									</div>
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


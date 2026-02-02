<script lang="ts">
	import { ModelsService, type ModelInfo } from '$lib/services/models';
	import { modelsCatalog, getSmallestCompatibleModel } from '$lib/data/models_catalog';
	import FamilyAccordion from './FamilyAccordion.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Input } from '$lib/components/ui/input';
	import { Search, RefreshCw, Loader2, Download, CheckCircle2, Trash2 } from '@lucide/svelte';
	import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '$lib/components/ui/card';
	import { Badge } from '$lib/components/ui/badge';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import { onMount, onDestroy } from 'svelte';
	import { toast } from 'svelte-sonner';

	type ViewMode = 'catalog' | 'installed';

	let viewMode: ViewMode = $state('installed');
	let searchQuery = $state('');
	let systemRAMGB = $state<number | null>(null);
	let loadingRAM = $state(false);
	let installedModels: ModelInfo[] = $state([]);
	let loadingInstalled = $state(false);
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
	let autoDownloadAttempted = $state(false);

	// Get installed model names as a Set for quick lookup
	const installedModelNames = $derived(new Set(installedModels.map((m) => m.name)));

	// Filter families based on search query
	const filteredFamilies = $derived.by(() => {
		if (!searchQuery.trim()) {
			return modelsCatalog;
		}
		const query = searchQuery.toLowerCase();
		return modelsCatalog.filter(
			(family) =>
				family.name.toLowerCase().includes(query) ||
				family.description.toLowerCase().includes(query) ||
				family.models.some(
					(model) =>
						model.name.toLowerCase().includes(query) ||
						model.display_name.toLowerCase().includes(query)
				)
		);
	});

	// Filter installed models based on search query
	const filteredInstalledModels = $derived.by(() => {
		if (!searchQuery.trim()) {
			return installedModels;
		}
		const query = searchQuery.toLowerCase();
		return installedModels.filter(
			(model) =>
				model.name.toLowerCase().includes(query) ||
				model.display_name.toLowerCase().includes(query) ||
				model.description.toLowerCase().includes(query)
		);
	});

	async function loadSystemRAM() {
		loadingRAM = true;
		try {
			const ramInfo = await ModelsService.getSystemRAM();
			systemRAMGB = ramInfo.total_ram_gb;
		} catch (error) {
			console.error('Failed to load system RAM:', error);
			// Fallback: try browser API
			if (typeof navigator !== 'undefined' && 'deviceMemory' in navigator) {
				const deviceMemory = (navigator as any).deviceMemory;
				if (deviceMemory) {
					systemRAMGB = deviceMemory;
				}
			}
			// If still no RAM info, default to 8GB for safety
			if (systemRAMGB === null) {
				systemRAMGB = 8;
			}
		} finally {
			loadingRAM = false;
		}
	}

	async function loadInstalledModels() {
		loadingInstalled = true;
		try {
			const response = await ModelsService.listInstalled();
			installedModels = Array.isArray(response) ? response : response.models || [];
		} catch (error) {
			console.error('Error loading installed models:', error);
			installedModels = [];
		} finally {
			loadingInstalled = false;
		}
	}

	async function handleDownload(modelName: string) {
		console.log('[Download] Starting download for:', modelName);
		downloadingModel = modelName;
		downloadProgress = { progress: 0, current_bytes: 0, total_bytes: 0, completed: false, failed: false };
		
		try {
			await ModelsService.download(modelName);
			
			const pollProgress = async () => {
				try {
					const progress = await ModelsService.getDownloadProgress(modelName);
					downloadProgress = progress;
					
					if (progress.completed || progress.failed) {
						if (progressPollInterval) {
							clearInterval(progressPollInterval);
							progressPollInterval = null;
						}
						
						if (progress.completed) {
							toast.success(`Model ${modelName} downloaded successfully`);
							await loadInstalledModels();
							setTimeout(() => {
								downloadProgress = null;
								downloadingModel = null;
							}, 2000);
						} else if (progress.failed) {
							toast.error(progress.error_message || 'Download failed');
							downloadProgress = null;
							downloadingModel = null;
						}
					}
				} catch (e) {
					console.error('[Download] Error polling progress:', e);
				}
			};
			
			await pollProgress();
			progressPollInterval = setInterval(pollProgress, 500);
		} catch (e) {
			const errorMessage = e instanceof Error ? e.message : 'Failed to download model';
			toast.error(errorMessage);
			console.error('[Download] Error downloading model:', e);
			if (progressPollInterval) {
				clearInterval(progressPollInterval);
				progressPollInterval = null;
			}
			downloadProgress = null;
			downloadingModel = null;
		}
	}

	async function handleRemove(modelName: string) {
		removingModel = modelName;
		try {
			await ModelsService.remove(modelName);
			toast.success(`Model ${modelName} removed successfully`);
			await loadInstalledModels();
		} catch (e) {
			const errorMessage = e instanceof Error ? e.message : 'Failed to remove model';
			toast.error(errorMessage);
			console.error('Error removing model:', e);
		} finally {
			removingModel = null;
			confirmDeleteModel = null;
		}
	}

	async function autoDownloadSmallestCompatible() {
		if (autoDownloadAttempted || systemRAMGB === null || installedModels.length > 0) {
			return;
		}

		autoDownloadAttempted = true;
		const smallestModel = getSmallestCompatibleModel(systemRAMGB);
		
		if (smallestModel) {
			toast.info(`Auto-downloading smallest compatible model: ${smallestModel.display_name}`);
			await handleDownload(smallestModel.name);
		} else {
			console.warn('No compatible models found for auto-download');
		}
	}

	onMount(async () => {
		console.log('ModelManagementTab mounted');
		await loadSystemRAM();
		await loadInstalledModels();
		
		// Auto-download smallest compatible model if none installed
		if (installedModels.length === 0 && systemRAMGB !== null) {
			// Delay auto-download slightly to let UI render
			setTimeout(() => {
				autoDownloadSmallestCompatible();
			}, 1000);
		}
	});

	onDestroy(() => {
		if (progressPollInterval) {
			clearInterval(progressPollInterval);
		}
	});
</script>

<div class="model-management-container space-y-6" style="background: #001F3F; color: white;">
	<!-- Header with search and view toggle -->
	<div class="flex flex-col gap-4 md:flex-row md:items-center md:justify-between">
		<div class="flex items-center gap-2">
			<Button
				variant={viewMode === 'installed' ? 'default' : 'outline'}
				size="sm"
				onclick={() => {
					viewMode = 'installed';
					searchQuery = '';
				}}
			>
				Installed ({installedModels.length})
			</Button>
			<Button
				variant={viewMode === 'catalog' ? 'default' : 'outline'}
				size="sm"
				onclick={() => {
					viewMode = 'catalog';
					searchQuery = '';
				}}
			>
				Catalog
			</Button>
		</div>

		<div class="flex items-center gap-2">
			<div class="relative flex-1 md:flex-initial md:w-64">
				<Search class="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-muted-foreground" />
				<Input
					type="text"
					placeholder="Search models..."
					bind:value={searchQuery}
					class="pl-9 bg-background/50 border-border/30 text-foreground"
				/>
			</div>
			<Button variant="outline" size="sm" onclick={loadInstalledModels} disabled={loadingInstalled}>
				<RefreshCw class="h-4 w-4 mr-2 {loadingInstalled ? 'animate-spin' : ''}" />
				Refresh
			</Button>
		</div>
	</div>

	<!-- System RAM Info -->
	{#if systemRAMGB !== null}
		<div class="text-sm text-muted-foreground">
			System RAM: <span class="font-semibold text-foreground">{systemRAMGB} GB</span>
		</div>
	{:else if loadingRAM}
		<div class="flex items-center gap-2 text-sm text-muted-foreground">
			<Loader2 class="h-4 w-4 animate-spin" />
			Detecting system RAM...
		</div>
	{/if}

	<!-- Installed Models View -->
	{#if viewMode === 'installed'}
		{#if loadingInstalled && installedModels.length === 0}
			<div class="flex items-center justify-center py-12">
				<Loader2 class="h-8 w-8 animate-spin text-muted-foreground" />
			</div>
		{:else if filteredInstalledModels.length === 0}
			<Card class="bg-card/30 border-border/30">
				<CardContent class="pt-6">
					<div class="text-center py-8">
						<p class="text-muted-foreground mb-2">
							{#if searchQuery}
								No installed models match your search.
							{:else}
								No models installed.
							{/if}
						</p>
						{#if !searchQuery}
							<p class="text-sm text-muted-foreground">
								Switch to the "Catalog" tab to download models.
							</p>
						{/if}
					</div>
				</CardContent>
			</Card>
		{:else}
			<div class="space-y-3">
				{#each filteredInstalledModels as model (model.name)}
					<Card class="bg-card/50 border-border/30">
						<CardHeader>
							<div class="flex items-start justify-between">
								<div class="flex-1">
									<CardTitle class="text-base">{model.display_name}</CardTitle>
									<CardDescription class="mt-1">{model.description}</CardDescription>
								</div>
								<Badge variant="secondary">
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
						</CardContent>
					</Card>
				{/each}
			</div>
		{/if}
	<!-- Catalog View -->
	{:else}
		{#if systemRAMGB === null && !loadingRAM}
			<Card class="bg-card/30 border-border/30">
				<CardContent class="pt-6">
					<div class="text-center py-8 text-muted-foreground">
						<p>Unable to detect system RAM. Hardware-aware filtering disabled.</p>
					</div>
				</CardContent>
			</Card>
		{:else if filteredFamilies.length === 0}
			<Card class="bg-card/30 border-border/30">
				<CardContent class="pt-6">
					<div class="text-center py-8 text-muted-foreground">
						<p>No model families match your search.</p>
					</div>
				</CardContent>
			</Card>
		{:else}
			<div class="space-y-4">
				{#each filteredFamilies as family (family.id)}
					<FamilyAccordion
						{family}
						expanded={false}
						systemRAMGB={systemRAMGB || 8}
						{installedModelNames}
						onModelDownload={handleDownload}
						onModelRemove={handleRemove}
						{downloadingModel}
						removingModel={removingModel}
						{downloadProgress}
					/>
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

<style>
	.model-management-container {
		min-height: 400px;
		padding: 1rem;
		border-radius: 0.5rem;
	}

	:global(.model-management-container input) {
		color: white;
	}

	:global(.model-management-container input::placeholder) {
		color: rgba(255, 255, 255, 0.5);
	}
</style>

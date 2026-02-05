<script lang="ts">
	import { ModelsService, type ModelInfo } from '$lib/services/models';
	import { slotsService } from '$lib/services/slots';
	import { modelsCatalog, getSmallestCompatibleModel } from '$lib/data/models_catalog';
	import { selectedModelName as getSelectedModelName } from '$lib/stores/models.svelte';
	import FamilyAccordion from './FamilyAccordion.svelte';
	import InstalledModelRow from './InstalledModelRow.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Input } from '$lib/components/ui/input';
	import { Search, RefreshCw, Loader2 } from '@lucide/svelte';
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

	const selectedModelName = $derived(getSelectedModelName());

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
				const deviceMemory = (navigator as Navigator & { deviceMemory?: number }).deviceMemory;
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
		downloadProgress = {
			progress: 0,
			current_bytes: 0,
			total_bytes: 0,
			completed: false,
			failed: false
		};

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

	async function handleContextChange(modelName: string, ctx: number) {
		// If this model is currently selected, update backend so server uses new context (llama-server -c)
		if (selectedModelName === modelName) {
			try {
				const useResponse = await ModelsService.use(modelName, ctx);
				toast.success(`Context set to ${ctx >= 1000 ? ctx / 1000 + 'k' : ctx} for ${modelName}`);
				// Update Context stat immediately when backend reloads with new ctx
				if (useResponse.loaded && useResponse.ctx_size != null && useResponse.ctx_size > 0) {
					slotsService.setLoadedContextTotal(useResponse.ctx_size);
				}
			} catch (error) {
				console.error('Failed to set context:', error);
				toast.error(
					`Failed to set context: ${error instanceof Error ? error.message : String(error)}`
				);
			}
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

<!-- 
	LlamaBarn-style Model Management Panel
	- Deep navy background (#0a1421 / #001f3f)
	- Clean card styling with proper spacing
	- Pill-shaped tabs
	- Prominent system RAM display
-->
<div class="model-management-container">
	<!-- Top Bar: Tabs, Search, RAM Display -->
	<div class="mb-6 flex flex-col gap-4">
		<!-- Tabs and Search Row -->
		<div class="flex items-center justify-between gap-4">
			<!-- Pill-shaped Tabs -->
			<div class="flex items-center gap-2 rounded-full border border-[#1a2b44]/50 bg-[#0a1421] p-1">
				<button
					class="rounded-full px-4 py-1.5 text-sm font-medium transition-all duration-200 {viewMode ===
					'installed'
						? 'bg-[#4cc9f0] text-white shadow-lg shadow-[#4cc9f0]/20'
						: 'text-[#d0d8ff]/60 hover:text-[#e0e0ff]'}"
					onclick={() => {
						viewMode = 'installed';
						searchQuery = '';
					}}
					type="button"
				>
					Installed ({installedModels.length})
				</button>
				<button
					class="rounded-full px-4 py-1.5 text-sm font-medium transition-all duration-200 {viewMode ===
					'catalog'
						? 'bg-[#4cc9f0] text-white shadow-lg shadow-[#4cc9f0]/20'
						: 'text-[#d0d8ff]/60 hover:text-[#e0e0ff]'}"
					onclick={() => {
						viewMode = 'catalog';
						searchQuery = '';
					}}
					type="button"
				>
					Catalog
				</button>
			</div>

			<!-- Search Bar -->
			<div class="relative max-w-md flex-1">
				<Search
					class="absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 transform text-[#d0d8ff]/50"
				/>
				<Input
					type="text"
					placeholder="Search models..."
					bind:value={searchQuery}
					class="rounded-lg border-[#1a2b44] bg-[#11243a] pl-9 text-[#e0e0ff] placeholder:text-[#d0d8ff]/40 focus:border-[#4cc9f0]/50 focus:ring-1 focus:ring-[#4cc9f0]/20"
				/>
			</div>

			<!-- Refresh Button -->
			<Button
				variant="ghost"
				size="sm"
				class="text-[#d0d8ff]/60 hover:bg-[#1a2b44] hover:text-[#4cc9f0]"
				onclick={loadInstalledModels}
				disabled={loadingInstalled}
			>
				<RefreshCw class="h-4 w-4 {loadingInstalled ? 'animate-spin' : ''}" />
			</Button>
		</div>

		<!-- System RAM Display (Prominent, Right-aligned) -->
		<div class="flex items-center justify-end">
			{#if systemRAMGB !== null}
				<div class="text-sm text-[#d0d8ff]/70">
					System RAM: <span class="font-bold text-[#4cc9f0]">{systemRAMGB} GB</span>
				</div>
			{:else if loadingRAM}
				<div class="flex items-center gap-2 text-sm text-[#d0d8ff]/50">
					<Loader2 class="h-4 w-4 animate-spin" />
					Detecting system RAM...
				</div>
			{/if}
		</div>
	</div>

	<!-- Installed Models View -->
	{#if viewMode === 'installed'}
		{#if loadingInstalled && installedModels.length === 0}
			<div class="flex items-center justify-center py-16">
				<Loader2 class="h-8 w-8 animate-spin text-[#4cc9f0]" />
			</div>
		{:else if filteredInstalledModels.length === 0}
			<div class="py-16 text-center text-[#d0d8ff]/50">
				<p class="mb-2">
					{#if searchQuery}
						No installed models match your search.
					{:else}
						No models installed.
					{/if}
				</p>
				{#if !searchQuery}
					<p class="text-sm">Switch to the "Catalog" tab to download models.</p>
				{/if}
			</div>
		{:else}
			<!-- Installed Models List: manage context length and delete only. Load/select model via chat model selector. -->
			<div class="space-y-2">
				{#each filteredInstalledModels as model (model.name)}
					<InstalledModelRow
						{model}
						onRemove={handleRemove}
						onContextChange={handleContextChange}
						removing={removingModel === model.name}
						{systemRAMGB}
					/>
				{/each}
			</div>
		{/if}
		<!-- Catalog View -->
	{:else if systemRAMGB === null && !loadingRAM}
		<div class="py-16 text-center text-[#d0d8ff]/50">
			<p>Unable to detect system RAM. Hardware-aware filtering disabled.</p>
		</div>
	{:else if filteredFamilies.length === 0}
		<div class="py-16 text-center text-[#d0d8ff]/50">
			<p>No model families match your search.</p>
		</div>
	{:else}
		<!-- Model Families with Accordions -->
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
					{removingModel}
					{downloadProgress}
				/>
			{/each}
		</div>
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
	/* 
		LlamaBarn-inspired dark theme
		- Deep navy background (#0a1421) for main container
		- Slightly lighter cards (#11243a / #1a2b44)
		- White/light gray text (#e0e0ff / #d0d8ff)
		- Cyan accent (#4cc9f0) for highlights
	*/
	.model-management-container {
		min-height: 400px;
		padding: 1.5rem;
		background: #0a1421;
		border-radius: 0.75rem;
		color: #e0e0ff;
	}

	/* Ensure inputs use proper colors */
	:global(.model-management-container input) {
		color: #e0e0ff;
		background-color: #11243a;
		border-color: #1a2b44;
	}

	:global(.model-management-container input::placeholder) {
		color: rgba(208, 216, 255, 0.4);
	}

	:global(.model-management-container input:focus) {
		border-color: rgba(76, 201, 240, 0.5);
		outline: none;
		box-shadow: 0 0 0 1px rgba(76, 201, 240, 0.2);
	}
</style>

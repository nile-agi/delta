<script lang="ts">
	import { ModelsService, type ModelInfo } from '$lib/services/models';
	import { slotsService } from '$lib/services/slots';
	import { modelsCatalog, groupFamiliesByProvider, findModelByName } from '$lib/data/models_catalog';
	import { selectedModelName as getSelectedModelName, fetchModels } from '$lib/stores/models.svelte';
	import FamilyAccordion from './FamilyAccordion.svelte';
	import InstalledModelRow from './InstalledModelRow.svelte';
	import DownloadingModelRow from './DownloadingModelRow.svelte';
	import { downloads } from '$lib/stores/downloads.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Input } from '$lib/components/ui/input';
	import { Search, RefreshCw, Loader2 } from '@lucide/svelte';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import { onMount } from 'svelte';
	import { toast } from 'svelte-sonner';

	type ViewMode = 'catalog' | 'installed';

	let viewMode: ViewMode = $state('installed');
	let searchQuery = $state('');
	let systemRAMGB = $state<number | null>(null);
	let loadingRAM = $state(false);
	let installedModels: ModelInfo[] = $state([]);
	let loadingInstalled = $state(false);
	let removingModel = $state<string | null>(null);
	let confirmDeleteModel = $state<string | null>(null);

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

	// In-flight downloads aren't installed yet, so they have no ModelInfo — surface them in the
	// Installed view from the store instead, filtered by the same search box.
	const filteredDownloads = $derived.by(() => {
		const query = searchQuery.trim().toLowerCase();
		if (!query) return downloads.active;
		// Match display name too, so searching behaves the same as it does for installed rows.
		return downloads.active.filter(
			(d) =>
				d.model.toLowerCase().includes(query) ||
				(findModelByName(d.model)?.display_name ?? '').toLowerCase().includes(query)
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
		// The store owns the poll loop and the terminal toasts, so progress survives this
		// component being unmounted when settings is closed, minimized or switched away from.
		await downloads.start(modelName);
	}

	async function handleStopDownload(modelName: string) {
		await downloads.cancel(modelName);
	}

	async function handleRemove(modelName: string) {
		removingModel = modelName;
		try {
			await ModelsService.remove(modelName);
			toast.success(`Model ${modelName} removed successfully`);
			await loadInstalledModels();
			await fetchModels(true);
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

	onMount(async () => {
		await loadSystemRAM();
		await loadInstalledModels();
	});

	// downloads.completionTick bumps whenever a download reaches a terminal state; a newly
	// finished model needs to appear in the installed list without a manual refresh.
	// Plain `let`, not $state: the effect writes this, and tracking it would make the effect
	// re-run on its own write.
	let lastCompletionTick = downloads.completionTick;
	$effect(() => {
		const tick = downloads.completionTick;
		if (tick === lastCompletionTick) return;
		lastCompletionTick = tick;
		void loadInstalledModels();
	});
</script>

<!-- 
	LlamaBarn-style Model Management Panel
	- Deep navy background (#0a1421 / #001f3f)
	- Clean card styling with proper spacing
	- Pill-shaped tabs
	- Prominent system RAM display
-->
<div class="model-management-container flex min-h-0 flex-1 flex-col overflow-hidden rounded-lg border border-border bg-background text-foreground">
	<!-- Top Bar: Tabs, Search, RAM Display (fixed header) -->
	<div class="mb-4 flex shrink-0 flex-col gap-4 px-6 pt-6">
		<!-- Tabs and Search Row -->
		<div class="flex items-center justify-between gap-4">
			<!-- Pill-shaped Tabs -->
			<div class="flex items-center gap-2 rounded-full border border-border bg-muted p-1">
				<button
					class="rounded-full px-4 py-1.5 text-sm font-medium transition-all duration-200 {viewMode ===
					'installed'
						? 'bg-primary text-primary-foreground shadow-sm'
						: 'text-muted-foreground hover:text-foreground'}"
					onclick={() => {
						viewMode = 'installed';
						searchQuery = '';
					}}
					type="button"
				>
					Installed ({installedModels.length}){downloads.activeCount > 0
						? ` · ${downloads.activeCount} downloading`
						: ''}
				</button>
				<button
					class="rounded-full px-4 py-1.5 text-sm font-medium transition-all duration-200 {viewMode ===
					'catalog'
						? 'bg-primary text-primary-foreground shadow-sm'
						: 'text-muted-foreground hover:text-foreground'}"
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
					class="absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 transform text-muted-foreground"
				/>
				<Input
					type="text"
					placeholder="Search models..."
					bind:value={searchQuery}
					class="w-full pl-9"
				/>
			</div>

			<!-- Refresh Button -->
			<Button
				variant="ghost"
				size="sm"
				class="text-muted-foreground hover:bg-accent hover:text-accent-foreground"
				onclick={loadInstalledModels}
				disabled={loadingInstalled}
			>
				<RefreshCw class="h-4 w-4 {loadingInstalled ? 'animate-spin' : ''}" />
			</Button>
		</div>

		<!-- System RAM Display (Prominent, Right-aligned) -->
		<div class="flex items-center justify-end">
			{#if systemRAMGB !== null}
				<div class="text-sm text-muted-foreground">
					System RAM: <span class="font-bold text-foreground">{systemRAMGB} GB</span>
				</div>
			{:else if loadingRAM}
				<div class="flex items-center gap-2 text-sm text-muted-foreground">
					<Loader2 class="h-4 w-4 animate-spin" />
					Detecting system RAM...
				</div>
			{/if}
		</div>
	</div>

	<!-- Scrollable model list (header above stays fixed) -->
	<div class="min-h-0 flex-1 overflow-y-auto px-6 pb-6">
		<!-- Installed Models View -->
		{#if viewMode === 'installed'}
			{#if loadingInstalled && installedModels.length === 0 && filteredDownloads.length === 0}
				<div class="flex items-center justify-center py-16">
					<Loader2 class="h-8 w-8 animate-spin text-primary" />
				</div>
			{:else if filteredInstalledModels.length === 0 && filteredDownloads.length === 0}
				<div class="py-16 text-center text-muted-foreground">
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
					<!-- In-flight downloads first: they're the thing the user is waiting on. -->
					{#each filteredDownloads as download (download.model)}
						<DownloadingModelRow {download} onCancel={handleStopDownload} />
					{/each}
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
			<div class="py-16 text-center text-muted-foreground">
				<p>Unable to detect system RAM. Hardware-aware filtering disabled.</p>
			</div>
		{:else if filteredFamilies.length === 0}
			<div class="py-16 text-center text-muted-foreground">
				<p>No model families match your search.</p>
			</div>
		{:else}
			<!-- Catalog grouped by provider -->
			<div class="space-y-6">
				{#each groupFamiliesByProvider(filteredFamilies) as group (group.provider)}
					<div>
						<div class="mb-2 px-1 text-xs font-semibold uppercase tracking-wide text-muted-foreground">
							{group.provider}
						</div>
						<div class="space-y-3">
							{#each group.families as family (family.id)}
								<FamilyAccordion
									{family}
									expanded={false}
									systemRAMGB={systemRAMGB || 8}
									{installedModelNames}
									onModelDownload={handleDownload}
									onModelRemove={handleRemove}
									onModelStopDownload={handleStopDownload}
									{removingModel}
								/>
							{/each}
						</div>
					</div>
				{/each}
			</div>
		{/if}
	</div>
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

<!-- Option A: uses app design tokens (background, foreground, muted, border, primary, accent) for light/dark consistency -->

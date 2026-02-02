<script lang="ts">
	import { Trash2, Copy, Loader2, CheckCircle2, Info } from '@lucide/svelte';
	import { slide } from 'svelte/transition';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import type { ModelInfo } from '$lib/services/models';
	import {
		findModelByName,
		getContextOptionsForModel,
		estimateMemoryGB,
		getFamilyIconForModelName
	} from '$lib/data/models_catalog';

	const STORAGE_KEY_PREFIX = 'delta_model_ctx_';

	interface Props {
		model: ModelInfo;
		onRemove?: (modelName: string) => void;
		onSelect?: (modelName: string) => void;
		onContextChange?: (modelName: string, ctx: number) => void;
		removing?: boolean;
		selected?: boolean;
		/** When true, show context length section (so first model can be expanded by default) */
		expanded?: boolean;
		systemRAMGB?: number | null;
	}

	let {
		model,
		onRemove,
		onSelect,
		onContextChange,
		removing = false,
		selected = false,
		expanded = false,
		systemRAMGB = null
	}: Props = $props();

	/** Show context length block when row is selected for chat OR expanded (clicked / default first) */
	const showContextLength = $derived(selected || expanded);

	/** File size in GB for memory estimation (from size_bytes or catalog). */
	const fileSizeGB = $derived.by(() => {
		if (model.size_bytes > 0) {
			return model.size_bytes / (1024 * 1024 * 1024);
		}
		const catalogModel = findModelByName(model.name);
		return catalogModel?.file_size_gb ?? 1;
	});

	const catalogModel = $derived(findModelByName(model.name));
	const contextOptions = $derived(getContextOptionsForModel(catalogModel?.context_size));

	/** Persisted context choice (localStorage); default first option or 8192. */
	let selectedCtx = $state(8192);

	// Hydrate from localStorage and keep in sync with contextOptions
	$effect(() => {
		const opts = contextOptions;
		if (opts.length === 0) return;
		const raw = typeof window !== 'undefined' ? localStorage.getItem(STORAGE_KEY_PREFIX + model.name) : null;
		if (raw) {
			const n = parseInt(raw, 10);
			if (opts.includes(n)) selectedCtx = n;
		} else if (!opts.includes(selectedCtx)) {
			selectedCtx = opts[0];
		}
	});

	function setContext(ctx: number) {
		selectedCtx = ctx;
		if (typeof window !== 'undefined') {
			localStorage.setItem(STORAGE_KEY_PREFIX + model.name, String(ctx));
		}
		onContextChange?.(model.name, ctx);
	}

	/** Family icon from shared util (LlamaBarn: ◇ ∇ M etc.) */
	const familyIcon = $derived(getFamilyIconForModelName(model.name));

	/** LlamaBarn-style tags: ∞ (large ctx), Q (quantized), ○○ (Thinking) */
	const modelTags = $derived.by(() => {
		const tags: string[] = [];
		const name = (model.display_name || model.name);
		if (name.includes('∞') || (catalogModel?.context_size && catalogModel.context_size >= 128_000)) tags.push('∞');
		if (model.quantization) tags.push('Q');
		if (name.toLowerCase().includes('thinking')) tags.push('○○');
		return tags;
	});

	/**
	 * Format context size for display in the main row.
	 * Prefer the user's selected context (selectedCtx) when available so the row shows
	 * the active choice (e.g. "16k ctx"); otherwise use catalog default.
	 */
	function getContextSizeDisplay(): string {
		// Show user's selected context when we have options (LlamaBarn: row shows current selection)
		if (contextOptions.length > 0 && contextOptions.includes(selectedCtx)) {
			if (selectedCtx >= 1000000) return `${(selectedCtx / 1000000).toFixed(1)}M ctx`;
			if (selectedCtx >= 1000) return `${(selectedCtx / 1000).toFixed(0)}k ctx`;
			return `${selectedCtx} ctx`;
		}
		// Fallback: catalog model default
		const cat = findModelByName(model.name);
		if (cat?.context_size) {
			const ctx = cat.context_size;
			if (ctx >= 1000000) return `${(ctx / 1000000).toFixed(1)}M ctx`;
			if (ctx >= 1000) return `${(ctx / 1000).toFixed(0)}k ctx`;
			return `${ctx} ctx`;
		}
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
	- When selected: "Context length (i)" subsection with radio options and mem estimates
-->
<div
	class="installed-model-row group flex flex-col rounded-lg transition-all duration-200 {showContextLength
		? 'bg-[#1a2b44] border border-[#4cc9f0]/40 shadow-lg shadow-[#4cc9f0]/10'
		: 'bg-[#11243a] border border-[#1a2b44]/50 hover:bg-[#1a2b44] hover:border-[#4cc9f0]/20'}"
>
	<!-- Main row: clickable to select -->
	<div
		class="flex items-center gap-4 px-4 py-3 cursor-pointer min-h-0"
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

		<!-- Model Info - Center: name, optional tags (∞ Q ○○), selected check -->
		<div class="flex-1 min-w-0">
			<div class="flex items-center gap-2 mb-1 flex-wrap">
				<h4 class="font-semibold text-sm text-[#e0e0ff] truncate">{model.display_name || model.name}</h4>
				{#each modelTags as tag}
					<span class="text-xs text-[#d0d8ff]/70 font-normal" aria-hidden="true">{tag}</span>
				{/each}
				{#if selected}
					<CheckCircle2 class="h-4 w-4 text-[#4cc9f0] flex-shrink-0" />
				{/if}
			</div>
			<div class="flex items-center gap-2 text-xs text-[#d0d8ff]/70 flex-wrap">
				{#if model.size_str}
					<span>{model.size_str}</span>
				{/if}
				{#if model.size_str && (model.quantization || getContextSizeDisplay())}
					<span class="text-[#d0d8ff]/40">|</span>
				{/if}
				{#if model.quantization}
					<span>{model.quantization}</span>
				{/if}
				{#if model.quantization && getContextSizeDisplay()}
					<span class="text-[#d0d8ff]/40">|</span>
				{/if}
				<span>{getContextSizeDisplay()}</span>
			</div>
		</div>

		<!-- Actions - Right side -->
		<div class="flex items-center gap-2 flex-shrink-0">
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

	<!--
		Context length subsection (LlamaBarn-style): expandable when row is selected.
		Smooth slide transition; dark theme (#1a2b44, #e0e0ff/#d0d8ff, #444 radios, #4cc9f0 accent).
		Filter: options with mem > system RAM are grayed out; if none viable, show "No compatible context lengths".
	-->
	{#if showContextLength}
		<!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
		<div
			class="context-length-wrapper overflow-hidden"
			onclick={(e) => e.stopPropagation()}
			onkeydown={(e) => e.stopPropagation()}
			role="group"
			aria-label="Context length"
			transition:slide={{ duration: 200 }}
		>
			<div class="context-length-section px-4 pb-3 pt-0 pl-14 text-[#d0d8ff]/80">
				{#if contextOptions.length > 0}
					{@const optionsWithMem = contextOptions.map((ctx) => ({ ctx, memGB: estimateMemoryGB(fileSizeGB, ctx) }))}
					{@const allDisabled = systemRAMGB != null && optionsWithMem.every((o) => o.memGB > systemRAMGB)}
					<div class="flex items-center gap-2 mb-2">
						<span class="text-sm font-medium text-[#e0e0ff]">Context length</span>
						<Tooltip.Provider>
							<Tooltip.Root>
								<Tooltip.Trigger>
									<span class="inline-flex items-center justify-center w-5 h-5 rounded-full bg-[#1a2b44] text-[#d0d8ff]/60 hover:text-[#4cc9f0] cursor-help" aria-label="Info">
										<Info class="h-3.5 w-3.5" />
									</span>
								</Tooltip.Trigger>
								<Tooltip.Content>
									<p>Maximum context (tokens). Higher values use more memory (KV cache).</p>
								</Tooltip.Content>
							</Tooltip.Root>
						</Tooltip.Provider>
					</div>
					{#if allDisabled}
						<p class="text-sm text-[#d0d8ff]/70 italic">No compatible context lengths (need more RAM).</p>
					{:else}
						<!-- LlamaBarn-style: "Xk ctx on Y.Y GB mem", one decimal; gray out if mem > system RAM -->
						<div class="flex flex-col gap-1.5">
							{#each optionsWithMem as { ctx, memGB }}
								{@const memStr = (typeof memGB === 'number' && !Number.isNaN(memGB)) ? memGB.toFixed(1) : '—'}
								{@const ctxLabel = ctx >= 1000 ? `${ctx / 1000}k` : String(ctx)}
								{@const disabled = systemRAMGB != null && memGB > systemRAMGB}
								{#if disabled}
									<Tooltip.Provider>
										<Tooltip.Root>
											<Tooltip.Trigger>
												<label
													class="flex items-center gap-2 cursor-not-allowed rounded px-2 py-1 opacity-60"
												>
													<input
														type="radio"
														name="ctx-{model.name}"
														value={ctx}
														checked={false}
														disabled
														class="context-radio h-4 w-4 border-[#444] bg-[#11243a]"
													/>
													<span class="text-sm text-[#d0d8ff]/90">
														{ctxLabel} ctx on <span class="font-semibold text-[#e0e0ff]">{memStr} GB mem</span>
													</span>
												</label>
											</Tooltip.Trigger>
											<Tooltip.Content>
												<p>Requires {memStr} GB+ RAM (system: {systemRAMGB} GB)</p>
											</Tooltip.Content>
										</Tooltip.Root>
									</Tooltip.Provider>
								{:else}
									<label
										class="flex items-center gap-2 cursor-pointer rounded px-2 py-1.5 hover:bg-[#11243a]/80 transition-colors"
									>
										<input
											type="radio"
											name="ctx-{model.name}"
											value={ctx}
											checked={selectedCtx === ctx}
											onchange={() => setContext(ctx)}
											class="context-radio h-4 w-4 border-[#444] bg-[#11243a] text-[#4cc9f0] focus:ring-2 focus:ring-[#4cc9f0]/30 focus:ring-offset-0"
										/>
										<span class="text-sm text-[#d0d8ff]/90">
											{ctxLabel} ctx on <span class="font-semibold text-[#e0e0ff]">{memStr} GB mem</span>
										</span>
									</label>
								{/if}
							{/each}
						</div>
					{/if}
				{:else}
					<p class="text-sm text-[#d0d8ff]/70 italic">Context length options unavailable for this model.</p>
				{/if}
			</div>
		</div>
	{/if}
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

	/* Dark-theme radios: visible border, cyan accent when selected (LlamaBarn-style adapted) */
	.context-length-section :global(input[type="radio"]) {
		accent-color: #4cc9f0;
	}
</style>

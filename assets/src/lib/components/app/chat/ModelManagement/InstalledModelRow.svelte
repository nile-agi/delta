<script lang="ts">
	import { Trash2, Copy, Loader2, ChevronDown } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
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
		onContextChange?: (modelName: string, ctx: number) => void;
		removing?: boolean;
		systemRAMGB?: number | null;
	}

	let { model, onRemove, onContextChange, removing = false, systemRAMGB = null }: Props = $props();

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

	/** Persisted context choice (localStorage); used when loading model from chat selector. */
	let selectedCtx = $state(8192);

	$effect(() => {
		const opts = contextOptions;
		if (opts.length === 0) return;
		const raw =
			typeof window !== 'undefined' ? localStorage.getItem(STORAGE_KEY_PREFIX + model.name) : null;
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

	const familyIcon = $derived(getFamilyIconForModelName(model.name));

	const modelTags = $derived.by(() => {
		const tags: string[] = [];
		const name = model.display_name || model.name;
		if (name.includes('∞') || (catalogModel?.context_size && catalogModel.context_size >= 128_000))
			tags.push('∞');
		if (model.quantization) tags.push('Q');
		if (name.toLowerCase().includes('thinking')) tags.push('○○');
		return tags;
	});

	function getContextSizeDisplay(): string {
		if (contextOptions.length > 0 && contextOptions.includes(selectedCtx)) {
			if (selectedCtx >= 1000000) return `${(selectedCtx / 1000000).toFixed(1)}M ctx`;
			if (selectedCtx >= 1000) return `${(selectedCtx / 1000).toFixed(0)}k ctx`;
			return `${selectedCtx} ctx`;
		}
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
		e.stopPropagation();
		navigator.clipboard.writeText(model.name);
	}

	function handleDelete(e: MouseEvent) {
		e.stopPropagation();
		onRemove?.(model.name);
	}

	/** Options with mem for dropdown: "Xk ctx on Y.Y GB mem" */
	const optionsWithMem = $derived(
		contextOptions.map((ctx) => ({
			ctx,
			memGB: estimateMemoryGB(fileSizeGB, ctx)
		}))
	);
</script>

<!-- Installed model row: no model loading here (use chat model selector). Context length in dropdown; click model field to open, click again or choose option to close. -->
<div
	class="installed-model-row group flex flex-col rounded-lg border border-[#1a2b44]/50 bg-[#11243a] transition-all duration-200 hover:border-[#4cc9f0]/20 hover:bg-[#1a2b44]"
>
	<div class="flex min-h-0 items-center gap-4 px-4 py-3">
		{#if contextOptions.length > 0}
			<!-- Whole model field (icon + info) toggles context dropdown: click to open, click again to close -->
			<DropdownMenu.Root>
				<DropdownMenu.Trigger
					class="flex min-w-0 flex-1 cursor-pointer items-center gap-4 rounded-md border-0 bg-transparent p-0 text-left outline-none focus:ring-0"
					aria-haspopup="listbox"
					aria-label="Model context length (click to change)"
				>
					<!-- Family Icon -->
					<div
						class="flex h-10 w-10 flex-shrink-0 items-center justify-center rounded-full bg-[#1a2b44] text-xl text-[#e0e0ff]"
					>
						{familyIcon}
					</div>
					<!-- Model Info: name, tags, size | quantization | ctx -->
					<div class="min-w-0 flex-1">
						<div class="mb-1 flex flex-wrap items-center gap-2">
							<h4 class="truncate text-sm font-semibold text-[#e0e0ff]">
								{model.display_name || model.name}
							</h4>
							{#each modelTags as tag (tag)}
								<span class="text-xs font-normal text-[#d0d8ff]/70" aria-hidden="true">{tag}</span>
							{/each}
						</div>
						<div class="flex flex-wrap items-center gap-2 text-xs text-[#d0d8ff]/70">
							{#if model.size_str}
								<span>{model.size_str}</span>
								<span class="text-[#d0d8ff]/40">|</span>
							{/if}
							{#if model.quantization}
								<span>{model.quantization}</span>
								<span class="text-[#d0d8ff]/40">|</span>
							{/if}
							<span class="font-medium text-[#4cc9f0]">{getContextSizeDisplay()}</span>
							<ChevronDown class="h-3.5 w-3.5 text-[#d0d8ff]/60" />
						</div>
					</div>
				</DropdownMenu.Trigger>
				<DropdownMenu.Content
					class="min-w-[12rem] border-[#1a2b44] bg-[#11243a] text-[#e0e0ff]"
					side="bottom"
					align="start"
				>
					{#each optionsWithMem as { ctx, memGB } (ctx)}
						{@const memStr =
							typeof memGB === 'number' && !Number.isNaN(memGB) ? memGB.toFixed(1) : '—'}
						{@const disabled = systemRAMGB != null && memGB > systemRAMGB}
						<DropdownMenu.Item
							class="cursor-pointer text-sm text-[#d0d8ff] focus:bg-[#1a2b44] focus:text-[#e0e0ff] data-[disabled]:pointer-events-none data-[disabled]:opacity-50"
							{disabled}
							onclick={() => {
								if (!disabled) setContext(ctx);
							}}
						>
							<span class={selectedCtx === ctx ? 'font-semibold text-[#4cc9f0]' : ''}>
								{ctx >= 1000 ? Math.round(ctx / 1000) : ctx}k ctx on {memStr} GB mem
							</span>
						</DropdownMenu.Item>
					{/each}
				</DropdownMenu.Content>
			</DropdownMenu.Root>
		{:else}
			<!-- No context options: show static model field (no dropdown) -->
			<div class="flex min-w-0 flex-1 items-center gap-4">
				<div
					class="flex h-10 w-10 flex-shrink-0 items-center justify-center rounded-full bg-[#1a2b44] text-xl text-[#e0e0ff]"
				>
					{familyIcon}
				</div>
				<div class="min-w-0 flex-1">
					<div class="mb-1 flex flex-wrap items-center gap-2">
						<h4 class="truncate text-sm font-semibold text-[#e0e0ff]">
							{model.display_name || model.name}
						</h4>
						{#each modelTags as tag (tag)}
							<span class="text-xs font-normal text-[#d0d8ff]/70" aria-hidden="true">{tag}</span>
						{/each}
					</div>
					<div class="flex flex-wrap items-center gap-2 text-xs text-[#d0d8ff]/70">
						{#if model.size_str}
							<span>{model.size_str}</span>
							<span class="text-[#d0d8ff]/40">|</span>
						{/if}
						{#if model.quantization}
							<span>{model.quantization}</span>
							<span class="text-[#d0d8ff]/40">|</span>
						{/if}
						<span>{getContextSizeDisplay()}</span>
					</div>
				</div>
			</div>
		{/if}

		<!-- Actions: copy, delete (outside dropdown trigger so they don't toggle context) -->
		<div class="flex flex-shrink-0 items-center gap-2">
			<Tooltip.Provider>
				<Tooltip.Root>
					<Tooltip.Trigger>
						<Button
							variant="ghost"
							size="sm"
							class="h-8 w-8 p-0 text-[#d0d8ff]/60 transition-colors hover:bg-[#1a2b44] hover:text-[#4cc9f0]"
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
								class="h-8 w-8 p-0 text-[#d0d8ff]/60 transition-colors hover:bg-red-500/10 hover:text-red-400"
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
</div>

<style>
	.installed-model-row:hover {
		box-shadow: 0 0 0 1px rgba(76, 201, 240, 0.2);
	}
</style>

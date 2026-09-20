<script lang="ts">
	import { onMount } from 'svelte';
	import { Button } from '$lib/components/ui/button';
	import { AlertTriangle, Check, X, Zap, Cpu, HardDrive, XCircle, Sparkles } from '@lucide/svelte';

	interface ModelEfficiency {
		model_name: string;
		display_name: string;
		size_bytes: number;
		size_gb: number;
		quantization: string;
		can_run: boolean;
		efficient: boolean;
		gpu_layers: number;
		cpu_layers: number;
		gpu_mem_needed: number;
		cpu_mem_needed: number;
		all_layers_on_gpu: boolean;
		warning: string;
		recommendation: string;
	}

	interface Recommendation {
		model_name: string;
		display_name: string;
		can_run: boolean;
		efficient: boolean;
		warning: string;
		recommendation: string;
	}

	interface ResourceHog {
		name: string;
		pid: number;
		cpu_pct: number;
		ram_gb: number;
		type: string;
		suggestion: string;
	}

	interface EfficiencyResponse {
		models: ModelEfficiency[];
		count: number;
	}

	interface HogsResponse {
		hogs: ResourceHog[];
		count: number;
	}

	let models = $state<ModelEfficiency[]>([]);
	let recommendation = $state<Recommendation | null>(null);
	let hogs = $state<ResourceHog[]>([]);
	let loading = $state(true);

	onMount(async () => {
		await loadAnalysis();
		// Refresh every 30 seconds
		setInterval(loadAnalysis, 30000);
	});

	async function loadAnalysis() {
		try {
			const [modelsRes, recRes, hogsRes] = await Promise.all([
				fetch('/api/v1/models/efficiency'),
				fetch('/api/v1/models/recommendation'),
				fetch('/api/v1/system/resource-hogs')
			]);

			if (modelsRes.ok) {
				const data = (await modelsRes.json()) as EfficiencyResponse;
				models = data.models || [];
			}

			if (recRes.ok) {
				recommendation = (await recRes.json()) as Recommendation;
			}

			if (hogsRes.ok) {
				const data = (await hogsRes.json()) as HogsResponse;
				hogs = data.hogs || [];
			}
		} catch (e) {
			console.error('Failed to load analysis:', e);
		} finally {
			loading = false;
		}
	}

	async function killProcess(pid: number) {
		if (!confirm('Terminate this process? This will close the application.')) return;

		try {
			const res = await fetch('/api/v1/system/kill-process', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ pid })
			});
			if (res.ok) {
				// Refresh after kill
				setTimeout(loadAnalysis, 2000);
			} else {
				alert('Failed to terminate process. You may need to close it manually.');
			}
		} catch (e) {
			alert('Failed to terminate process');
		}
	}

	function getStatusIcon(model: ModelEfficiency) {
		if (model.can_run && model.efficient) return 'success';
		if (model.can_run) return 'warning';
		return 'error';
	}
</script>

<div class="space-y-4">
	<!-- Top Recommendation -->
	{#if recommendation && recommendation.can_run}
		<div class="rounded-lg border border-green-500/30 bg-green-500/10 p-4">
			<div class="flex items-start gap-3">
				<Check class="h-5 w-5 text-green-500 shrink-0 mt-0.5" />
				<div class="flex-1">
					<h3 class="font-semibold text-green-700 dark:text-green-400 flex items-center gap-2">
						<Sparkles class="h-4 w-4" />
						Recommended: {recommendation.display_name || recommendation.model_name}
					</h3>
					<p class="text-sm text-muted-foreground mt-1">
						{recommendation.recommendation}
					</p>
				</div>
			</div>
		</div>
	{:else if recommendation && !recommendation.can_run}
		<div class="rounded-lg border border-amber-500/30 bg-amber-500/10 p-4">
			<div class="flex items-start gap-3">
				<AlertTriangle class="h-5 w-5 text-amber-500 shrink-0 mt-0.5" />
				<div class="flex-1">
					<h3 class="font-semibold text-amber-700 dark:text-amber-400">
						Resource Warning
					</h3>
					<p class="text-sm text-muted-foreground mt-1">
						{recommendation.recommendation}
					</p>
				</div>
			</div>
		</div>
	{/if}

	<!-- Resource Hogs -->
	{#if hogs.length > 0}
		<div class="rounded-lg border border-blue-500/30 bg-blue-500/10 p-4">
			<h3 class="font-semibold text-blue-700 dark:text-blue-400 mb-3 flex items-center gap-2">
				<Zap class="h-4 w-4" />
				Resource-Heavy Applications
			</h3>
			<p class="text-xs text-muted-foreground mb-3">
				Closing these applications will free up resources for better model performance.
			</p>
			<div class="space-y-2">
				{#each hogs as hog}
					<div class="flex items-center justify-between gap-3 text-sm bg-background/50 rounded-lg p-3 border border-border/50">
						<div class="flex-1 min-w-0">
							<div class="font-medium truncate">{hog.name}</div>
							<div class="text-xs text-muted-foreground flex items-center gap-3 mt-1">
								{#if hog.cpu_pct > 0}
									<span class="flex items-center gap-1">
										<Cpu class="h-3 w-3" />
										{hog.cpu_pct.toFixed(1)}% CPU
									</span>
								{/if}
								<span class="flex items-center gap-1">
									<HardDrive class="h-3 w-3" />
									{hog.ram_gb.toFixed(1)}GB RAM
								</span>
								<span class="capitalize px-1.5 py-0.5 rounded bg-muted text-xs">
									{hog.type}
								</span>
							</div>
							<p class="text-xs text-muted-foreground mt-1 italic">
								{hog.suggestion}
							</p>
						</div>
						<Button
							size="sm"
							variant="ghost"
							onclick={() => killProcess(hog.pid)}
							class="h-8 px-3 text-xs text-destructive hover:bg-destructive/10 shrink-0"
						>
							<XCircle class="h-3.5 w-3.5 mr-1" />
							Close
						</Button>
					</div>
				{/each}
			</div>
		</div>
	{/if}

	<!-- Model List -->
	<div class="rounded-lg border bg-card">
		<div class="border-b p-4">
			<h3 class="font-semibold flex items-center gap-2">
				<HardDrive class="h-4 w-4" />
				Model Compatibility
			</h3>
			<p class="text-xs text-muted-foreground mt-1">
				Models are analyzed against your current hardware state
			</p>
		</div>
		<div class="divide-y max-h-96 overflow-y-auto">
			{#if loading}
				<div class="p-8 text-center text-muted-foreground">
					<Zap class="h-8 w-8 mx-auto mb-2 animate-pulse" />
					Analyzing models against your hardware...
				</div>
			{:else if models.length === 0}
				<div class="p-8 text-center text-muted-foreground">
					<p>No installed models found.</p>
					<p class="text-xs mt-1">Download some models to see compatibility.</p>
				</div>
			{:else}
				{#each models as model}
					{@const status = getStatusIcon(model)}
					<div class="p-4 hover:bg-muted/50 transition-colors">
						<div class="flex items-start justify-between gap-3">
							<div class="flex-1 min-w-0">
								<div class="flex items-center gap-2">
									{#if status === 'success'}
										<Check class="h-4 w-4 text-green-500 shrink-0" />
									{:else if status === 'warning'}
										<AlertTriangle class="h-4 w-4 text-amber-500 shrink-0" />
									{:else}
										<X class="h-4 w-4 text-red-500 shrink-0" />
									{/if}
									<span class="font-medium truncate">
										{model.display_name}
									</span>
									{#if model.quantization}
										<span class="text-xs px-1.5 py-0.5 rounded bg-muted text-muted-foreground">
											{model.quantization}
										</span>
									{/if}
								</div>
								<div class="flex items-center gap-3 text-xs text-muted-foreground mt-1.5">
									<span>{model.size_gb.toFixed(1)}GB</span>
									{#if model.can_run}
										<span>
											{model.gpu_layers} GPU / {model.cpu_layers} CPU layers
										</span>
										{#if model.gpu_mem_needed > 0}
											<span>{model.gpu_mem_needed.toFixed(1)}GB GPU</span>
										{/if}
									{/if}
								</div>
								{#if model.warning}
									<div class="text-xs text-muted-foreground mt-1.5 italic">
										{model.warning}
									</div>
								{/if}
								{#if model.recommendation}
									<div class="text-xs mt-1 {status === 'success' ? 'text-green-600 dark:text-green-400' : 'text-muted-foreground'}">
										{model.recommendation}
									</div>
								{/if}
							</div>
							<div class="shrink-0">
								{#if !model.can_run}
									<span class="text-xs px-2 py-1 rounded-full bg-red-500/20 text-red-700 dark:text-red-400 border border-red-500/30">
										Cannot Run
									</span>
								{:else if model.efficient}
									<span class="text-xs px-2 py-1 rounded-full bg-green-500/20 text-green-700 dark:text-green-400 border border-green-500/30">
										Efficient
									</span>
								{:else}
									<span class="text-xs px-2 py-1 rounded-full bg-amber-500/20 text-amber-700 dark:text-amber-400 border border-amber-500/30">
										Slow
									</span>
								{/if}
							</div>
						</div>
					</div>
				{/each}
			{/if}
		</div>
	</div>
</div>
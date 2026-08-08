<script lang="ts">
	interface Props {
		progress: number;
		currentBytes: number;
		totalBytes: number;
		errorMessage?: string | null;
	}

	let { progress, currentBytes, totalBytes, errorMessage = null }: Props = $props();

	const clamped = $derived(Math.max(0, Math.min(100, progress)));
	const toMB = (bytes: number) => (bytes / (1024 * 1024)).toFixed(1);
</script>

<div class="space-y-1.5">
	<div class="flex items-center justify-between text-xs text-muted-foreground">
		<span class="font-medium">{progress.toFixed(1)}%</span>
		{#if totalBytes > 0}
			<span>{toMB(currentBytes)} MB / {toMB(totalBytes)} MB</span>
		{:else}
			<!-- The backend reports 0/0 until libcurl has the response headers. -->
			<span>Starting…</span>
		{/if}
	</div>
	<div class="h-2 w-full overflow-hidden rounded-full bg-muted">
		<div
			class="h-full rounded-full bg-primary transition-all duration-300 ease-out"
			style="width: {clamped}%;"
		></div>
	</div>
	{#if errorMessage}
		<p class="text-xs text-destructive">{errorMessage}</p>
	{/if}
</div>

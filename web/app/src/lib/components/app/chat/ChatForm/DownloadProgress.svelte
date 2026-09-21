<!-- web/app/src/lib/components/app/chat/ChatForm/DownloadProgress.svelte -->
<script lang="ts">
	import { X, Download } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { downloads } from '$lib/stores/downloads.svelte';
	import { modelsCatalog, findModelByName } from '$lib/data/models_catalog';

	function formatBytes(bytes: number): string {
		if (bytes === 0) return '0 B';
		const k = 1024;
		const sizes = ['B', 'KB', 'MB', 'GB'];
		const i = Math.floor(Math.log(bytes) / Math.log(k));
		return Number.isNaN(i) ? '0 B' : `${(bytes / Math.pow(k, i)).toFixed(1)} ${sizes[i]}`;
	}

	function handleCancel(model: string) {
		void downloads.cancel(model);
	}
</script>

{#if downloads.activeCount > 0}
	<div class="mx-auto mb-3 w-full max-w-[48rem] px-5">
		<div class="rounded-lg border border-border bg-muted/50 p-3">
			{#each downloads.active as download (download.model)}
				{@const modelInfo = findModelByName(download.model)}
				{@const displayName = modelInfo?.display_name || download.model}
				{@const progress = download.progress}
				{@const currentBytes = download.currentBytes}
				{@const totalBytes = download.totalBytes}

				<div class="mb-2 last:mb-0">
					<div class="mb-1 flex items-center justify-between">
						<div class="flex items-center gap-2">
							<Download class="h-4 w-4 text-primary" />
							<span class="text-sm font-medium">{displayName}</span>
						</div>
						<div class="flex items-center gap-2">
							<span class="text-xs text-muted-foreground">
								{progress.toFixed(1)}% · {formatBytes(currentBytes)} / {formatBytes(totalBytes)}
							</span>
							<Button
								variant="ghost"
								size="sm"
								class="h-6 w-6 p-0"
								onclick={() => handleCancel(download.model)}
								title="Cancel download"
							>
								<X class="h-3 w-3" />
							</Button>
						</div>
					</div>
					<div class="h-1.5 w-full overflow-hidden rounded-full bg-muted">
						<div
							class="h-full rounded-full bg-primary transition-all duration-300"
							style="width: {progress}%;"
						></div>
					</div>
				</div>
			{/each}
		</div>
	</div>
{/if}

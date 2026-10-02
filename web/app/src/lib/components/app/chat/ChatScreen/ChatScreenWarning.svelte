<script lang="ts">
	import { AlertTriangle, RefreshCw } from '@lucide/svelte';
	import { serverLoading, serverStore } from '$lib/stores/server.svelte';
	import { fly } from 'svelte/transition';

	interface Props {
		class?: string;
	}

	let { class: className = '' }: Props = $props();

	function handleRefreshServer() {
		serverStore.fetchServerProps();
	}
</script>

<div class="mb-3 {className}" in:fly={{ y: 10, duration: 250 }}>
	<div
		class="rounded-md border border-amber-300/60 bg-amber-50 px-3 py-2 dark:border-amber-400/25 dark:bg-amber-400/10"
	>
		<div class="flex items-center justify-between">
			<div class="flex items-center">
				<AlertTriangle class="h-4 w-4 text-amber-600 dark:text-amber-300" />
				<p class="ml-2 text-sm text-amber-900 dark:text-amber-100">
					Server `/props` endpoint not available - using cached data
				</p>
			</div>
			<button
				onclick={handleRefreshServer}
				disabled={serverLoading()}
				class="ml-3 flex items-center gap-1.5 rounded bg-amber-200/70 px-2 py-1 text-xs font-medium text-amber-900 hover:bg-amber-200 disabled:opacity-50 dark:bg-amber-400/15 dark:text-amber-100 dark:hover:bg-amber-400/25"
			>
				<RefreshCw class="h-3 w-3 {serverLoading() ? 'animate-spin' : ''}" />
				{serverLoading() ? 'Checking...' : 'Retry'}
			</button>
		</div>
	</div>
</div>

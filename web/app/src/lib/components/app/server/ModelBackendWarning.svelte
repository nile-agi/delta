<script lang="ts">
	import { AlertTriangle } from '@lucide/svelte';

	interface Props {
		error: string;
	}

	let { error }: Props = $props();
</script>

<div
	class="fixed top-3 left-1/2 z-[1100] flex w-[min(42rem,calc(100vw-2rem))] -translate-x-1/2 items-start gap-3 rounded-md border border-yellow-500/30 bg-yellow-50 px-4 py-3 text-sm text-yellow-950 shadow-lg dark:bg-yellow-950/90 dark:text-yellow-50"
	role="status"
>
	<AlertTriangle class="mt-0.5 h-4 w-4 shrink-0 text-yellow-600 dark:text-yellow-300" />
	<div class="min-w-0">
		<p class="font-medium">Model backend unavailable</p>
		<p class="mt-0.5 break-words text-yellow-900/80 dark:text-yellow-100/80">
			{error}. The app is ready for model management; fix the Linux graphics runtime or choose a
			model to retry the backend.
		</p>
	</div>
</div>

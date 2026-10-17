<script lang="ts">
	import { browser } from '$app/environment';
	import { untrack } from 'svelte';
	let { stage = 'model-load', message = '', model, exitCode, label = 'Review report' }: {
		stage?: string; message?: string; model?: string; exitCode?: number; label?: string;
	} = $props();
	$effect(() => {
		const context = { stage, message, model, exitCode };
		if (browser && message && stage !== 'previous') {
			untrack(() => void (window as any).__DELTA_DIAGNOSTICS__?.record(context));
		}
	});
	function review() {
		const reports = (window as any).__DELTA_DIAGNOSTICS__;
		if (reports) reports.review({ stage, message, model, exitCode });
		else window.open('https://github.com/nile-agi/delta-release/issues/new', '_blank', 'noopener,noreferrer');
	}
</script>

<button type="button" class="rounded-md border border-border px-3 py-1.5 text-xs text-foreground hover:bg-accent" onclick={review}>{label}</button>

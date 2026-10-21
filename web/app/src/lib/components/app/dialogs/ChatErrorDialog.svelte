<script lang="ts">
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import { AlertTriangle, TimerOff } from '@lucide/svelte';

	interface Props {
		open: boolean;
		type: 'timeout' | 'server';
		message: string;
		onOpenChange?: (open: boolean) => void;
	}

	let { open = $bindable(), type, message, onOpenChange }: Props = $props();

	const isTimeout = $derived(type === 'timeout');
	const title = $derived(isTimeout ? 'TCP Timeout' : 'Server Error');
	const description = $derived(
		isTimeout
			? 'The request did not receive a response from the server before timing out.'
			: 'The server responded with an error message. Review the details below.'
	);
	// The theme's error colour for both kinds, so errors match the rest of the app in light and dark.
	const iconClass = 'text-destructive';
	const badgeClass = 'border-destructive/40 bg-destructive/10 text-destructive';

	function handleOpenChange(newOpen: boolean) {
		open = newOpen;
		onOpenChange?.(newOpen);
	}
</script>

<AlertDialog.Root {open} onOpenChange={handleOpenChange}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title class="flex items-center gap-2">
				{#if isTimeout}
					<TimerOff class={`h-5 w-5 ${iconClass}`} />
				{:else}
					<AlertTriangle class={`h-5 w-5 ${iconClass}`} />
				{/if}

				{title}
			</AlertDialog.Title>

			<AlertDialog.Description>
				{description}
			</AlertDialog.Description>
		</AlertDialog.Header>

		<div class={`rounded-lg border px-4 py-3 text-sm ${badgeClass}`}>
			<p class="font-medium">{message}</p>
		</div>

		<AlertDialog.Footer>
			<AlertDialog.Action onclick={() => handleOpenChange(false)}>Close</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>

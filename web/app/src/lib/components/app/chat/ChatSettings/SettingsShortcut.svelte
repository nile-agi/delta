<script lang="ts">
	import { goto } from '$app/navigation';
	import Settings from '@lucide/svelte/icons/settings';
	import { Button } from '$lib/components/ui/button';
	import { useSidebar } from '$lib/components/ui/sidebar/context.svelte';

	const sidebar = useSidebar();
</script>

{#if !(sidebar.isMobile ? sidebar.openMobile : sidebar.open)}
	<Button
		variant="ghost"
		size="icon"
		class="fixed bottom-[max(1rem,env(safe-area-inset-bottom))] left-4 z-40 size-10 rounded-lg bg-background text-muted-foreground hover:text-foreground"
		aria-label="Open Settings"
		title="Settings"
		onclick={() => goto('?#/settings')}
	>
		<Settings class="size-5" />
	</Button>
{/if}

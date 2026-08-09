<script lang="ts">
	import { page } from '$app/state';
	import { goto } from '$app/navigation';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu/index.js';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Calendar, StickyNote, ChevronDown, Wrench } from 'lucide-svelte';

	let open = $state(false);

	const tools = [
		{ label: 'Calendar', href: '/calendar', icon: Calendar },
		{ label: 'Notes', href: '/notes', icon: StickyNote }
	];

	let currentTool = $derived(
		tools.find(t => page.route.id?.startsWith(t.href)) ?? null
	);
</script>

<DropdownMenu.Root bind:open>
	<DropdownMenu.Trigger asChild let:builder>
		<Button
			builders={[builder]}
			variant="ghost"
			class="w-full justify-between gap-2 text-sidebar-foreground hover:bg-sidebar-accent hover:text-sidebar-accent-foreground"
		>
			<span class="flex items-center gap-2">
				<Wrench class="h-4 w-4" />
				<span>Tools</span>
			</span>
			<ChevronDown class="h-3 w-3 transition-transform {open ? 'rotate-180' : ''}" />
		</Button>
	</DropdownMenu.Trigger>
	<DropdownMenu.Content
		side="right"
		align="start"
		class="w-48"
	>
		<DropdownMenu.Group>
			{#each tools as tool}
				<DropdownMenu.Item
					class="flex items-center gap-2 cursor-pointer {currentTool?.href === tool.href ? 'bg-accent' : ''}"
					on:click={() => goto(tool.href)}
				>
					<tool.icon class="h-4 w-4" />
					<span>{tool.label}</span>
				</DropdownMenu.Item>
			{/each}
		</DropdownMenu.Group>
	</DropdownMenu.Content>
</DropdownMenu.Root>
<script lang="ts">
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu/index.js';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Calendar, StickyNote, ChevronDown, Wrench } from '@lucide/svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { Cpu } from '@lucide/svelte';
	import { monitorWindow } from '$lib/stores/monitor-window.svelte';


	let open = $state(false);

	const tools = [
		{ label: 'Calendar', icon: Calendar, open: () => calendarWindow.open() },
		{ label: 'Notes', icon: StickyNote, open: () => notesWindow.open() },
		{ label: 'System Monitor', icon: Cpu, open: () => monitorWindow.open() }
	];
</script>

<DropdownMenu.Root bind:open>
	<DropdownMenu.Trigger>
		{#snippet child({ props })}
			<Button
				{...props}
				variant="ghost"
				class="w-full justify-between gap-2 text-sidebar-foreground hover:bg-sidebar-accent hover:text-sidebar-accent-foreground"
			>
				<span class="flex items-center gap-2">
					<Wrench class="h-4 w-4" />
					<span>Tools</span>
				</span>
				<ChevronDown class="h-3 w-3 transition-transform {open ? 'rotate-180' : ''}" />
			</Button>
		{/snippet}
	</DropdownMenu.Trigger>

	<DropdownMenu.Content side="right" align="start" class="w-48">
		<DropdownMenu.Group>
			{#each tools as tool}
				{@const Icon = tool.icon}
				<DropdownMenu.Item
					class="flex items-center gap-2 cursor-pointer"
					onclick={tool.open}
				>
					<Icon class="h-4 w-4" />
					<span>{tool.label}</span>
				</DropdownMenu.Item>
			{/each}
		</DropdownMenu.Group>
	</DropdownMenu.Content>
</DropdownMenu.Root>
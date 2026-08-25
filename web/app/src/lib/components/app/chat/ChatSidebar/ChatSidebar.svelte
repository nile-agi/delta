<script lang="ts">
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { Settings, Calendar, Trash2, StickyNote, ChevronDown, Wrench, Activity } from '@lucide/svelte';
	import { hardwareWindow } from '$lib/stores/hardware-window.svelte';
    import { dockStore } from '$lib/stores/dock.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { config } from '$lib/stores/settings.svelte';
	import { settingsWindow } from '$lib/stores/settings-window.svelte';
	import { ChatSidebarConversationItem, ConfirmationDialog } from '$lib/components/app';
	import ScrollArea from '$lib/components/ui/scroll-area/scroll-area.svelte';
	import * as Sidebar from '$lib/components/ui/sidebar';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import Input from '$lib/components/ui/input/input.svelte';
	import {
		conversations,
		deleteConversation,
		updateConversationName
	} from '$lib/stores/chat.svelte';
	import ChatSidebarActions from './ChatSidebarActions.svelte';

	const sidebar = Sidebar.useSidebar();

	let currentChatId = $derived(page.params.id);
	let isSearchModeActive = $state(false);
	let searchQuery = $state('');
	let showDeleteDialog = $state(false);
	let showEditDialog = $state(false);
	let selectedConversation = $state<{ id: string; name: string } | null>(null);
	let editedName = $state('');

	let filteredConversations = $derived.by(() => {
		if (searchQuery.trim().length > 0) {
			return conversations().filter((conversation: { name: string }) =>
				conversation.name.toLowerCase().includes(searchQuery.toLowerCase())
			);
		}
		return conversations();
	});

	async function handleDeleteConversation(id: string) {
		const conversation = conversations().find((conv) => conv.id === id);
		if (conversation) {
			selectedConversation = conversation;
			showDeleteDialog = true;
		}
	}

	async function handleEditConversation(id: string) {
		const conversation = conversations().find((conv) => conv.id === id);
		if (conversation) {
			selectedConversation = conversation;
			editedName = conversation.name;
			showEditDialog = true;
		}
	}

	function handleConfirmDelete() {
		if (selectedConversation) {
			const id = selectedConversation.id;
			showDeleteDialog = false;
			selectedConversation = null;
			setTimeout(() => {
				deleteConversation(id);
			}, 100);
		}
	}

	function handleConfirmEdit() {
		if (!editedName.trim() || !selectedConversation) return;
		showEditDialog = false;
		updateConversationName(selectedConversation.id, editedName);
		selectedConversation = null;
	}

	export function handleMobileSidebarItemClick() {
		if (sidebar.isMobile) {
			sidebar.toggle();
		}
	}

	export function activateSearchMode() {
		isSearchModeActive = true;
	}

	export function editActiveConversation() {
		if (currentChatId) {
			const activeConversation = conversations().find((conv) => conv.id === currentChatId);
			if (activeConversation) {
				const event = new CustomEvent('edit-active-conversation', {
					detail: { conversationId: currentChatId }
				});
				document.dispatchEvent(event);
			}
		}
	}

	async function selectConversation(id: string) {
		if (isSearchModeActive) {
			isSearchModeActive = false;
			searchQuery = '';
		}
		await goto(`#/chat/${id}`);
	}

	function openHardwareDashboard() {
        // Register the window in the dock store
        dockStore.register('hardware-telemetry', 'Hardware Telemetry');
    }

	let toolsOpen = $state(false);
</script>

<div class="relative h-full overflow-hidden">
	<ScrollArea class="h-full">
		<div class="pb-16">
			<Sidebar.Header class="top-0 z-10 gap-6 bg-sidebar/50 px-4 pt-4 pb-2 backdrop-blur-lg md:sticky">
				<a href="#/" onclick={handleMobileSidebarItemClick} aria-label="Go to home">
					<h1 class="inline-flex items-center gap-1 px-2 text-xl font-semibold">Delta</h1>
				</a>
				<ChatSidebarActions {handleMobileSidebarItemClick} bind:isSearchModeActive bind:searchQuery />
			</Sidebar.Header>

			<!-- TOOLS DROPDOWN -->
			<Sidebar.Group class="mt-2 space-y-1 p-0 px-4">
				<Sidebar.GroupLabel>Tools</Sidebar.GroupLabel>
				<Sidebar.GroupContent>
					<DropdownMenu.Root bind:open={toolsOpen}>
						<DropdownMenu.Trigger class="w-full">
							<div
								class="flex w-full items-center justify-between gap-2 rounded-md px-2 py-1.5 text-sm text-sidebar-foreground transition-colors hover:bg-sidebar-accent hover:text-sidebar-accent-foreground cursor-pointer"
							>
								<span class="flex items-center gap-2">
									<Wrench class="h-4 w-4" />
									<span>Tools</span>
								</span>
								<ChevronDown class="h-3 w-3 transition-transform {toolsOpen ? 'rotate-180' : ''}" />
							</div>
						</DropdownMenu.Trigger>
						<DropdownMenu.Content side="right" align="start" class="w-48">
							<DropdownMenu.Group>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={() => { calendarWindow.open(); handleMobileSidebarItemClick(); }}
								>
									<Calendar class="h-4 w-4" />
									<span>Calendar</span>
								</DropdownMenu.Item>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={() => { notesWindow.open(); handleMobileSidebarItemClick(); }}
								>
									<StickyNote class="h-4 w-4" />
									<span>Notes</span>
								</DropdownMenu.Item>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={() => { openHardwareDashboard(); handleMobileSidebarItemClick(); }}
								>
									<Activity class="h-4 w-4" />
									<span>Hardware</span>
								</DropdownMenu.Item>
							</DropdownMenu.Group>
						</DropdownMenu.Content>
					</DropdownMenu.Root>
				</Sidebar.GroupContent>
			</Sidebar.Group>

			<Sidebar.Group class="mt-4 space-y-2 p-0 px-4">
				{#if (filteredConversations.length > 0 && isSearchModeActive) || !isSearchModeActive}
					<Sidebar.GroupLabel>
						{isSearchModeActive ? 'Search results' : 'Conversations'}
					</Sidebar.GroupLabel>
				{/if}
				<Sidebar.GroupContent>
					<Sidebar.Menu>
						{#each filteredConversations as conversation (conversation.id)}
							<Sidebar.MenuItem class="mb-1">
								<ChatSidebarConversationItem
									conversation={{
										id: conversation.id,
										name: conversation.name,
										lastModified: conversation.lastModified,
										currNode: conversation.currNode
									}}
									{handleMobileSidebarItemClick}
									isActive={currentChatId === conversation.id}
									onSelect={selectConversation}
									onEdit={handleEditConversation}
									onDelete={handleDeleteConversation}
								/>
							</Sidebar.MenuItem>
						{/each}
						{#if filteredConversations.length === 0}
							<div class="px-2 py-4 text-center">
								<p class="mb-4 p-4 text-sm text-muted-foreground">
									{searchQuery.length > 0
										? 'No results found'
										: isSearchModeActive
											? 'Start typing to see results'
											: 'No conversations yet'}
								</p>
							</div>
						{/if}
					</Sidebar.Menu>
				</Sidebar.GroupContent>
			</Sidebar.Group>
		</div>
	</ScrollArea>

	<div class="absolute bottom-0 left-0 right-0 z-50 flex items-center justify-between border-t border-border/30 bg-sidebar/80 px-4 py-3 backdrop-blur-lg">
		<span class="text-sm font-medium text-muted-foreground truncate">
			{config().userName || 'User'}
		</span>
		<button
			class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
			onclick={() => settingsWindow.toggle()}
			title="Settings"
			aria-label="Settings"
		>
			<Settings class="h-4 w-4" />
		</button>
	</div>
</div>

<ConfirmationDialog
	bind:open={showDeleteDialog}
	title="Delete Conversation"
	description={selectedConversation
		? `Are you sure you want to delete "${selectedConversation.name}"? This action cannot be undone and will permanently remove all messages in this conversation.`
		: ''}
	confirmText="Delete"
	cancelText="Cancel"
	variant="destructive"
	icon={Trash2}
	onConfirm={handleConfirmDelete}
	onCancel={() => {
		showDeleteDialog = false;
		selectedConversation = null;
	}}
/>

<AlertDialog.Root bind:open={showEditDialog}>
	<AlertDialog.Content>
		<AlertDialog.Header>
			<AlertDialog.Title>Edit Conversation Name</AlertDialog.Title>
			<AlertDialog.Description>Enter a new name for this conversation.</AlertDialog.Description>
		</AlertDialog.Header>
		<Input
			class="mt-2 text-foreground"
			onkeydown={(e) => {
				if (e.key === 'Enter') {
					e.preventDefault();
					handleConfirmEdit();
				}
			}}
			placeholder="Enter a new name"
			type="text"
			bind:value={editedName}
		/>
		<AlertDialog.Footer>
			<AlertDialog.Cancel
				onclick={() => {
					showEditDialog = false;
					selectedConversation = null;
				}}>Cancel</AlertDialog.Cancel
			>
			<AlertDialog.Action onclick={handleConfirmEdit}>Save</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>
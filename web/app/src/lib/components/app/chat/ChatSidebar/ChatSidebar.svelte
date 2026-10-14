<script lang="ts">
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import {
		Settings,
		Calendar,
		Trash2,
		StickyNote,
		ChevronDown,
		Wrench,
		Activity,
		PanelLeftClose,
		PanelLeftOpen,
		MessageSquarePlus
	} from '@lucide/svelte';
	import { hardwareWindow } from '$lib/stores/hardware-window.svelte';
	import { dockStore } from '$lib/stores/dock.svelte';
	import { config } from '$lib/stores/settings.svelte';
	import { ChatSidebarConversationItem, ConfirmationDialog } from '$lib/components/app';
	import ScrollArea from '$lib/components/ui/scroll-area/scroll-area.svelte';
	import * as Sidebar from '$lib/components/ui/sidebar';
	import * as AlertDialog from '$lib/components/ui/alert-dialog';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import Input from '$lib/components/ui/input/input.svelte';
	import { cn } from '$lib/components/ui/utils';
	import {
		conversations,
		deleteConversation,
		updateConversationName
	} from '$lib/stores/chat.svelte';
	import ChatSidebarActions from './ChatSidebarActions.svelte';
	import { browser } from '$app/environment';
	import { openHardwareWindow } from '$lib/services/hardware-window';
	import { openCalendarWindow } from '$lib/services/calendar-window';
	import { openNotesWindow } from '$lib/services/notes-window';
	import { fade } from 'svelte/transition';

	function openCalendar() {
		void openCalendarWindow();
		handleMobileSidebarItemClick();
	}

	function openNotes() {
		void openNotesWindow();
		handleMobileSidebarItemClick();
	}

	function openHardwareDashboard() {
		const isTauri = browser && '__TAURI_INTERNALS__' in window;
		if (isTauri) {
			void openHardwareWindow();
		} else {
			hardwareWindow.open();
			dockStore.register('hardware-telemetry', 'Hardware Telemetry');
		}
		handleMobileSidebarItemClick?.();
	}

	const sidebar = Sidebar.useSidebar();
	let currentChatId = $derived(page.params.id);
	let isSearchModeActive = $state(false);
	let searchQuery = $state('');
	let showDeleteDialog = $state(false);
	let showEditDialog = $state(false);
	let selectedConversation = $state<{ id: string; name: string } | null>(null);
	let editedName = $state('');
	let toolsOpen = $state(false);

	let isCollapsed = $derived(!sidebar.isMobile && sidebar.state === 'collapsed');

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
</script>

<div class="relative h-full overflow-hidden md:rounded-lg">
	<ScrollArea class="h-full">
		<div class="pb-16">

			<!-- ========================================== -->
			<!-- HEADER                                     -->
			<!-- ========================================== -->
			<Sidebar.Header
				class={cn(
					'top-0 z-10 bg-sidebar/50 backdrop-blur-lg md:sticky transition-all duration-300 ease-in-out border-none',
					isCollapsed ? '!px-0 !py-4' : 'gap-4 px-4 pt-4 pb-3'
				)}
			>
				{#if isCollapsed}
					<div
						class="flex w-full flex-col items-center gap-2"
						in:fade={{ duration: 200 }}
						out:fade={{ duration: 150 }}
					>
						<!-- Expand Icon -->
						<!-- Expand Icon -->
						<button
							onclick={() => sidebar.toggle()}
							class="group relative flex h-8 w-8 shrink-0 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
							aria-label="Expand sidebar"
							title="Expand Sidebar"
						>
							<!-- Favicon (visible by default, fades out on hover) -->
							<img 
								src="/favicon.svg" 
								alt="Delta" 
								class="absolute inset-0 m-auto h-4.5 w-4.5 object-contain transition-opacity duration-200 group-hover:opacity-0 pointer-events-none" 
							/>
							<!-- Expand Icon (hidden by default, fades in on hover) -->
							<PanelLeftOpen class="h-4 w-4 opacity-0 transition-opacity duration-200 group-hover:opacity-100 pointer-events-none" />
						</button>

						<a
							href="#/"
							onclick={handleMobileSidebarItemClick}
							class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
							aria-label="New Chat"
							title="New Chat"
						>
							<MessageSquarePlus class="h-4 w-4" />
						</a>
					</div>
				{:else}
					<div
						class="flex items-center justify-between"
						in:fade={{ duration: 200 }}
						out:fade={{ duration: 150 }}
					>
						<a
							href="#/"
							onclick={handleMobileSidebarItemClick}
							aria-label="Go to home"
							class="flex items-center"
						>
							<!-- <h1 class="text-lg font-semibold tracking-tight">Delta</h1> -->
							<img 
								src="/favicon.svg" 
								alt="Delta" 
								class="h-4.5 w-4.5 object-contain" 
							/>
						</a>

						<button
							onclick={() => sidebar.toggle()}
							class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
							aria-label="Collapse sidebar"
							title="Collapse Sidebar"
						>
							<PanelLeftClose class="h-4 w-4" />
						</button>
					</div>

					<ChatSidebarActions
						{handleMobileSidebarItemClick}
						bind:isSearchModeActive
						bind:searchQuery
					/>
				{/if}
			</Sidebar.Header>

			<!-- ========================================== -->
			<!-- TOOLS                                      -->
			<!-- ========================================== -->
			<div
				class={cn(
					'transition-all duration-300 ease-in-out',
					isCollapsed ? 'mt-2 !px-0' : 'mt-3 space-y-1 p-0 px-4'
				)}
			>
				{#if isCollapsed}
					<div class="flex w-full justify-center">
						<DropdownMenu.Root bind:open={toolsOpen}>
							<!-- <DropdownMenu.Trigger asChild>
								<button
									class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
									aria-label="Tools"
									title="Tools"
								>
									<Wrench class="h-4 w-4" />
								</button>
							</DropdownMenu.Trigger> -->
							<DropdownMenu.Trigger>
								{#snippet child({ props })}
									<button
										{...props}
										class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
										aria-label="Tools"
										title="Tools"
									>
										<Wrench class="h-4 w-4" />
									</button>
								{/snippet}
							</DropdownMenu.Trigger>
							<DropdownMenu.Content side="right" align="start" class="w-48">
								<DropdownMenu.Group>
									<DropdownMenu.Item
										class="flex items-center gap-2 cursor-pointer"
										onclick={openCalendar}
									>
										<Calendar class="h-4 w-4" />
										<span>Calendar</span>
									</DropdownMenu.Item>
									<DropdownMenu.Item
										class="flex items-center gap-2 cursor-pointer"
										onclick={openNotes}
									>
										<StickyNote class="h-4 w-4" />
										<span>Notes</span>
									</DropdownMenu.Item>
									<DropdownMenu.Item
										class="flex items-center gap-2 cursor-pointer"
										onclick={() => {
											openHardwareDashboard();
											handleMobileSidebarItemClick();
										}}
									>
										<Activity class="h-4 w-4" />
										<span>Hardware</span>
									</DropdownMenu.Item>
								</DropdownMenu.Group>
							</DropdownMenu.Content>
						</DropdownMenu.Root>
					</div>
				{:else}
					<Sidebar.GroupLabel class="text-xs font-medium text-muted-foreground/70">
						Tools
					</Sidebar.GroupLabel>
					<DropdownMenu.Root bind:open={toolsOpen}>
						<!-- <DropdownMenu.Trigger asChild>
							<button
								class="flex w-full items-center justify-between gap-2 rounded-md px-2 py-1.5 text-sm text-sidebar-foreground transition-all duration-200 hover:bg-sidebar-accent hover:text-sidebar-accent-foreground"
								aria-label="Tools"
							>
								<span class="flex items-center gap-2">
									<Wrench class="h-4 w-4" />
									<span>Tools</span>
								</span>
								<ChevronDown
									class="h-3 w-3 transition-transform duration-200 {toolsOpen
										? 'rotate-180'
										: ''}"
								/>
							</button>
						</DropdownMenu.Trigger> -->
						<DropdownMenu.Trigger>
							{#snippet child({ props })}
								<button
									{...props}
									class="flex w-full items-center justify-between gap-2 rounded-md px-2 py-1.5 text-sm text-sidebar-foreground transition-all duration-200 hover:bg-sidebar-accent hover:text-sidebar-accent-foreground"
									aria-label="Tools"
								>
									<span class="flex items-center gap-2">
										<Wrench class="h-4 w-4" />
										<span>Tools</span>
									</span>
									<ChevronDown
										class="h-3 w-3 transition-transform duration-200 {toolsOpen
											? 'rotate-180'
											: ''}"
									/>
								</button>
							{/snippet}
						</DropdownMenu.Trigger>
						<DropdownMenu.Content side="right" align="start" class="w-48">
							<DropdownMenu.Group>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={openCalendar}
								>
									<Calendar class="h-4 w-4" />
									<span>Calendar</span>
								</DropdownMenu.Item>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={openNotes}
								>
									<StickyNote class="h-4 w-4" />
									<span>Notes</span>
								</DropdownMenu.Item>
								<DropdownMenu.Item
									class="flex items-center gap-2 cursor-pointer"
									onclick={() => {
										openHardwareDashboard();
										handleMobileSidebarItemClick();
									}}
								>
									<Activity class="h-4 w-4" />
									<span>Hardware</span>
								</DropdownMenu.Item>
							</DropdownMenu.Group>
						</DropdownMenu.Content>
					</DropdownMenu.Root>
				{/if}
			</div>

			<!-- ========================================== -->
			<!-- CONVERSATIONS                              -->
			<!-- ========================================== -->
			{#if !isCollapsed}
				<div in:fade={{ duration: 250, delay: 50 }} out:fade={{ duration: 150 }}>
					<Sidebar.Group class="mt-3 space-y-2 p-0 px-4">
						{#if (filteredConversations.length > 0 && isSearchModeActive) || !isSearchModeActive}
							<Sidebar.GroupLabel class="text-xs font-medium text-muted-foreground/70">
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
			{/if}
		</div>
	</ScrollArea>

	<!-- ========================================== -->
	<!-- FOOTER                                     -->
	<!-- ========================================== -->
	<div
		class={cn(
			'absolute bottom-0 left-0 right-0 z-50 flex items-center transition-all duration-300 ease-in-out',
			isCollapsed
				? '!px-0 !py-4 bg-sidebar/50 backdrop-blur-lg justify-center'
				: 'justify-between px-4 py-3 border-t border-border/30 bg-sidebar/80 backdrop-blur-lg'
		)}
	>
		{#if isCollapsed}
			<button
				class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
				onclick={() => goto('?#/settings')}
				title="Settings"
				aria-label="Settings"
			>
				<Settings class="h-4 w-4" />
			</button>
		{:else}
			<span class="text-sm font-medium text-muted-foreground truncate" in:fade={{ duration: 200 }}>
				{config().userName || 'User'}
			</span>

			<button
				class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-all duration-200 hover:bg-accent hover:text-foreground hover:scale-105 active:scale-95"
				onclick={() => goto('?#/settings')}
				title="Settings"
				aria-label="Settings"
			>
				<Settings class="h-4 w-4" />
			</button>
		{/if}
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
				}}
			>
				Cancel
			</AlertDialog.Cancel>

			<AlertDialog.Action onclick={handleConfirmEdit}>Save</AlertDialog.Action>
		</AlertDialog.Footer>
	</AlertDialog.Content>
</AlertDialog.Root>
<script module lang="ts">
	import { defineMeta } from '@storybook/addon-svelte-csf';
	import ChatSidebar from '$lib/components/app/chat/ChatSidebar/ChatSidebar.svelte';
	import { Provider as SidebarProvider } from '$lib/components/ui/sidebar';
	import { expect, waitFor } from 'storybook/test';

	const { Story } = defineMeta({
		title: 'Components/ChatSidebar',
		component: ChatSidebar,
		parameters: {
			layout: 'centered'
		}
	});

	// Mock conversations for the sidebar
	const mockConversations: DatabaseConversation[] = [
		{
			id: 'conv-1',
			name: 'Getting Started with AI',
			lastModified: Date.now() - 1000 * 60 * 5, // 5 minutes ago
			currNode: 'msg-1'
		},
		{
			id: 'conv-2',
			name: 'Python Programming Help',
			lastModified: Date.now() - 1000 * 60 * 60 * 2, // 2 hours ago
			currNode: 'msg-2'
		},
		{
			id: 'conv-3',
			name: 'Creative Writing Ideas',
			lastModified: Date.now() - 1000 * 60 * 60 * 24, // 1 day ago
			currNode: 'msg-3'
		},
		{
			id: 'conv-4',
			name: 'This is a very long conversation title that should be truncated properly when displayed',
			lastModified: Date.now() - 1000 * 60 * 60 * 24 * 3, // 3 days ago
			currNode: 'msg-4'
		},
		{
			id: 'conv-5',
			name: 'Math Problem Solving',
			lastModified: Date.now() - 1000 * 60 * 60 * 24 * 7, // 1 week ago
			currNode: 'msg-5'
		}
	];
</script>

<Story
	asChild
	name="Default"
	play={async ({ canvas }) => {
		const { chatStore } = await import('$lib/stores/chat.svelte');
		chatStore.conversations = mockConversations;
		await expect(await canvas.findByText('Getting Started with AI')).toBeVisible();
	}}
>
	<div class="flex-column h-full h-screen w-72 bg-background">
		<SidebarProvider>
			<ChatSidebar />
		</SidebarProvider>
	</div>
</Story>

<Story
	asChild
	name="SearchActive"
	play={async ({ canvas, userEvent }) => {
		const { chatStore } = await import('$lib/stores/chat.svelte');
		chatStore.conversations = mockConversations;
		await expect(await canvas.findByText('Getting Started with AI')).toBeVisible();
		await userEvent.click(await canvas.findByRole('button', { name: /Search conversations/ }));
		const searchInput = await canvas.findByPlaceholderText('Search conversations...');
		await expect(searchInput).toHaveFocus();
		await userEvent.type(searchInput, 'Python');
		await expect(await canvas.findByText('Python Programming Help')).toBeVisible();
		await waitFor(() =>
			expect(canvas.queryByText('Getting Started with AI')).not.toBeInTheDocument()
		);
	}}
>
	<div class="flex-column h-full h-screen w-72 bg-background">
		<SidebarProvider>
			<ChatSidebar />
		</SidebarProvider>
	</div>
</Story>

<Story
	asChild
	name="Empty"
	play={async ({ canvas }) => {
		// Mock empty conversations store
		const { chatStore } = await import('$lib/stores/chat.svelte');
		chatStore.conversations = [];
		await expect(await canvas.findByText('No conversations yet')).toBeVisible();
	}}
>
	<div class="flex-column h-full h-screen w-72 bg-background">
		<SidebarProvider>
			<ChatSidebar />
		</SidebarProvider>
	</div>
</Story>

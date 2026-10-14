<script lang="ts">
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import SettingsPage from '$lib/components/app/chat/ChatSettings/SettingsPage.svelte';
	import { activeConversation } from '$lib/stores/chat.svelte';
	import { settingsReturnPath } from '$lib/utils/settings-return';

	const section = $derived(page.url.searchParams.get('section') ?? 'General');
</script>

<svelte:head>
	<title>Settings - Delta</title>
</svelte:head>

<SettingsPage
	{section}
	backLabel={activeConversation() ? 'Back to chat' : 'Back to Home'}
	onBack={() => goto(settingsReturnPath(activeConversation()?.id))}
/>

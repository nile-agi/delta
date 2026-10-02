<script lang="ts">
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import SettingsPage from '$lib/components/app/chat/ChatSettings/SettingsPage.svelte';

	const section = $derived(page.url.searchParams.get('section') ?? 'General');
</script>

<svelte:head>
	<title>Settings - Delta</title>
</svelte:head>

<SettingsPage {section} onBack={() => goto('?#/')} />

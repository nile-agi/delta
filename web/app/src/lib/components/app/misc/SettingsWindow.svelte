<script lang="ts">
	import ChatSettingsDialog from '../chat/ChatSettings/ChatSettingsDialog.svelte';
	import { settingsWindow } from '$lib/stores/settings-window.svelte';
	import { browser } from '$app/environment';

	const IS_TAURI_ENV = browser && typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

	if (browser && IS_TAURI_ENV) {
		import('@tauri-apps/api/window')
			.then(({ getCurrentWindow }) => {
				const label = getCurrentWindow().label;
				if (label === 'settings') {
					settingsWindow.state.open = true;
					settingsWindow.state.minimized = false;
				}
			})
			.catch(() => {});
	}
</script>

<ChatSettingsDialog fullscreen={true} />

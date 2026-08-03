<script lang="ts">
	import '../app.css';
	import { browser } from '$app/environment';
	import { page } from '$app/state';
	import { ChatSidebar, ConversationTitleUpdateDialog } from '$lib/components/app';
	import {
		activeMessages,
		isLoading,
		setTitleUpdateConfirmationCallback
	} from '$lib/stores/chat.svelte';
	import * as Sidebar from '$lib/components/ui/sidebar/index.js';
	import { serverStore } from '$lib/stores/server.svelte';
	import { config, settingsStore } from '$lib/stores/settings.svelte';
	import { resolveModelApiBaseUrl, resetModelApiResolution } from '$lib/utils/model-api-url';
	import { getServerBaseUrl } from '$lib/utils/server-base-url';
	import { ServerErrorSplash } from '$lib/components/app';
	import { ModeWatcher } from 'mode-watcher';
	import { Toaster } from 'svelte-sonner';
	import NotificationCenter from '$lib/components/app/notifications/NotificationCenter.svelte';
	import OnboardingDialog from '$lib/components/app/onboarding/OnboardingDialog.svelte';
	import { goto } from '$app/navigation';
	import { onDestroy } from 'svelte';
	import { startReminderPolling, stopReminderPolling } from '$lib/services/reminders';

	let { children } = $props();

	const IS_TAURI_ENV =
		browser && typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
	let serverReady = $state(!IS_TAURI_ENV);
	let serverError = $state(false);
	let serverErrorMessage = $state('');

	function handleRetryConnection() {
		serverError = false;
		serverErrorMessage = '';
		serverStore.fetchServerProps().then(() => {
			if (!serverStore.error) {
				serverReady = true;
			} else {
				serverError = true;
				serverErrorMessage = serverStore.error;
			}
		});
	}

	$effect(() => {
		if (!IS_TAURI_ENV) return;
		if ((window as any).__DELTA_PORT__ != null && !(window as any).__DELTA_SERVER_ERROR__) {
			serverReady = true;
			return;
		}
		if ((window as any).__DELTA_SERVER_ERROR__) {
			serverError = true;
			serverErrorMessage = 'Server failed to start. Check that no other instance is running and restart the app.';
			return;
		}
		const onReady = () => {
			resetModelApiResolution();
			serverError = false;
			serverReady = true;
		};
		const onError = () => {
			resetModelApiResolution();
			serverError = true;
			serverErrorMessage = 'Server failed to start. Check that no other instance is running and restart the app.';
		};
		window.addEventListener('delta-server-ready', onReady);
		window.addEventListener('delta-server-error', onError);

		// Poll via Tauri command as fallback — handles the race where
		// window.eval() fires before SvelteKit hydrates and the event is lost
		let pollFailures = 0;
		const poll = setInterval(async () => {
			try {
				const { invoke } = await import('@tauri-apps/api/core');
				const [port, mapiPort, ready, error] = await invoke<[number, number, boolean, boolean]>('get_server_status');
				pollFailures = 0;
				if (ready) {
					(window as any).__DELTA_PORT__ = port;
					(window as any).__DELTA_MODEL_API_PORT__ = mapiPort;
					resetModelApiResolution();
					serverError = false;
					serverReady = true;
					clearInterval(poll);
				} else if (error) {
					serverError = true;
					serverErrorMessage = 'Server failed to start. Check that no other instance is running and restart the app.';
					clearInterval(poll);
				}
			} catch (e) {
				pollFailures++;
				if (pollFailures >= 60) {
					console.error('[Delta] IPC poll failed 60 times, giving up:', e);
					serverError = true;
					serverErrorMessage = 'Unable to communicate with the server process.';
					clearInterval(poll);
				}
			}
		}, 500);

		return () => {
			clearInterval(poll);
			window.removeEventListener('delta-server-ready', onReady);
			window.removeEventListener('delta-server-error', onError);
		};
	});

	let modelApiReady = $state(!browser || typeof window === 'undefined');
	$effect(() => {
		if (!serverReady) return;
		if (!browser || typeof window === 'undefined') {
			modelApiReady = true;
			return;
		}
		Promise.race([
			resolveModelApiBaseUrl().then(() => true),
			new Promise<boolean>((resolve) => setTimeout(() => resolve(false), 3000))
		]).then((resolved) => {
			if (!resolved) {
				console.warn('Model API readiness check timed out after 3s, proceeding anyway');
			}
			modelApiReady = true;
		});
	});

	$effect(() => {
		if ((serverReady && modelApiReady) || serverError) {
			const el = document.getElementById('app-loading');
			if (el) {
				el.style.transition = 'opacity 0.3s';
				el.style.opacity = '0';
				setTimeout(() => el.remove(), 400);
			}
		}
	});

	let isChatRoute = $derived(page.route.id === '/chat/[id]');
	let isHomeRoute = $derived(page.route.id === '/');
	let isToolRoute = $derived(page.route.id === '/calendar');
	let isNewChatMode = $derived(page.url.searchParams.get('new_chat') === 'true');
	let showSidebarByDefault = $derived(activeMessages().length > 0 || isLoading());
	let currentConfig = $derived(config());
	let sidebarOpen = $state(false);
	let innerHeight = $state<number | undefined>();
	let chatSidebar:
		| { activateSearchMode?: () => void; editActiveConversation?: () => void }
		| undefined = $state();

	// Conversation title update dialog state
	let titleUpdateDialogOpen = $state(false);
	let titleUpdateCurrentTitle = $state('');
	let titleUpdateNewTitle = $state('');
	let titleUpdateResolve: ((value: boolean) => void) | null = null;

	// Global keyboard shortcuts
	function handleKeydown(event: KeyboardEvent) {
		const isCtrlOrCmd = event.ctrlKey || event.metaKey;

		if (isCtrlOrCmd && event.key === 'k') {
			event.preventDefault();
			if (chatSidebar?.activateSearchMode) {
				chatSidebar.activateSearchMode();
				sidebarOpen = true;
			}
		}

		if (isCtrlOrCmd && event.shiftKey && event.key === 'O') {
			event.preventDefault();
			goto('?new_chat=true#/');
		}

		if (event.shiftKey && isCtrlOrCmd && event.key === 'E') {
			event.preventDefault();

			if (chatSidebar?.editActiveConversation) {
				chatSidebar.editActiveConversation();
			}
		}
	}

	function handleTitleUpdateCancel() {
		titleUpdateDialogOpen = false;
		if (titleUpdateResolve) {
			titleUpdateResolve(false);
			titleUpdateResolve = null;
		}
	}

	function handleTitleUpdateConfirm() {
		titleUpdateDialogOpen = false;
		if (titleUpdateResolve) {
			titleUpdateResolve(true);
			titleUpdateResolve = null;
		}
	}

	$effect(() => {
		const alwaysShow = currentConfig.alwaysShowSidebar === true;
		const autoShowOnNewChat = currentConfig.autoShowSidebarOnNewChat !== false;
		if (alwaysShow) {
			sidebarOpen = true;
		} else if (isHomeRoute && !isNewChatMode) {
			sidebarOpen = false;
		} else if (isHomeRoute && isNewChatMode) {
			sidebarOpen = autoShowOnNewChat;
		} else if (isChatRoute || isToolRoute) {
			sidebarOpen = true;
		} else {
			sidebarOpen = showSidebarByDefault;
		}
	});

	$effect(() => {
		if (!serverReady) return;
		serverStore.fetchServerProps();
	});

	$effect(() => {
		if (serverReady && modelApiReady && !serverError) {
			startReminderPolling();
		}
	});

	onDestroy(() => {
		stopReminderPolling();
	});

	// Sync settings when server props are loaded
	$effect(() => {
		const serverProps = serverStore.serverProps;

		if (serverProps?.default_generation_settings?.params) {
			settingsStore.syncWithServerDefaults();
		}
	});

	$effect(() => {
		if (!serverReady) return;
		const apiKey = config().apiKey;

		if (
			(page.route.id === '/' || page.route.id === '/chat/[id]') &&
			page.status !== 401 &&
			page.status !== 403
		) {
			const headers: Record<string, string> = {
				'Content-Type': 'application/json'
			};

			if (apiKey && apiKey.trim() !== '') {
				headers.Authorization = `Bearer ${apiKey.trim()}`;
			}

			fetch(`${getServerBaseUrl()}/props`, { headers })
				.then((response) => {
					if (response.status === 401 || response.status === 403) {
						serverError = true;
						serverErrorMessage = 'Access denied — check your API key settings.';
					}
				})
				.catch(() => {});
		}
	});

	// Set up title update confirmation callback
	$effect(() => {
		setTitleUpdateConfirmationCallback(async (currentTitle: string, newTitle: string) => {
			return new Promise<boolean>((resolve) => {
				titleUpdateCurrentTitle = currentTitle;
				titleUpdateNewTitle = newTitle;
				titleUpdateResolve = resolve;
				titleUpdateDialogOpen = true;
			});
		});
	});
</script>

<ModeWatcher />

<Toaster richColors />
<NotificationCenter />

{#if serverError}
	<div class="splash-screen">
		<ServerErrorSplash
			error={serverErrorMessage}
			onRetry={handleRetryConnection}
			showRetry={true}
			showTroubleshooting={true}
			class="h-full"
		/>
	</div>
{:else if serverReady && modelApiReady}
	<!-- isInitialized distinguishes "not onboarded yet" from "localStorage not read yet". -->
	{#if settingsStore.isInitialized && !config().onboardingCompleted}
		<OnboardingDialog />
	{/if}

	<ConversationTitleUpdateDialog
		bind:open={titleUpdateDialogOpen}
		currentTitle={titleUpdateCurrentTitle}
		newTitle={titleUpdateNewTitle}
		onConfirm={handleTitleUpdateConfirm}
		onCancel={handleTitleUpdateCancel}
	/>

	<Sidebar.Provider bind:open={sidebarOpen}>
		<div class="flex h-screen w-full" style:height="{innerHeight}px">
			<Sidebar.Root class="h-full">
				<ChatSidebar bind:this={chatSidebar} />
			</Sidebar.Root>

			<Sidebar.Trigger
				class="transition-left absolute left-0 z-[900] h-8 w-8 duration-200 ease-linear {sidebarOpen
					? 'md:left-[var(--sidebar-width)]'
					: ''}"
				style="translate: 1rem 1rem;"
			/>

			<Sidebar.Inset class="flex flex-1 flex-col overflow-hidden">
				{@render children?.()}
			</Sidebar.Inset>
		</div>
	</Sidebar.Provider>
{/if}

<svelte:window onkeydown={handleKeydown} bind:innerHeight />

<style>
	.splash-screen {
		display: flex;
		align-items: center;
		justify-content: center;
		height: 100vh;
		background: var(--background);
		color: var(--foreground);
		overflow: hidden;
	}
</style>

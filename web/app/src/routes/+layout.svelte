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
	import { goto } from '$app/navigation';

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
		} else if (isChatRoute) {
			sidebarOpen = true;
		} else {
			sidebarOpen = showSidebarByDefault;
		}
	});

	$effect(() => {
		if (!serverReady) return;
		serverStore.fetchServerProps();
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
{:else}
	<div class="splash-screen">
		<div class="splash-content">
			<div class="splash-logo">
				<svg viewBox="0 0 430 580" xmlns="http://www.w3.org/2000/svg">
					<g transform="translate(0,580) scale(0.1,-0.1)" fill="currentColor" stroke="none">
						<path d="M1490 5784 c-154 -26 -338 -69 -418 -98 -47 -17 -183 -88 -253 -131 -76 -48 -191 -173 -227 -246 -59 -122 -76 -267 -47 -405 16 -77 63 -196 100 -253 33 -52 150 -179 230 -252 39 -34 79 -72 90 -83 31 -32 236 -199 294 -240 28 -20 59 -44 69 -54 10 -9 46 -39 81 -67 35 -27 85 -68 110 -90 26 -22 71 -60 100 -85 58 -49 59 -50 181 -159 l83 -75 -34 -12 c-19 -7 -47 -13 -63 -13 -16 -1 -55 -7 -85 -15 -31 -8 -87 -22 -126 -32 -64 -15 -200 -58 -315 -99 -66 -24 -223 -99 -288 -139 -31 -19 -79 -46 -107 -62 -54 -29 -174 -117 -270 -198 -234 -195 -445 -501 -518 -751 -14 -47 -24 -86 -61 -247 -22 -93 -23 -498 -1 -601 51 -244 75 -322 138 -457 64 -134 139 -249 228 -345 21 -22 47 -52 58 -66 11 -14 53 -53 93 -85 40 -32 80 -66 89 -75 24 -23 192 -126 289 -177 89 -46 142 -68 200 -82 19 -5 67 -18 105 -29 39 -11 127 -30 198 -41 109 -18 158 -21 350 -17 249 4 360 20 528 72 26 8 53 15 62 15 8 0 36 9 63 21 27 11 58 24 69 28 27 11 123 59 220 110 64 34 253 165 275 190 3 3 22 19 43 35 60 45 272 263 303 312 15 24 31 44 35 44 4 0 18 17 30 38 12 20 35 54 51 74 69 88 191 353 223 483 83 335 72 757 -26 1035 -28 80 -30 83 -98 222 -132 266 -351 548 -531 681 -30 23 -59 46 -65 53 -49 57 -572 414 -860 587 -11 7 -22 14 -25 17 -3 3 -41 27 -85 53 -44 27 -114 70 -155 97 -41 28 -84 54 -95 60 -24 12 -20 9 -205 129 -234 151 -279 182 -399 270 -126 93 -222 198 -267 289 -25 51 -28 67 -28 162 -1 143 23 190 141 288 143 118 351 179 608 176 107 -2 282 -27 299 -44 5 -5 15 -10 22 -10 14 0 89 -34 126 -58 144 -91 251 -179 448 -366 203 -194 330 -274 466 -294 78 -11 166 1 238 32 52 23 129 102 156 162 31 66 34 210 6 301 -37 120 -146 225 -331 317 -191 95 -381 147 -732 201 -136 20 -658 20 -783 -1z m751 -2581 c109 -112 339 -397 339 -420 0 -5 11 -24 24 -43 36 -54 120 -217 150 -293 102 -256 165 -602 152 -841 -13 -251 -41 -411 -106 -603 -62 -185 -141 -320 -270 -457 -52 -55 -147 -134 -180 -149 -8 -4 -28 -16 -45 -27 -42 -27 -90 -48 -165 -73 -56 -18 -89 -21 -245 -21 -207 0 -269 14 -390 85 -37 21 -69 39 -72 39 -12 0 -143 108 -198 164 -53 53 -155 181 -192 241 -33 53 -111 242 -137 335 -37 132 -46 171 -62 280 -25 172 -23 557 4 705 31 174 55 257 111 395 17 41 31 78 31 82 0 11 97 172 117 193 10 11 36 43 58 70 42 53 112 125 123 125 4 0 27 19 52 41 106 95 325 202 515 252 259 69 236 74 386 -80z" />
						<path d="M3785 4976 c-92 -23 -182 -87 -249 -178 -32 -43 -66 -161 -66 -232 0 -95 41 -212 97 -271 74 -80 150 -123 247 -139 93 -16 181 -4 264 37 89 43 143 99 186 191 35 73 36 81 36 189 0 84 -4 118 -15 133 -8 10 -15 26 -15 35 -1 45 -113 163 -200 209 -68 37 -195 48 -285 26z" />
					</g>
				</svg>
			</div>
			<h1 class="splash-title">Delta</h1>
			<p class="splash-status">Starting<span class="splash-dots"><span>.</span><span>.</span><span>.</span></span></p>
		</div>
	</div>
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
	.splash-content {
		text-align: center;
		animation: splashFadeIn 0.6s ease-out forwards;
		opacity: 0;
	}
	.splash-logo {
		width: 80px;
		height: 80px;
		margin: 0 auto 1.5rem;
		animation: splashPulse 2s ease-in-out infinite;
		color: #3898ec;
		filter: drop-shadow(0 0 20px rgba(56, 152, 236, 0.3));
	}
	.splash-logo svg {
		width: 100%;
		height: 100%;
	}
	.splash-title {
		font-size: 1.5rem;
		font-weight: 600;
		letter-spacing: -0.02em;
		margin-bottom: 0.25rem;
	}
	.splash-status {
		font-size: 0.875rem;
		opacity: 0.5;
	}
	.splash-dots span {
		animation: splashBlink 1.4s infinite both;
	}
	.splash-dots span:nth-child(2) {
		animation-delay: 0.2s;
	}
	.splash-dots span:nth-child(3) {
		animation-delay: 0.4s;
	}
	@keyframes splashFadeIn {
		to { opacity: 1; }
	}
	@keyframes splashPulse {
		0%, 100% { transform: scale(1); opacity: 1; }
		50% { transform: scale(1.05); opacity: 0.8; }
	}
	@keyframes splashBlink {
		0%, 80%, 100% { opacity: 0; }
		40% { opacity: 1; }
	}
</style>

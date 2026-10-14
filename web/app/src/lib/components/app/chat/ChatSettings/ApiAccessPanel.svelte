<script lang="ts">
	import { Copy, Eye, EyeOff, KeyRound, RefreshCw } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Checkbox } from '$lib/components/ui/checkbox';
	import {
		apiSnippets,
		getApiServer,
		listApiModels,
		updateApiServer,
		type ApiServerChange,
		type ApiServerState
	} from '$lib/services/developer-api';
	import { copyToClipboard } from '$lib/utils/copy';
	import { onMount } from 'svelte';
	import { toast } from 'svelte-sonner';

	interface Props {
		/** Keeps the settings page's unsaved copy of the key in step, so Save cannot undo it. */
		onKeyChange?: (key: string) => void;
	}

	let { onKeyChange }: Props = $props();

	const ENDPOINTS = [
		['GET', '/v1/models', 'Your downloaded models'],
		['POST', '/v1/chat/completions', 'Chat, streaming, tools and JSON mode'],
		['POST', '/v1/completions', 'Plain text completion'],
		['POST', '/v1/embeddings', 'Embeddings (models that support them)']
	] as const;
	const LANGUAGES = [
		['curl', 'curl'],
		['python', 'Python'],
		['javascript', 'JavaScript']
	] as const;

	let server = $state<ApiServerState | null>(null);
	let models = $state<string[]>([]);
	let loadError = $state('');
	let busy = $state(false);
	let showKey = $state(false);
	let language = $state<(typeof LANGUAGES)[number][0]>('curl');

	const snippets = $derived(
		server ? apiSnippets(server.local_url, models[0] ?? 'MODEL_ID', server.has_key) : null
	);

	async function load() {
		try {
			server = await getApiServer();
			loadError = '';
			models = await listApiModels(server.has_key ? server.api_key : '').catch(() => []);
		} catch {
			loadError = 'The API settings could not be loaded. Is Delta running?';
		}
	}

	async function change(update: ApiServerChange, done: string) {
		busy = true;
		try {
			server = await updateApiServer(update);
			onKeyChange?.(server.has_key ? server.api_key : '');
			toast.success(done);
		} catch (error) {
			toast.error(error instanceof Error ? error.message : 'Could not change the API settings.');
		} finally {
			busy = false;
		}
	}

	onMount(load);
</script>

{#if loadError}
	<p role="alert" class="text-sm text-destructive">{loadError}</p>
{:else if !server}
	<p class="text-sm text-muted-foreground">Loading…</p>
{:else}
	<div class="space-y-8">
		<section aria-label="Address" class="space-y-3">
			<p class="text-sm text-muted-foreground">
				Apps and code that speak the OpenAI API can use your Delta models while Delta is open. Point
				their base URL here.
			</p>
			<div class="flex flex-wrap items-center gap-2">
				<code class="rounded-md border bg-muted/40 px-3 py-1.5 text-sm break-all"
					>{server.local_url}</code
				>
				<Button
					variant="outline"
					size="sm"
					aria-label="Copy base URL"
					onclick={() => copyToClipboard(server!.local_url, 'Base URL copied')}
				>
					<Copy class="h-3.5 w-3.5" /> Copy
				</Button>
			</div>

			<div class="flex items-start gap-3 pt-2">
				<Checkbox
					id="api-network"
					checked={server.network}
					disabled={busy}
					onCheckedChange={(checked) =>
						change(
							{ network: checked === true },
							checked ? 'Shared on your network' : 'Available on this computer only'
						)}
					class="mt-1"
				/>
				<div class="space-y-1">
					<label for="api-network" class="cursor-pointer text-sm leading-none font-medium"
						>Allow devices on my network</label
					>
					<p class="text-xs text-muted-foreground">
						Other computers and phones on your network can call the API with the key below.
					</p>
					{#if server.network_url}
						<div class="flex flex-wrap items-center gap-2 pt-1">
							<code class="rounded-md border bg-muted/40 px-2 py-1 text-xs break-all"
								>{server.network_url}</code
							>
							<Button
								variant="ghost"
								size="sm"
								aria-label="Copy network URL"
								onclick={() => copyToClipboard(server!.network_url!, 'Network URL copied')}
							>
								<Copy class="h-3.5 w-3.5" />
							</Button>
						</div>
					{/if}
				</div>
			</div>
			{#if server.overridden}
				<p class="text-xs text-muted-foreground">
					Delta was started with --host or --api-key, so those apply until it restarts.
				</p>
			{/if}
		</section>

		<section aria-label="API key" class="space-y-3 border-t border-border pt-6">
			<h3 class="flex items-center gap-2 text-sm font-medium">
				<KeyRound class="h-4 w-4" /> API key
			</h3>
			{#if server.has_key}
				<div class="flex flex-wrap items-center gap-2">
					<code class="rounded-md border bg-muted/40 px-3 py-1.5 text-sm break-all"
						>{showKey ? server.api_key : '•'.repeat(24)}</code
					>
					<Button
						variant="ghost"
						size="sm"
						aria-label={showKey ? 'Hide key' : 'Show key'}
						onclick={() => (showKey = !showKey)}
					>
						{#if showKey}<EyeOff class="h-3.5 w-3.5" />{:else}<Eye class="h-3.5 w-3.5" />{/if}
					</Button>
					<Button
						variant="outline"
						size="sm"
						aria-label="Copy key"
						onclick={() => copyToClipboard(server!.api_key, 'API key copied')}
					>
						<Copy class="h-3.5 w-3.5" /> Copy
					</Button>
				</div>
				<p class="text-xs text-muted-foreground">
					Send it as <code>Authorization: Bearer &lt;key&gt;</code>.
				</p>
				<div class="flex flex-wrap gap-2">
					<Button
						variant="outline"
						size="sm"
						disabled={busy}
						onclick={() => change({ key: 'generate' }, 'New key created; the old one stops working')}
					>
						<RefreshCw class="h-3.5 w-3.5" /> New key
					</Button>
					{#if !server.network}
						<Button
							variant="ghost"
							size="sm"
							disabled={busy}
							onclick={() => change({ key: 'remove' }, 'Key removed')}>Remove key</Button
						>
					{/if}
				</div>
			{:else}
				<p class="text-xs text-muted-foreground">
					No key needed on this computer; any key value works. Create one to require it.
				</p>
				<Button
					variant="outline"
					size="sm"
					disabled={busy}
					onclick={() => change({ key: 'generate' }, 'API key created')}>Create key</Button
				>
			{/if}
		</section>

		<section aria-label="Models" class="space-y-3 border-t border-border pt-6">
			<h3 class="text-sm font-medium">Models</h3>
			{#if models.length === 0}
				<p class="text-xs text-muted-foreground">Download a model to use it through the API.</p>
			{:else}
				<p class="text-xs text-muted-foreground">Pass one of these as <code>model</code>.</p>
				<ul class="flex flex-wrap gap-2">
					{#each models as id (id)}
						<li>
							<button
								class="rounded-md border px-2 py-1 font-mono text-xs hover:bg-accent"
								onclick={() => copyToClipboard(id, `Copied ${id}`)}>{id}</button
							>
						</li>
					{/each}
				</ul>
			{/if}
		</section>

		<section aria-label="Example" class="space-y-3 border-t border-border pt-6">
			<div class="flex flex-wrap items-center justify-between gap-2">
				<h3 class="text-sm font-medium">Example</h3>
				<div role="tablist" class="flex gap-1">
					{#each LANGUAGES as [value, label] (value)}
						<button
							role="tab"
							aria-selected={language === value}
							class="rounded-md px-2 py-1 text-xs {language === value
								? 'bg-accent font-medium'
								: 'text-muted-foreground hover:text-foreground'}"
							onclick={() => (language = value)}>{label}</button
						>
					{/each}
				</div>
			</div>
			{#if snippets}
				<div class="relative">
					<pre
						class="overflow-x-auto rounded-md border bg-muted/40 p-3 pr-12 text-xs"><code
							>{snippets[language]}</code
						></pre>
					<Button
						variant="ghost"
						size="sm"
						class="absolute top-1.5 right-1.5"
						aria-label="Copy example"
						onclick={() => copyToClipboard(snippets![language], 'Example copied')}
					>
						<Copy class="h-3.5 w-3.5" />
					</Button>
				</div>
			{/if}
		</section>

		<section aria-label="Endpoints" class="space-y-2 border-t border-border pt-6">
			<h3 class="text-sm font-medium">Endpoints</h3>
			<ul class="space-y-1 text-xs">
				{#each ENDPOINTS as [method, path, what] (path)}
					<li class="flex flex-wrap gap-2">
						<span class="w-10 font-mono text-muted-foreground">{method}</span>
						<code class="font-mono">{path}</code>
						<span class="text-muted-foreground">{what}</span>
					</li>
				{/each}
			</ul>
		</section>
	</div>
{/if}

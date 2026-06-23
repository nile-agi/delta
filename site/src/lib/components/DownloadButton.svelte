<script lang="ts">
	import { onMount } from 'svelte';

	const REPO = 'nile-agi/delta';

	type Platform = 'macos' | 'windows' | 'linux' | 'unknown';

	let platform = $state<Platform>('unknown');
	let downloadUrl = $state('');
	let loading = $state(true);
	let allAssets = $state<{ name: string; url: string }[]>([]);

	const labels: Record<Platform, string> = {
		macos: 'Download for macOS',
		windows: 'Download for Windows',
		linux: 'Download for Linux',
		unknown: 'Download'
	};

	const extensions: Record<Platform, string[]> = {
		macos: ['.dmg', '.app.tar.gz'],
		windows: ['.msi', '.exe'],
		linux: ['.AppImage', '.deb'],
		unknown: []
	};

	function detectPlatform(): Platform {
		const ua = navigator.userAgent.toLowerCase();
		if (ua.includes('mac')) return 'macos';
		if (ua.includes('win')) return 'windows';
		if (ua.includes('linux')) return 'linux';
		return 'unknown';
	}

	onMount(async () => {
		platform = detectPlatform();

		try {
			const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`);
			if (res.ok) {
				const data = await res.json();
				allAssets = (data.assets || []).map((a: { name: string; browser_download_url: string }) => ({
					name: a.name,
					url: a.browser_download_url
				}));

				const exts = extensions[platform];
				for (const ext of exts) {
					const match = allAssets.find((a) => a.name.endsWith(ext));
					if (match) {
						downloadUrl = match.url;
						break;
					}
				}
			}
		} catch {
			// GitHub API unavailable — fall back to releases page
		}
		loading = false;
	});

	const releasesUrl = `https://github.com/${REPO}/releases/latest`;
</script>

<div class="flex flex-col items-center gap-4">
	{#if loading}
		<div class="bg-white text-delta-blue px-8 py-4 rounded-xl font-semibold text-lg opacity-50">
			Loading...
		</div>
	{:else if downloadUrl}
		<a href={downloadUrl}
			class="bg-white text-delta-blue px-8 py-4 rounded-xl font-semibold text-lg
				hover:bg-white/90 hover:scale-105 transition-all shadow-lg shadow-black/20">
			{labels[platform]}
		</a>
	{:else}
		<a href={releasesUrl} target="_blank" rel="noopener"
			class="bg-white text-delta-blue px-8 py-4 rounded-xl font-semibold text-lg
				hover:bg-white/90 hover:scale-105 transition-all shadow-lg shadow-black/20">
			View Downloads
		</a>
	{/if}

	<a href={releasesUrl} target="_blank" rel="noopener"
		class="text-sm text-white/50 hover:text-white/80 transition-colors">
		All platforms &rarr;
	</a>
</div>

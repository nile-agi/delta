<script lang="ts">
	import '../app.css';

	let { children } = $props();
</script>

<div class="min-h-screen bg-delta-blue text-white">
	<nav class="fixed top-0 w-full z-50 bg-delta-blue/90 backdrop-blur-sm border-b border-white/10">
		<div class="max-w-6xl mx-auto px-6 h-16 flex items-center justify-between">
			<a href="/" class="flex items-center gap-3 font-bold text-xl">
				<img src="/favicon.svg" alt="Delta" class="h-8 w-8 invert" />
				Delta
			</a>
			<div class="flex items-center gap-6">
				<a href="/#features" class="text-sm text-white/70 hover:text-white transition-colors">Features</a>
				<a href="https://github.com/nile-agi/delta" target="_blank" rel="noopener"
					class="text-sm text-white/70 hover:text-white transition-colors">GitHub</a>
				<a href="/#download"
					class="text-sm bg-white text-delta-blue px-4 py-2 rounded-lg font-medium hover:bg-white/90 transition-colors">
					Download
				</a>
			</div>
		</div>
	</nav>

	<main class="pt-16">
		{@render children()}
	</main>

	<footer class="border-t border-white/10 py-12">
		<div class="max-w-6xl mx-auto px-6 flex flex-col md:flex-row items-center justify-between gap-4">
			<p class="text-sm text-white/50">
				&copy; {new Date().getFullYear()} Nile AGI. Open source under MIT License.
			</p>
			<div class="flex items-center gap-6">
				<a href="https://github.com/nile-agi/delta" target="_blank" rel="noopener"
					class="text-sm text-white/50 hover:text-white transition-colors">GitHub</a>
				<a href="https://github.com/nile-agi/delta/issues" target="_blank" rel="noopener"
					class="text-sm text-white/50 hover:text-white transition-colors">Issues</a>
			</div>
		</div>
	</footer>
</div>

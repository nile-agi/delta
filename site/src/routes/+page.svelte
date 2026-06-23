<script lang="ts">
	import DownloadButton from '$lib/components/DownloadButton.svelte';
</script>

<svelte:head>
	<title>Delta - Offline AI Assistant</title>
	<meta name="description" content="Run powerful AI models locally on your device. Private, fast, no internet required." />
</svelte:head>

<!-- Hero -->
<section class="min-h-[90vh] flex flex-col items-center justify-center text-center px-6">
	<div class="mb-8">
		<img src="/favicon.svg" alt="Delta" class="h-24 w-24 mx-auto mb-6 invert" />
		<h1 class="text-5xl md:text-7xl font-bold tracking-tight mb-4">
			Delta
		</h1>
		<p class="text-xl md:text-2xl text-white/70 max-w-2xl mx-auto">
			Run AI locally, privately.<br />
			No cloud. No data leaves your device.
		</p>
	</div>

	<div id="download" class="mt-8">
		<DownloadButton />
	</div>

	<div class="mt-16 flex items-center gap-8 text-sm text-white/40">
		<span>macOS</span>
		<span>&bull;</span>
		<span>Windows</span>
		<span>&bull;</span>
		<span>Linux</span>
	</div>
</section>

<!-- Features -->
<section id="features" class="py-24 px-6">
	<div class="max-w-6xl mx-auto">
		<h2 class="text-3xl md:text-4xl font-bold text-center mb-16">
			AI that runs on your machine
		</h2>

		<div class="grid md:grid-cols-3 gap-8">
			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#128274;</div>
				<h3 class="text-xl font-semibold mb-3">Completely Private</h3>
				<p class="text-white/60 leading-relaxed">
					Your conversations never leave your device. No accounts, no telemetry,
					no cloud processing. Your data stays yours.
				</p>
			</div>

			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#9889;</div>
				<h3 class="text-xl font-semibold mb-3">GPU Accelerated</h3>
				<p class="text-white/60 leading-relaxed">
					Automatic GPU acceleration with Apple Metal, NVIDIA CUDA, and Vulkan.
					Fast inference on any hardware.
				</p>
			</div>

			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#128268;</div>
				<h3 class="text-xl font-semibold mb-3">Works Offline</h3>
				<p class="text-white/60 leading-relaxed">
					Download a model once, use it forever. No internet connection needed
					after initial setup. Perfect for air-gapped environments.
				</p>
			</div>

			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#128640;</div>
				<h3 class="text-xl font-semibold mb-3">Multiple Models</h3>
				<p class="text-white/60 leading-relaxed">
					Choose from Qwen, Llama, Mistral, Gemma, and more. Download models
					from Hugging Face with a single command.
				</p>
			</div>

			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#127760;</div>
				<h3 class="text-xl font-semibold mb-3">Web UI & CLI</h3>
				<p class="text-white/60 leading-relaxed">
					Beautiful chat interface in your browser, or use the command line.
					OpenAI-compatible API for integration with your tools.
				</p>
			</div>

			<div class="bg-white/5 rounded-2xl p-8 border border-white/10">
				<div class="text-3xl mb-4">&#128736;</div>
				<h3 class="text-xl font-semibold mb-3">Open Source</h3>
				<p class="text-white/60 leading-relaxed">
					Built on llama.cpp. MIT licensed. Inspect the code, contribute,
					or fork it for your own use.
				</p>
			</div>
		</div>
	</div>
</section>

<!-- Install CLI -->
<section class="py-24 px-6 bg-white/5">
	<div class="max-w-4xl mx-auto text-center">
		<h2 class="text-3xl md:text-4xl font-bold mb-8">
			Or install via command line
		</h2>

		<div class="grid md:grid-cols-2 gap-6 text-left">
			<div class="bg-delta-blue rounded-xl p-6 border border-white/10">
				<p class="text-sm text-white/50 mb-2">macOS (Homebrew)</p>
				<code class="text-sm text-delta-green font-mono">
					brew tap nile-agi/delta-cli<br />
					brew install --HEAD delta-cli
				</code>
			</div>

			<div class="bg-delta-blue rounded-xl p-6 border border-white/10">
				<p class="text-sm text-white/50 mb-2">Windows (WinGet)</p>
				<code class="text-sm text-delta-green font-mono">
					winget install DeltaCLI.DeltaCLI
				</code>
			</div>
		</div>

		<p class="mt-8 text-white/50 text-sm">
			Then run <code class="text-delta-green">delta</code> to start chatting, or
			<code class="text-delta-green">delta server</code> to start the web UI.
		</p>
	</div>
</section>

<!-- CTA -->
<section class="py-24 px-6 text-center">
	<h2 class="text-3xl md:text-4xl font-bold mb-4">
		Ready to run AI locally?
	</h2>
	<p class="text-white/60 mb-8 text-lg">
		Download Delta and start chatting in minutes.
	</p>
	<DownloadButton />
</section>

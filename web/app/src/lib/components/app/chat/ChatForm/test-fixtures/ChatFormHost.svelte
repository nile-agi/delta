<script lang="ts">
	import ChatForm from '../ChatForm.svelte';
	import { TooltipProvider } from '$lib/components/ui/tooltip';
	let { onFileUpload }: { onFileUpload?: (files: File[]) => void } = $props();
</script>

<TooltipProvider>
	<ChatForm {onFileUpload} />
</TooltipProvider>

<script lang="ts">
	import { Wrench } from '@lucide/svelte';
	import { Badge } from '$lib/components/ui/badge';

	interface Props {
		class?: string;
	}

	let { class: className = '' }: Props = $props();
</script>

<Badge
	variant="secondary"
	class="h-5 border-border bg-secondary px-1.5 py-0.5 text-xs text-secondary-foreground {className}"
>
	<Wrench class="mr-1 h-3 w-3" />
	Custom
</Badge>

<script module lang="ts">
	import { defineMeta } from '@storybook/addon-svelte-csf';
	import { SettingsPage } from '$lib/components/app';
	import { fn } from 'storybook/test';
	import type { ComponentProps } from 'svelte';

	const { Story } = defineMeta({
		title: 'Components/SettingsPage',
		component: SettingsPage,
		parameters: { layout: 'fullscreen' },
		args: { onBack: fn() }
	});
</script>

{#snippet template(args: ComponentProps<typeof SettingsPage>)}
	<div class="h-screen">
		<SettingsPage {...args} />
	</div>
{/snippet}

<Story name="General" {template} />
<Story name="Model Management" args={{ section: 'Model Management' }} {template} />

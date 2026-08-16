<script lang="ts">
	import { config, updateConfig } from '$lib/stores/settings.svelte';
	import { Input } from '$lib/components/ui/input/index.js';
	import { Button } from '$lib/components/ui/button/index.js';

	const currentConfig = $derived(config());

	function toggle(key: 'alwaysShowSidebar' | 'autoShowSidebarOnNewChat' | 'useAgentTools') {
		updateConfig(key, !currentConfig[key]);
	}
</script>

<div class="h-screen w-full overflow-y-auto bg-background text-foreground">
	<div class="mx-auto max-w-2xl p-6">
		<h2 class="mb-6 text-xl font-semibold">Settings</h2>

		<!-- 1. PROFILE -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Profile</h3>
			<label for="settings-username" class="mb-1 block text-sm">Display name</label>
			<Input
				id="settings-username"
				value={currentConfig.userName || ''}
				placeholder="Your name"
				oninput={(e) => updateConfig('userName', (e.target as HTMLInputElement).value)}
			/>
			<p class="mt-2 text-xs text-muted-foreground">
				Shown in the sidebar footer and used for greetings.
			</p>
		</section>

		<!-- 2. APPEARANCE / SIDEBAR -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Appearance</h3>
			<label class="flex items-center justify-between py-2 text-sm">
				Always show sidebar
				<input
					type="checkbox"
					checked={currentConfig.alwaysShowSidebar === true}
					onchange={() => toggle('alwaysShowSidebar')}
				/>
			</label>
			<label class="flex items-center justify-between py-2 text-sm">
				Show sidebar on new chat
				<input
					type="checkbox"
					checked={currentConfig.autoShowSidebarOnNewChat !== false}
					onchange={() => toggle('autoShowSidebarOnNewChat')}
				/>
			</label>
		</section>

		<!-- 3. CALENDAR -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Calendar</h3>
			<label for="settings-week-start" class="mb-1 block text-sm">Week starts on</label>
			<select
				id="settings-week-start"
				class="h-9 w-full rounded-md border border-input bg-background px-2 text-sm"
				value={String(currentConfig.calendarWeekStart ?? 'monday')}
				onchange={(e) => updateConfig('calendarWeekStart', (e.target as HTMLSelectElement).value)}
			>
				<option value="monday">Monday</option>
				<option value="sunday">Sunday</option>
			</select>
		</section>

		<!-- 4. AGENT TOOLS -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Agent</h3>
			<label class="flex items-center justify-between py-2 text-sm">
				Enable agent tools
				<input
					type="checkbox"
					checked={currentConfig.useAgentTools === true}
					onchange={() => toggle('useAgentTools')}
				/>
			</label>
			<p class="mt-1 text-xs text-muted-foreground">
				Allows the model to use calendar, notes and other local tools.
			</p>
		</section>

		<!-- 5. SECURITY / CONNECTION -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Security</h3>
			<label for="settings-apikey" class="mb-1 block text-sm">API key (optional)</label>
			<Input
				id="settings-apikey"
				type="password"
				value={currentConfig.apiKey || ''}
				placeholder="sk-..."
				oninput={(e) => updateConfig('apiKey', (e.target as HTMLInputElement).value)}
			/>
			<p class="mt-2 text-xs text-muted-foreground">
				Sent as a Bearer token when the app talks to the local server.
			</p>
		</section>

		<!-- 6. ONBOARDING -->
		<section class="mb-6 rounded-lg border p-4">
			<h3 class="mb-3 text-sm font-semibold uppercase text-muted-foreground">Onboarding</h3>
			<Button
				variant="outline"
				onclick={() => updateConfig('onboardingCompleted', false)}
			>
				Re-run onboarding
			</Button>
			<p class="mt-2 text-xs text-muted-foreground">
				Shows the welcome / setup dialog again on next launch.
			</p>
		</section>
	</div>
</div>
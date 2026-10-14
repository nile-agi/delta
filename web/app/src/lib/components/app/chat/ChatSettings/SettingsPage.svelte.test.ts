import '../../../../../app.css';
import { afterEach, beforeEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import SettingsPage from './SettingsPage.svelte';
import { settingsStore } from '$lib/stores/settings.svelte';
import { SETTING_CONFIG_DEFAULT } from '$lib/constants/settings-config';

beforeEach(async () => {
	await page.viewport(1280, 800);
});

afterEach(() => {
	cleanup();
	vi.unstubAllGlobals();
	settingsStore.config = { ...SETTING_CONFIG_DEFAULT };
});
function openSettings(section?: string) {
	vi.stubGlobal(
		'fetch',
		vi.fn(
			async () =>
				new Response(JSON.stringify({ blocked: false }), {
					headers: { 'Content-Type': 'application/json' }
				})
		)
	);
	const result = render(SettingsPage, { section, onBack: vi.fn() });
	result.container.style.height = '100vh';
}
it('opens everyday preferences and puts technical controls in Developer', async () => {
	openSettings();
	await expect
		.element(page.getByRole('heading', { name: 'Personal preferences', exact: true }))
		.toBeVisible();
	await expect
		.element(page.getByRole('button', { name: 'Developer', exact: true }).first())
		.toBeVisible();
	await page.getByRole('button', { name: 'Developer', exact: true }).first().click();
	await expect
		.element(page.getByRole('heading', { name: 'Response controls', exact: true }))
		.toBeVisible();
	await page.getByRole('button', { name: 'Repetition controls', exact: true }).click();
	await expect
		.element(page.getByLabelText('Repetition lookback (tokens)', { exact: true }))
		.toBeVisible();
	await page.getByLabelText('Repetition lookback (tokens)', { exact: true }).fill('-1');
	await expect
		.element(page.getByText('Repetition lookback: use 0 or greater.', { exact: true }))
		.toBeVisible();
	await expect
		.element(page.getByRole('button', { name: 'Save settings', exact: true }))
		.toBeDisabled();
});
it('shows Info without save controls', async () => {
	openSettings('Info');
	await expect
		.element(page.getByRole('heading', { name: 'About Delta', exact: true }))
		.toBeVisible();
	await expect
		.element(page.getByRole('button', { name: 'Save settings', exact: true }))
		.not.toBeInTheDocument();
});
it('discards invalid edits without changing saved values', async () => {
	openSettings('Penalties');
	await page.getByLabelText('Repetition lookback (tokens)', { exact: true }).fill('-1');
	await page.getByRole('button', { name: 'Discard changes', exact: true }).click();
	await expect
		.element(page.getByLabelText('Repetition lookback (tokens)', { exact: true }))
		.toHaveValue(64);
	expect(settingsStore.config.dry_penalty_last_n).toBe(64);
});

it('only offers discard when values have changed and links to issue templates', async () => {
	openSettings();
	await expect
		.element(page.getByRole('button', { name: 'Discard changes', exact: true }))
		.not.toBeInTheDocument();
	await expect
		.element(page.getByRole('button', { name: 'Save settings', exact: true }))
		.toBeDisabled();
	await page.getByLabelText('What should Delta call you?', { exact: true }).fill('Alex');
	await expect
		.element(page.getByRole('button', { name: 'Discard changes', exact: true }))
		.toBeVisible();
	await expect
		.element(page.getByRole('button', { name: 'Save settings', exact: true }))
		.toBeEnabled();
	await page.getByRole('button', { name: 'Discard changes', exact: true }).click();
	await expect
		.element(page.getByRole('button', { name: 'Discard changes', exact: true }))
		.not.toBeInTheDocument();
	await page.getByRole('button', { name: 'Info', exact: true }).click();
	await expect
		.element(page.getByRole('link', { name: 'Get help or report a problem' }))
		.toHaveAttribute('href', 'https://github.com/nile-agi/delta-release/issues/new/choose');
});

it('keeps settings usable on narrow screens and uses a vertical section list on desktop', async () => {
	await page.viewport(1280, 800);
	openSettings();
	const groups = document.querySelector('nav[aria-label="Settings groups"]')!;
	expect(getComputedStyle(groups).flexDirection).toBe('column');
	await page.screenshot({ path: '__screenshots__/delta-settings-desktop.png' });
	await page.viewport(390, 844);
	await expect.element(page.getByLabelText('Settings group', { exact: true })).toBeVisible();
	expect(document.documentElement.scrollWidth).toBeLessThanOrEqual(390);
	await page.screenshot({ path: '__screenshots__/delta-settings-mobile.png' });
	await page.getByRole('button', { name: 'Developer', exact: true }).click();
	await expect
		.element(page.getByRole('button', { name: 'Reset to model defaults', exact: true }))
		.toBeVisible();
	expect(document.documentElement.scrollWidth).toBeLessThanOrEqual(390);
	document.documentElement.classList.add('dark');
	await page.screenshot({ path: '__screenshots__/delta-settings-mobile-dark.png' });
	document.documentElement.classList.remove('dark');
	await page.viewport(1280, 800);
});

it('keeps reply preferences optional and guides them by the selected detail level', async () => {
	openSettings();
	await expect
		.element(page.getByRole('button', { name: 'Chat preferences', exact: true }))
		.not.toBeInTheDocument();
	await expect
		.element(page.getByLabelText('Anything else about replies?', { exact: true }))
		.not.toBeVisible();
	await page.getByText('Add reply preferences (optional)', { exact: true }).click();
	const input = page.getByLabelText('Anything else about replies?', { exact: true });
	await expect
		.element(input)
		.toHaveAttribute('placeholder', expect.stringContaining('brief explanation'));
	await input.fill('Reply in Kiswahili.');
	await page.getByLabelText('How much detail should replies include?', { exact: true }).click();
	await page.getByRole('option', { name: 'Short', exact: true }).click();
	await expect
		.element(input)
		.toHaveAttribute('placeholder', expect.stringContaining('few sentences'));
	await expect.element(input).toHaveValue('Reply in Kiswahili.');
	await page.getByRole('button', { name: 'Save settings', exact: true }).click();
	expect(settingsStore.config.systemMessage).toBe('Reply in Kiswahili.');
	expect(settingsStore.config.replyStyle).toBe('concise');
});

it('includes theme in personal preferences without an Appearance tab', async () => {
	openSettings('Display');
	await expect.element(page.getByLabelText('Theme', { exact: true })).toBeVisible();
	await expect
		.element(page.getByRole('button', { name: 'Appearance', exact: true }))
		.not.toBeInTheDocument();
	await expect.element(page.getByRole('checkbox')).not.toBeInTheDocument();
});

it('keeps response statistics optional and disabled by default', async () => {
	openSettings('Developer');
	await page.getByRole('button', { name: 'Diagnostics & extra settings', exact: true }).click();
	const statistics = page.getByRole('checkbox', { name: 'Show response statistics', exact: true });
	await expect.element(statistics).not.toBeChecked();
	await statistics.click();
	await page.getByRole('button', { name: 'Save settings', exact: true }).click();
	expect(settingsStore.config.showMessageStats).toBe(true);
});

it('shows the OpenAI-compatible address, models and a working example under API access', async () => {
	let server = {
		network: false,
		host: '127.0.0.1',
		port: 8081,
		local_url: 'http://127.0.0.1:8081/v1',
		network_url: null as string | null,
		has_key: false,
		api_key: '',
		overridden: false
	};
	const fetch = vi.fn(async (url: string, init?: RequestInit) => {
		if (String(url).endsWith('/api/developer/server')) {
			if (init?.method === 'PUT') {
				server = {
					...server,
					network: true,
					host: '0.0.0.0',
					network_url: 'http://192.168.1.5:8081/v1',
					has_key: true,
					api_key: 'delta-test-key'
				};
			}
			return new Response(JSON.stringify(server));
		}
		if (String(url).endsWith('/v1/models'))
			return new Response(JSON.stringify({ data: [{ id: 'qwen2.5:0.5b' }] }));
		return new Response(JSON.stringify({ blocked: false }));
	});
	vi.stubGlobal('fetch', fetch);
	const result = render(SettingsPage, { section: 'Connection', onBack: vi.fn() });
	result.container.style.height = '100vh';

	await expect.element(page.getByText('http://127.0.0.1:8081/v1', { exact: true })).toBeVisible();
	await expect.element(page.getByRole('button', { name: 'qwen2.5:0.5b' })).toBeVisible();
	await expect.element(page.getByText(/chat\/completions/).first()).toBeVisible();
	await page.getByRole('tab', { name: 'Python' }).click();
	await expect.element(page.getByText(/from openai import OpenAI/)).toBeVisible();

	await page.getByRole('checkbox', { name: 'Allow devices on my network' }).click();
	await expect.element(page.getByText('http://192.168.1.5:8081/v1', { exact: true })).toBeVisible();
	await expect.element(page.getByRole('button', { name: 'Copy key' })).toBeVisible();
	expect(settingsStore.config.apiKey).toBe('delta-test-key');
});

import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
	await page.route('**/v1/models', (route) =>
		route.fulfill({ json: { object: 'list', data: [] } })
	);
	await page.route(
		(url) => url.pathname === '/models',
		(route) => route.fulfill({ json: { models: [] } })
	);
});

// Exercise the production root layout, including the settings/server effects.
// Component tests alone do not mount this startup path.
for (const desktop of [false, true]) {
	test(`starts with model defaults in ${desktop ? 'desktop' : 'browser'} mode`, async ({
		page
	}) => {
		const errors: string[] = [];
		page.on('pageerror', (error) => errors.push(error.message));
		await page.addInitScript((desktop) => {
			localStorage.setItem(
				'config',
				JSON.stringify({
					onboardingCompleted: true,
					userName: 'Asha',
					top_p: 0.7
				})
			);
			localStorage.setItem('userOverrides', JSON.stringify(['top_p']));
			if (desktop) {
				Object.assign(window, {
					__TAURI_INTERNALS__: {
						metadata: { currentWindow: { label: 'main' }, currentWebview: { label: 'main' } },
						invoke: async (command: string) =>
							command === 'get_server_status' ? [8080, 8081, true, false, ''] : null,
						transformCallback: () => 1
					}
				});
			}
		}, desktop);
		await page.route('**/api/**', (route) =>
			route.fulfill({
				json: { models: [], downloads: [], events: [], reminders: [] }
			})
		);
		await page.route('**/api/v1/hardware/stream', (route) =>
			route.fulfill({ status: 204, contentType: 'text/event-stream', body: '' })
		);
		await page.route('**/slots', (route) => route.fulfill({ json: [] }));
		await page.route('**/props', (route) =>
			route.fulfill({
				json: {
					total_slots: 1,
					model_path: 'test.gguf',
					modalities: { vision: false, audio: false },
					default_generation_settings: { n_ctx: 4096, params: { top_p: 0.95, temperature: 0.4 } }
				}
			})
		);
		await page.goto('/');
		await expect(page.getByText('What are we working on?', { exact: true })).toBeVisible();
		await expect(page.locator('#app-loading')).toHaveCount(0);
		// Use the settings button in the collapsed sidebar after defaults arrive.
		await page.getByRole('button', { name: 'Settings', exact: true }).click();
		await expect(page.getByRole('main', { name: 'Settings' })).toBeVisible();
		await expect
			.poll(() =>
				page.evaluate(() => JSON.parse(localStorage.getItem('config') || '{}').temperature)
			)
			.toBe(0.4);
		const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('config')!));
		expect(saved.userName).toBe('Asha');
		expect(saved.top_p).toBe(0.7);
		expect(saved.temperature).toBe(0.4);
		expect(errors).toEqual([]);
	});
}

async function prepareOnboarding(page: import('@playwright/test').Page) {
	const errors: string[] = [];
	page.on('pageerror', (error) => errors.push(error.message));
	await page.addInitScript(() => {
		localStorage.setItem(
			'DeltaWebui.serverProps',
			JSON.stringify({
				total_slots: 1,
				model_path: 'test.gguf',
				modalities: { vision: false, audio: false },
				default_generation_settings: { n_ctx: 4096, params: { temperature: 0.6 } }
			})
		);
		Object.assign(window, {
			__TAURI_INTERNALS__: {
				metadata: { currentWindow: { label: 'main' }, currentWebview: { label: 'main' } },
				invoke: async (command: string) =>
					command === 'get_server_status' ? [8080, 8081, true, false, ''] : null,
				transformCallback: () => 1
			}
		});
	});
	await page.route('**/api/**', (route) =>
		route.fulfill({
			json: { models: [], downloads: [], events: [], reminders: [] }
		})
	);
	await page.route('**/api/v1/hardware/stream', (route) =>
		route.fulfill({ status: 204, contentType: 'text/event-stream', body: '' })
	);
	await page.route('**/slots', (route) => route.fulfill({ json: [] }));
	await page.route('**/props', (route) =>
		route.fulfill({
			json: {
				total_slots: 1,
				model_path: 'test.gguf',
				modalities: { vision: false, audio: false },
				default_generation_settings: { n_ctx: 4096, params: { temperature: 0.4 } }
			}
		})
	);
	await page.goto('/');
	await expect
		.poll(() => page.evaluate(() => JSON.parse(localStorage.getItem('config') || '{}').temperature))
		.toBe(0.4);
	return errors;
}

test('Skip for now dismisses onboarding after cached model defaults are refreshed', async ({
	page
}) => {
	const errors = await prepareOnboarding(page);
	const dialog = page.getByRole('dialog');
	await dialog.getByRole('button', { name: 'Skip for now', exact: true }).click();
	await expect(dialog).toHaveCount(0);
	await expect
		.poll(() =>
			page.evaluate(() => JSON.parse(localStorage.getItem('config')!).onboardingCompleted)
		)
		.toBe(true);
	await expect(page.getByText('How can I help you today?', { exact: true })).toBeVisible();
	expect(errors).toEqual([]);
});

test('Continue advances onboarding and finishing saves the selected preferences', async ({
	page
}) => {
	const errors = await prepareOnboarding(page);
	const dialog = page.getByRole('dialog');
	await dialog.getByLabel('Your name', { exact: true }).fill('Asha');
	await dialog.getByRole('button', { name: 'Continue', exact: true }).click();
	await expect(dialog.getByText('Step 2 of 3', { exact: true })).toBeVisible();
	await dialog.getByRole('button', { name: /^Short/ }).click();
	await dialog.getByRole('button', { name: 'Continue', exact: true }).click();
	await expect(dialog.getByText('Step 3 of 3', { exact: true })).toBeVisible();
	await dialog.getByRole('button', { name: 'Sunday', exact: true }).click();
	await dialog.getByRole('button', { name: 'Start using Delta', exact: true }).click();
	await expect(dialog).toHaveCount(0);
	await expect
		.poll(() =>
			page.evaluate(() => JSON.parse(localStorage.getItem('config')!).onboardingCompleted)
		)
		.toBe(true);
	const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('config')!));
	expect(saved).toMatchObject({
		userName: 'Asha',
		replyStyle: 'concise',
		calendarWeekStart: 'sunday'
	});
	await expect(page.getByText('What are we working on?', { exact: true })).toBeVisible();
	expect(errors).toEqual([]);
});

for (const fault of ['backend', 'bundle'] as const) {
	test(`reviews a ${fault} failure before opening a public issue`, async ({ page }) => {
		await page.addInitScript(() => {
			Object.assign(window, {
				__openedIssue: null,
				__TAURI_INTERNALS__: {
					metadata: { currentWindow: { label: 'main' }, currentWebview: { label: 'main' } },
					transformCallback: () => 1,
					invoke: async (command: string, args: any) => {
						if (command === 'get_server_status') return [8080, 8081, false, true, 'Backend exited with signal 4'];
						if (command === 'get_diagnostic_report') return {
							version: '1.0.18-rc.1', target: 'x86_64-apple-darwin', cpuPolicy: 'x86_64-baseline',
							os: 'macOS 12', stage: args.stage, exitCode: 132,
							message: 'Failed /Users/private-name/models/test.gguf hf_abcdefghijklmnop', logs: []
						};
						if (command === 'plugin:shell|open') (window as any).__openedIssue = args.path;
						return null;
					}
				}
			});
		});
		// The reporting fallback must also work without Svelte or any app modules.
		if (fault === 'bundle') await page.route('**/_app/immutable/**', route => route.abort());
		await page.goto('/');
		await page.getByRole('button', { name: 'Review report', exact: true }).first().click();
		const report = page.getByRole('textbox', { name: 'Diagnostic report contents' });
		await expect(report).toHaveValue(/x86_64-baseline/);
		expect(await report.inputValue()).not.toMatch(/private-name|abcdefghijklmnop/);
		expect(await page.evaluate(() => (window as any).__openedIssue)).toBeNull();
		const download = page.waitForEvent('download');
		await page.getByRole('button', { name: 'Download report', exact: true }).click();
		expect((await download).suggestedFilename()).toBe('delta-diagnostic-report.txt');
		await page.getByRole('button', { name: 'Open GitHub issue', exact: true }).click();
		await expect.poll(() => page.evaluate(() => (window as any).__openedIssue)).toMatch(/^https:\/\/github.com\/nile-agi\/delta-release\/issues\/new\?/);
		expect((await page.evaluate(() => (window as any).__openedIssue)).length).toBeLessThanOrEqual(1800);
	});
}

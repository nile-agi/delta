import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
	await page.addInitScript(() => {
		localStorage.setItem('mode-watcher-mode', 'light');
		if (!localStorage.getItem('config')) {
			localStorage.setItem(
				'config',
				JSON.stringify({
					onboardingCompleted: true,
					userName: 'Original',
					theme: 'light',
					alwaysShowSidebar: true
				})
			);
		}
	});
	await page.route('**/props', (route) =>
		route.fulfill({ json: { total_slots: 0, default_generation_settings: { params: {} } } })
	);
	await page.route('**/api/models/available', (route) => route.fulfill({ json: { models: [] } }));
	await page.route('**/api/models/list', (route) => route.fulfill({ json: { models: [] } }));
	await page.route('**/api/models/downloads', (route) =>
		route.fulfill({ json: { downloads: [] } })
	);
	await page.route('**/api/system/ram', (route) =>
		route.fulfill({ json: { total_ram_gb: 16, total_ram_bytes: 17179869184 } })
	);
	await page.route('**/api/v1/dhats/block-status', (route) =>
		route.fulfill({ json: { blocked: false } })
	);
	await page.route('**/api/agent/**', (route) =>
		route.fulfill({ json: { events: [], reminders: [] } })
	);
});

test('Settings opens as the whole app view and Back returns Home', async ({ page }) => {
	await page.goto('/');
	await page.getByRole('button', { name: 'Settings', exact: true }).click();
	await expect(page).toHaveURL(/#\/settings$/);
	const settings = page.getByRole('main', { name: 'Settings' });
	await expect(settings).toBeVisible();
	const viewport = page.viewportSize()!;
	const box = (await settings.boundingBox())!;
	expect(box.x).toBe(0);
	expect(box.y).toBe(0);
	expect(box.width).toBe(viewport.width);
	expect(box.height).toBe(viewport.height);
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	await expect(page).toHaveURL(/#\/$/);
	await expect(settings).toHaveCount(0);
});

for (const width of [1280, 390]) {
	test(`Settings is reachable with the drawer closed at ${width}px`, async ({ page }) => {
		await page.setViewportSize({ width, height: 720 });
		await page.addInitScript(() => {
			const config = JSON.parse(localStorage.getItem('config')!);
			localStorage.setItem('config', JSON.stringify({ ...config, alwaysShowSidebar: false }));
		});
		await page.goto('/');
		const settings = page.getByRole('button', { name: 'Open Settings', exact: true });
		await expect(settings).toBeInViewport();
		await settings.click();
		await expect(page.getByRole('main', { name: 'Settings' })).toBeVisible();
		await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
		await expect(settings).toBeInViewport();
		await page.getByRole('button', { name: 'Toggle Sidebar', exact: true }).click();
		await expect(settings).toHaveCount(0);
		await page.getByRole('button', { name: 'Settings', exact: true }).click();
		await expect(page.getByRole('main', { name: 'Settings' })).toBeVisible();
	});
}

test('Save persists settings while Back discards the next draft', async ({ page }) => {
	await page.goto('/#/settings');
	await page.getByRole('button', { name: 'You', exact: true }).click();
	const name = page.getByLabel('What should Delta call you?');
	await name.fill('Saved name');
	await page.getByRole('button', { name: 'Save settings', exact: true }).click();
	await expect(page).toHaveURL(/#\/settings$/);
	await name.fill('Discard this draft');
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	await page.getByRole('button', { name: 'Settings', exact: true }).click();
	await page.getByRole('button', { name: 'You', exact: true }).click();
	await expect(name).toHaveValue('Saved name');
});

test('enabling agent tools selects the core tools and preserves computer access choices', async ({
	page
}) => {
	await page.addInitScript(() => {
		const config = JSON.parse(localStorage.getItem('config')!);
		localStorage.setItem(
			'config',
			JSON.stringify({
				...config,
				useAgentTools: false,
				useCalendarTools: false,
				useNotesTools: false,
				useMemoryTools: false,
				useTaskTools: false,
				useFileTools: false,
				useShellTools: false,
				useWebTools: true
			})
		);
	});
	await page.goto('/?section=Agent%20tools#/settings');
	const master = page.getByRole('checkbox', { name: 'Enable agent tools', exact: true });
	const calendar = page.getByRole('checkbox', { name: 'Calendar and tasks', exact: true });
	await expect(calendar).toBeDisabled();
	await master.check();
	for (const name of [
		'Calendar and tasks',
		'Notes',
		'Planning and self-tracking',
		'Long-term memory'
	]) {
		await expect(page.getByRole('checkbox', { name, exact: true })).toBeChecked();
	}
	await expect(
		page.getByRole('checkbox', { name: 'Read and write files', exact: true })
	).not.toBeChecked();
	await expect(
		page.getByRole('checkbox', { name: 'Run shell commands', exact: true })
	).not.toBeChecked();
	await expect(page.getByRole('checkbox', { name: 'Fetch web pages', exact: true })).toHaveCount(0);
	await calendar.uncheck();
	await page.getByRole('button', { name: 'Save settings', exact: true }).click();
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	await page.getByRole('button', { name: 'Settings', exact: true }).click();
	await page.getByRole('button', { name: 'Agent tools', exact: true }).click();
	await expect(master).toBeChecked();
	await expect(calendar).not.toBeChecked();
	await master.uncheck();
	await expect(calendar).toBeDisabled();
	await master.check();
	await expect(calendar).toBeChecked();
});

test('a narrow settings page supports direct model management navigation', async ({ page }) => {
	await page.setViewportSize({ width: 390, height: 720 });
	await page.goto('/?section=Model%20Management#/settings');
	await expect(
		page.getByText('Manage your installed models and download new ones.', { exact: false })
	).toBeVisible();
	await expect(page.getByRole('button', { name: 'Back to Home', exact: true })).toBeVisible();
	await expect(page.getByRole('button', { name: 'Save settings', exact: true })).toBeInViewport();
	expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBe(390);
});

test('Back restores an unsaved theme preview', async ({ page }) => {
	await page.goto('/#/settings');
	await expect(page.locator('html')).not.toHaveClass(/dark/);
	await page.getByRole('button', { name: 'Light', exact: true }).click();
	await page.getByRole('option', { name: 'Dark', exact: true }).click();
	await expect(page.locator('html')).toHaveClass(/dark/);
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	await expect(page.locator('html')).not.toHaveClass(/dark/);
});

test('Back clears the section link before returning Home', async ({ page }) => {
	await page.goto('/?section=Model%20Management#/settings');
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	await expect(page).toHaveURL(/\/#\/$/);
	await page.getByRole('button', { name: 'Settings', exact: true }).click();
	await expect(page.getByRole('heading', { name: 'General', exact: true })).toBeVisible();
});

test('download progress continues after visiting Model Management and returning Home', async ({
	page
}) => {
	let progress = 15;
	await page.route('**/api/models/downloads', (route) =>
		route.fulfill({
			json: {
				downloads: [
					{
						model: 'qwen3:0.6b',
						progress,
						current_bytes: progress * 100,
						total_bytes: 10000
					}
				]
			}
		})
	);
	await page.goto('/');
	const download = page.getByRole('button', { name: /Downloading .*Open model management/ });
	await expect(download).toHaveAccessibleName(/15 percent/);
	await download.click();
	await expect(
		page.getByText('Manage your installed models and download new ones.', { exact: false })
	).toBeVisible();
	await page.getByRole('button', { name: 'Back to Home', exact: true }).click();
	progress = 65;
	await expect(download).toHaveAccessibleName(/65 percent/);
});

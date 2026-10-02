import { expect, test } from '@playwright/test';

test('standalone calendar and task form load without the startup overlay', async ({ page }) => {
	await page.route('**/api/agent/events?*', route => route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ events: [] }) }));
	await page.goto('/?window=calendar');
	await expect(page.getByRole('heading', { name: /^(January|February|March|April|May|June|July|August|September|October|November|December) \d{4}$/ })).toBeVisible();
	await expect(page.locator('#app-loading')).toHaveCount(0);
	await page.getByRole('button', { name: 'New', exact: true }).click();
	await expect(page.getByRole('dialog')).toBeVisible();
	await page.getByRole('button', { name: 'Task', exact: true }).click();
	await expect(page.getByPlaceholder('What needs doing?')).toBeVisible();
});

test('standalone windows never paint the startup splash, even before the app loads', async ({ page }) => {
	await page.route('**/api/agent/events?*', route => route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ events: [] }) }));
	// Recorded as soon as the HTML is parsed, before any app script has had a chance to remove it.
	await page.addInitScript(() => {
		document.addEventListener('DOMContentLoaded', () => {
			const splash = document.getElementById('app-loading');
			(window as unknown as { splashDisplay: string }).splashDisplay = splash ? getComputedStyle(splash).display : 'absent';
		});
	});
	const splashAt = async (url: string) => {
		await page.goto(url);
		return page.evaluate(() => (window as unknown as { splashDisplay: string }).splashDisplay);
	};
	expect(['none', 'absent']).toContain(await splashAt('/?window=calendar'));
	expect(['none', 'absent']).toContain(await splashAt('/?window=notes'));
	// The main window still gets it while the model server starts.
	expect(await splashAt('/')).toBe('flex');
});

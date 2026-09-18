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

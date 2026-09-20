import { test, expect } from '@playwright/test';

test.beforeEach(async ({ page }) => {
	await page.addInitScript(() => {
		if (localStorage.getItem('delta_notes')) return;
		const createdAt = new Date('2025-01-15T10:00:00Z').getTime();
		localStorage.setItem('delta_notes', JSON.stringify(Array.from({ length: 200 }, (_, i) => ({
			id: `note-${i}`, title: `Note ${i}`, content: i === 0 ? '<p>First note body</p>'.repeat(120) : `<p>Body ${i}</p>`,
			createdAt, updatedAt: createdAt - i, pinned: false, color: null, emoji: null, tags: []
		}))));
		localStorage.setItem('delta_active_note', 'note-0');
	});
	await page.goto('/?window=notes');
	await expect(page.getByRole('textbox', { name: 'Note editor', exact: true })).toBeVisible();
});

test('scrolls to the last of 200 notes without moving the editor or changing dates', async ({ page }) => {
	const list = page.getByRole('region', { name: 'Scrollable notes', exact: true });
	await list.hover();
	await page.mouse.wheel(0, 30000);
	await expect(page.getByRole('button', { name: 'Open Note 199', exact: true })).toBeInViewport();
	const scroll = await list.evaluate(el => el.scrollTop);
	expect(scroll).toBeGreaterThan(0);
	expect(await page.locator('.editor-scroll').evaluate(el => el.scrollTop)).toBe(0);
	await page.getByRole('button', { name: 'Open Note 199', exact: true }).click();
	await expect(page.getByRole('textbox', { name: 'Note editor', exact: true })).toHaveText('Body 199');
	await page.waitForTimeout(650);
	expect(await list.evaluate(el => el.scrollTop)).toBe(scroll);
	const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('delta_notes')!).find((n: {id: string}) => n.id === 'note-199'));
	expect(saved.createdAt).toBe(new Date('2025-01-15T10:00:00Z').getTime());
	expect(saved.updatedAt).toBe(saved.createdAt - 199);
});

test('dragging follows the pointer, clamps, persists and resets', async ({ page }) => {
	const folder = page.getByRole('complementary', { name: 'Note folders' });
	const list = page.getByRole('region', { name: 'Notes list', exact: true });
	const editor = page.getByRole('region', { name: 'Selected note' });
	const divider = page.getByRole('separator', { name: 'Resize folders and notes' });
	const before = { folder: (await folder.boundingBox())!.width, list: (await list.boundingBox())!.width, editor: (await editor.boundingBox())!.width };
	const box = (await divider.boundingBox())!;
	await page.mouse.move(box.x + box.width / 2, box.y + 200);
	await page.mouse.down();
	await page.mouse.move(box.x + box.width / 2 + 50, box.y + 250, { steps: 10 });
	expect((await folder.boundingBox())!.width).toBeCloseTo(before.folder + 50, 0);
	expect((await list.boundingBox())!.width).toBeCloseTo(before.list - 50, 0);
	expect((await editor.boundingBox())!.width).toBeCloseTo(before.editor, 0);
	await page.mouse.move(1190, 300);
	await page.mouse.up();
	expect((await list.boundingBox())!.width).toBeCloseTo(220, 0);
	expect(await page.evaluate(() => document.body.style.getPropertyValue('user-select'))).toBe('');
	const savedWidth = (await folder.boundingBox())!.width;
	await page.reload();
	await expect(divider).toBeVisible();
	expect((await folder.boundingBox())!.width).toBeCloseTo(savedWidth, 0);
	await divider.dblclick();
	expect((await folder.boundingBox())!.width).toBeCloseTo(before.folder, 0);
	await divider.focus(); await page.keyboard.press('ArrowRight');
	expect((await folder.boundingBox())!.width).toBeCloseTo(before.folder + 10, 0);
});

test('adapts to a narrow workspace and restores desktop panes', async ({ page }) => {
	await page.setViewportSize({ width: 500, height: 650 });
	await page.getByRole('button', { name: 'Back to notes', exact: false }).click();
	await expect(page.getByRole('region', { name: 'Notes list', exact: true })).toBeVisible();
	await page.getByRole('button', { name: 'Show folders' }).click();
	await expect(page.getByRole('complementary', { name: 'Note folders' })).toBeVisible();
	await page.getByRole('button', { name: 'All Notes', exact: false }).click();
	await page.getByRole('button', { name: 'Open Note 1', exact: true }).click();
	await expect(page.getByRole('textbox', { name: 'Note editor', exact: true })).toHaveText('Body 1');
	expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBe(500);
	await page.setViewportSize({ width: 1200, height: 800 });
	await expect(page.getByRole('separator')).toHaveCount(2);
	await expect(page.getByRole('complementary', { name: 'Note folders' })).toBeVisible();
});

test('formatting and body emoji survive closing and reopening', async ({ page }) => {
	const body = page.getByRole('textbox', { name: 'Note editor', exact: true });
	await body.fill('Keep this');
	await body.press('ControlOrMeta+a');
	await page.getByRole('button', { name: 'Bold', exact: true }).click();
	await expect(body.locator('strong')).toHaveText('Keep this');
	await body.press('ArrowRight');
	await page.getByRole('textbox', { name: 'Note title', exact: true }).click();
	await page.getByRole('button', { name: 'Add emoji', exact: true }).click();
	await page.getByRole('button', { name: '💡', exact: true }).click();
	await expect(body).toContainText('💡');
	await page.getByRole('button', { name: 'Open Note 1', exact: true }).click();
	await page.getByRole('button', { name: 'Open Note 0', exact: true }).click();
	await expect(body).toContainText('Keep this💡');
	await expect(body.locator('strong')).toContainText('Keep this');
});

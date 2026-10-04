import '../../../../app.css';
import { afterEach, expect, it } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import OnboardingDialog from './OnboardingDialog.svelte';
afterEach(() => cleanup());
it('shows clear progress and selected preferences through all three steps', async () => {
	render(OnboardingDialog);
	await expect.element(page.getByText('Step 1 of 3')).toBeVisible();
	await page.getByLabelText('Your name', { exact: true }).fill('Alex');
	await expect
		.element(page.getByRole('heading', { name: 'What should I call you?' }))
		.toBeVisible();
	await page.getByRole('button', { name: 'Continue', exact: true }).click();
	await expect.element(page.getByText('Step 2 of 3')).toBeVisible();
	await expect
		.element(page.getByRole('button', { name: /Balanced/ }))
		.toHaveAttribute('aria-pressed', 'true');
	await page.getByRole('button', { name: /Short/ }).click();
	await expect
		.element(page.getByRole('button', { name: /Short/ }))
		.toHaveAttribute('aria-pressed', 'true');
	await page.getByRole('button', { name: 'Continue', exact: true }).click();
	await expect.element(page.getByText('Step 3 of 3')).toBeVisible();
	await page.getByRole('button', { name: 'Sunday', exact: true }).click();
	await expect
		.element(page.getByRole('button', { name: 'Sunday', exact: true }))
		.toHaveAttribute('aria-pressed', 'true');
});
it('fits the name step on a small screen', async () => {
	await page.viewport(390, 844);
	render(OnboardingDialog);
	await expect.element(page.getByRole('button', { name: 'Continue', exact: true })).toBeVisible();
	expect(document.documentElement.scrollWidth).toBeLessThanOrEqual(390);
	await page.screenshot({ path: '/tmp/delta-onboarding-mobile.png' });
	await page.viewport(1280, 800);
});

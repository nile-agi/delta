import '../../../../../app.css';
import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import ChatForm from './test-fixtures/ChatFormHost.svelte';
import { settingsStore } from '$lib/stores/settings.svelte';
import { SETTING_CONFIG_DEFAULT } from '$lib/constants/settings-config';

afterEach(() => {
	cleanup();
	vi.unstubAllGlobals();
	settingsStore.config = { ...SETTING_CONFIG_DEFAULT };
});

it('keeps Send visible when the model cannot accept audio', async () => {
	render(ChatForm);
	await expect.element(page.getByRole('button', { name: 'Send', exact: true })).toBeVisible();
});

it('keeps long pasted text in the composer when attachment conversion is disabled', async () => {
	settingsStore.config = { ...SETTING_CONFIG_DEFAULT, pasteLongTextToFileLen: 0 };
	const onFileUpload = vi.fn();
	const result = render(ChatForm, { onFileUpload });
	const clipboardData = new DataTransfer();
	clipboardData.setData('text/plain', 'a'.repeat(3000));
	const paste = new ClipboardEvent('paste', { clipboardData, bubbles: true, cancelable: true });
	result.container.querySelector('textarea')!.dispatchEvent(paste);
	expect(paste.defaultPrevented).toBe(false);
	expect(onFileUpload).not.toHaveBeenCalled();
});

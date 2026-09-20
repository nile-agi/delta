import { defineConfig } from '@playwright/test';

export default defineConfig({
	testDir: 'e2e',
	testMatch: ['notes.test.ts', 'calendar.test.ts'],
	fullyParallel: true,
	workers: 2,
	timeout: 45000,
	expect: { timeout: 15000 },
	use: { baseURL: 'http://127.0.0.1:5200', viewport: { width: 1200, height: 800 } },
	projects: [
		{ name: 'chromium', use: { browserName: 'chromium' } },
		{ name: 'webkit', use: { browserName: 'webkit' } }
	],
	webServer: {
		command: 'pnpm build && pnpm exec http-server ../../public -a 127.0.0.1 -p 5200',
		url: 'http://127.0.0.1:5200',
		reuseExistingServer: !process.env.CI,
		timeout: 120000
	}
});

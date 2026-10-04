import { describe, expect, it } from 'vitest';
import {
	validateGenerationSettings,
	repairGenerationSettings,
	parseCustomParameters
} from './generation-settings';
import { SETTING_CONFIG_DEFAULT } from '$lib/constants/settings-config';

describe('generation settings', () => {
	it.each([-1, 1.5, '', Infinity, NaN, 2147483648])(
		'rejects invalid repetition lookback %s',
		(value) => {
			expect(
				validateGenerationSettings({ dry_penalty_last_n: value }).dry_penalty_last_n
			).toBeTruthy();
		}
	);
	it.each([0, 64, 2147483647])('accepts supported lookback %s', (value) => {
		expect(validateGenerationSettings({ dry_penalty_last_n: value })).toEqual({});
	});
	it('validates probabilities and preserves signed penalties and reply sentinel', () => {
		expect(validateGenerationSettings({ top_p: -1, min_p: 2, repeat_last_n: -1 })).toHaveProperty(
			'top_p'
		);
		expect(
			validateGenerationSettings({ presence_penalty: -1, frequency_penalty: -1, max_tokens: -1 })
		).toEqual({});
	});
	it('repairs invalid values only, using valid model defaults', () => {
		const result = repairGenerationSettings(
			{ dry_penalty_last_n: -1, top_p: 0.7, dry_base: 0 },
			{ dry_penalty_last_n: 128, dry_base: -1 }
		);
		expect(result.values).toMatchObject({ dry_penalty_last_n: 128, top_p: 0.7, dry_base: 1.75 });
		expect(result.repaired).toEqual(['dry_penalty_last_n', 'dry_base']);
	});
	it('has a valid disabled DRY fallback', () => {
		expect(SETTING_CONFIG_DEFAULT.dry_multiplier).toBe(0);
		expect(SETTING_CONFIG_DEFAULT.dry_penalty_last_n).toBe(64);
		expect(validateGenerationSettings(SETTING_CONFIG_DEFAULT)).toEqual({});
	});
	it('requires an object and validates custom aliases', () => {
		expect(() => parseCustomParameters('[]')).toThrow();
		expect(() => parseCustomParameters('{"dry_penalty_last_n":-1}')).toThrow(/lookback/i);
		expect(() => parseCustomParameters('{"typical_p":2}')).toThrow();
		expect(() => parseCustomParameters('{"n_predict":-2}')).toThrow();
		expect(parseCustomParameters('{"seed":123,"max_tokens":-1}')).toEqual({
			seed: 123,
			max_tokens: -1
		});
	});
	it('normalizes known numeric JSON strings while preserving extension fields', () => {
		expect(parseCustomParameters('{"temperature":"0.7","n_predict":"32","seed":123}')).toEqual({
			temperature: 0.7,
			n_predict: 32,
			seed: 123
		});
	});
});

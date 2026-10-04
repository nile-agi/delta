import { SETTING_CONFIG_DEFAULT } from '$lib/constants/settings-config';

const INT_MAX = 2147483647;
export interface NumericSettingRule {
	label: string;
	min?: number;
	max?: number;
	integer?: boolean;
	step?: number;
}
const count = (label: string, min = 0): NumericSettingRule => ({
	label,
	min,
	max: INT_MAX,
	integer: true,
	step: 1
});
const probability = (label: string): NumericSettingRule => ({ label, min: 0, max: 1, step: 0.01 });
export const NUMERIC_SETTING_RULES: Record<string, NumericSettingRule> = {
	temperature: { label: 'Response variation', min: 0, step: 0.1 },
	top_k: count('Candidate word limit'),
	top_p: probability('Candidate probability coverage'),
	min_p: probability('Minimum relative probability'),
	typ_p: probability('Typical probability coverage'),
	xtc_probability: probability('Alternative choice chance'),
	xtc_threshold: probability('Alternative choice threshold'),
	dynatemp_range: { label: 'Variation adjustment range', min: 0, step: 0.1 },
	dynatemp_exponent: { label: 'Variation adjustment curve', min: 0, step: 0.1 },
	repeat_last_n: count('Repetition check window'),
	repeat_penalty: { label: 'Repeated word penalty', min: 0, step: 0.1 },
	presence_penalty: { label: 'Previously used word penalty', step: 0.1 },
	frequency_penalty: { label: 'Frequently used word penalty', step: 0.1 },
	dry_multiplier: { label: 'Repeated phrase penalty', min: 0, step: 0.1 },
	dry_base: { label: 'Repeated phrase penalty growth', min: 1, step: 0.05 },
	dry_allowed_length: count('Allowed repeated phrase length'),
	dry_penalty_last_n: count('Repetition lookback'),
	max_tokens: count('Reply length', -1),
	pasteLongTextToFileLen: count('Long text attachment threshold')
};
const ALIASES: Record<string, string> = {
	typical_p: 'typ_p',
	n_predict: 'max_tokens',
	max_completion_tokens: 'max_tokens'
};
const SAMPLERS = new Set([
	'penalties',
	'dry',
	'top_n_sigma',
	'top_k',
	'typ_p',
	'top_p',
	'min_p',
	'xtc',
	'temperature',
	'infill',
	'mirostat',
	'adaptive_p'
]);

export function validateNumericSetting(key: string, value: unknown): string | undefined {
	const rule = NUMERIC_SETTING_RULES[key];
	if (!rule) return;
	// Do not let Number(''), booleans, or null become valid zeroes.
	const number =
		typeof value === 'number'
			? value
			: typeof value === 'string' && value.trim()
				? Number(value)
				: NaN;
	if (!Number.isFinite(number)) return `${rule.label}: enter a number.`;
	if (rule.integer && !Number.isInteger(number)) return `${rule.label}: enter a whole number.`;
	if (rule.min !== undefined && number < rule.min)
		return `${rule.label}: use ${rule.min} or greater.`;
	if (rule.max !== undefined && number > rule.max) return `${rule.label}: use ${rule.max} or less.`;
}

export function validateGenerationSettings(
	values: Record<string, unknown>
): Record<string, string> {
	const errors: Record<string, string> = {};
	for (const [key, value] of Object.entries(values)) {
		const canonical = ALIASES[key] ?? key;
		const error = validateNumericSetting(canonical, value);
		if (error) errors[key] = error;
		if (key === 'samplers') {
			const names =
				typeof value === 'string'
					? value
							.split(';')
							.map((s) => s.trim())
							.filter(Boolean)
					: value;
			if (
				!Array.isArray(names) ||
				!names.length ||
				names.some((s) => typeof s !== 'string' || !SAMPLERS.has(s))
			)
				errors[key] = 'Sampling order: choose supported sampling methods.';
		}
	}
	return errors;
}

export function parseCustomParameters(raw: unknown): Record<string, unknown> {
	if (raw === undefined || raw === '') return {};
	let value: unknown;
	try {
		value = typeof raw === 'string' ? JSON.parse(raw) : raw;
	} catch {
		throw new Error('Extra request settings: enter valid JSON.');
	}
	if (!value || typeof value !== 'object' || Array.isArray(value))
		throw new Error('Extra request settings: use a JSON object.');
	const errors = validateGenerationSettings(value as Record<string, unknown>);
	if (Object.keys(errors).length) throw new Error(Object.values(errors)[0]);
	return value as Record<string, unknown>;
}

export function repairGenerationSettings<T extends Record<string, unknown>>(
	values: T,
	modelDefaults: Record<string, unknown> = {}
): { values: T; repaired: string[] } {
	const repaired: string[] = [];
	const next = { ...values };
	for (const key of Object.keys(validateGenerationSettings(values))) {
		const candidate = modelDefaults[key];
		const valid = candidate !== undefined && !validateGenerationSettings({ [key]: candidate })[key];
		Object.assign(next, { [key]: valid ? candidate : SETTING_CONFIG_DEFAULT[key] });
		repaired.push(key);
	}
	if ('custom' in next) {
		try {
			parseCustomParameters(next.custom);
		} catch {
			// Preserve extension fields and valid overrides; remove only invalid known values.
			let custom: unknown;
			try {
				custom = JSON.parse(String(next.custom));
			} catch {
				custom = null;
			}
			if (custom && typeof custom === 'object' && !Array.isArray(custom)) {
				for (const key of Object.keys(
					validateGenerationSettings(custom as Record<string, unknown>)
				))
					delete (custom as Record<string, unknown>)[key];
				Object.assign(next, { custom: JSON.stringify(custom) });
			} else Object.assign(next, { custom: '' });
			repaired.push('custom');
		}
	}
	return { values: next, repaired };
}

export const SETTINGS_VIEWS = [
	{ title: 'Info', description: 'About Delta and where to get help.', groups: [] },
	{
		title: 'General',
		description: 'Make Delta comfortable to use, and choose what it can do for you.',
		groups: ['You', 'Agent tools', 'Model Management', 'Import/Export']
	},
	{
		title: 'Developer',
		description:
			'Fine-tune responses and inspect how Delta works. Model defaults are a good starting point.',
		groups: ['Sampling', 'Penalties', 'Connection', 'Developer']
	}
] as const;
export const GROUP_LABELS: Record<string, string> = {
	You: 'Personal preferences',
	Display: 'Appearance',
	'Agent tools': 'Assistant capabilities',
	'Model Management': 'Models',
	'Import/Export': 'Your data',
	Sampling: 'Response controls',
	Penalties: 'Repetition controls',
	Connection: 'API access',
	Developer: 'Diagnostics & extra settings'
};
export function resolveSettingsLocation(section = 'General'): { view: string; group: string } {
	if (section === 'Info') return { view: 'Info', group: 'You' };
	if (section === 'General' || section === 'Display') return { view: 'General', group: 'You' };
	if (section === 'Developer') return { view: 'Developer', group: 'Sampling' };
	const view = SETTINGS_VIEWS.find((view) => (view.groups as readonly string[]).includes(section));
	return view ? { view: view.title, group: section } : { view: 'General', group: 'You' };
}
export const FIELD_LABELS: Record<string, string> = {
	userName: 'What should Delta call you?',
	replyStyle: 'How much detail should replies include?',
	calendarWeekStart: 'First day of the week',
	systemMessage: 'How Delta should respond',
	askForTitleConfirmation: 'Ask before renaming a chat',
	pasteLongTextToFileLen: 'Turn long pasted text into an attachment',
	useAgentTools: 'Let Delta use assistant tools',
	useCalendarTools: 'Manage my calendar and tasks',
	useNotesTools: 'Read and update my notes',
	useMemoryTools: 'Remember things about me',
	useTaskTools: 'Plan work with multiple steps',
	useFileTools: 'Read and change files',
	useShellTools: 'Run commands',
	resetToolPolicies: 'Saved permission choices',
	backendSampling: 'Use device-accelerated sampling',
	samplers: 'Sampling order',
	temperature: 'Response variation',
	dynatemp_range: 'Variation adjustment range',
	dynatemp_exponent: 'Variation adjustment curve',
	top_k: 'Candidate word limit',
	top_p: 'Candidate probability coverage',
	min_p: 'Minimum relative probability',
	typ_p: 'Typical probability coverage',
	xtc_probability: 'Alternative choice chance',
	xtc_threshold: 'Alternative choice threshold',
	repeat_last_n: 'Repetition check window (tokens)',
	repeat_penalty: 'Repeated word penalty',
	presence_penalty: 'Previously used word penalty',
	frequency_penalty: 'Frequently used word penalty',
	dry_multiplier: 'Repeated phrase penalty',
	dry_base: 'Repeated phrase penalty growth',
	dry_allowed_length: 'Allowed repeated phrase length (tokens)',
	dry_penalty_last_n: 'Repetition lookback (tokens)',
	max_tokens: 'Reply length',
	showToolCallLabels: 'Show tool activity details',
	disableReasoningFormat: 'Show unformatted model output',
	custom: 'Extra request settings (JSON)',
	apiKey: 'API access key'
};
export const FIELD_HELP: Record<string, string> = {
	theme: 'Choose light, dark, or follow your device.',
	userName: 'Use any name you like, or leave this blank.',
	replyStyle: 'Choose shorter answers or more explanation.',
	calendarWeekStart: 'Choose the day shown first in your calendar.',
	systemMessage:
		'Give Delta instructions about tone, language, or how you want it to help. Leave empty to use its normal behavior.',
	pasteLongTextToFileLen:
		'Text longer than this number of characters becomes an attachment. Set to 0 to turn this off.',
	useAgentTools:
		'Allow Delta to work with your calendar, notes, memory, and plans. Choose individual capabilities below.',
	useCalendarTools: 'Let Delta find and update your appointments, reminders, and to-do items.',
	useNotesTools: 'Let Delta find, read, create, and update your saved notes.',
	useMemoryTools:
		'Let Delta save useful facts and preferences and recall them in later conversations.',
	useTaskTools:
		'Let Delta keep a private plan and working notes while completing a longer request.',
	useFileTools:
		'Allow file access in your home and temporary folders. Credential files remain protected.',
	useShellTools: 'Let Delta run commands on your device. Permission checks still apply.',
	resetToolPolicies:
		'Forget saved Always or Never answers so Delta asks again when permission is needed.',
	backendSampling:
		'Move supported sampling work to the model backend. Availability depends on your device and model.',
	temperature:
		'Lower values make word choices more predictable; higher values allow more variation. 0 chooses the most likely next token.',
	samplers:
		'The order of word-selection filters. Use supported names separated by semicolons. Keep the model default unless you need a specific order.',
	top_k: 'Keep this many possible next tokens. Set to 0 to turn this filter off.',
	top_p:
		'Keep choices covering this share of the total probability. Use a value from 0 to 1; 1 turns this filter off.',
	min_p: 'Drop choices much less likely than the best choice. Use 0 to 1; 0 turns this filter off.',
	typ_p:
		'Control how closely word choices follow the expected distribution. Use 0 to 1; 1 turns this filter off.',
	dynatemp_range:
		'Allow response variation to change within this range. Set to 0 to turn this adjustment off.',
	dynatemp_exponent:
		'Change how sharply the variation adjustment responds. Keep the model default if unsure.',
	xtc_probability:
		'Chance of filtering out very likely choices to allow alternatives. Use 0 to 1; 0 turns it off.',
	xtc_threshold:
		'Probability threshold for filtering likely choices. Values above 0.5 disable this filter.',
	repeat_last_n:
		'How many recent tokens to check for repeated words. Set to 0 to disable this check.',
	repeat_penalty: 'Discourage words already used. 1 leaves their likelihood unchanged.',
	presence_penalty:
		'Adjust choices based on whether they have appeared before. 0 has no effect; negative values encourage reuse.',
	frequency_penalty:
		'Adjust choices based on how often they have appeared. 0 has no effect; negative values encourage reuse.',
	dry_multiplier: 'Discourage repeated phrases. Set to 0 to turn this control off.',
	dry_base: 'How quickly the penalty grows as a phrase repeats. Must be at least 1.',
	dry_allowed_length: 'How many tokens may repeat before the phrase penalty starts.',
	dry_penalty_last_n:
		'How many recent tokens the repeated-phrase control checks. Set to 0 to disable it. Negative values are not supported.',
	max_tokens:
		'Limit each reply by token count. No fixed limit still respects model context and agent runtime limits.',
	apiKey: 'Leave blank unless your server requires an access key.',
	custom:
		'Optional. Leave blank to use the response controls. Enter a JSON object to override specific request options.',
	showToolCallLabels: 'Show tool names and their data below replies for troubleshooting.',
	disableReasoningFormat: 'Show the model response without normal formatting, for troubleshooting.'
};

export const FIELD_PLACEHOLDERS: Record<string, string> = {
	userName: 'e.g. Asha',
	systemMessage: 'e.g. Use simple language, keep replies short, and explain unfamiliar words.',
	apiKey: 'Paste the access key provided by your server',
	samplers: 'e.g. top_k;typ_p;top_p;min_p;temperature',
	custom: '{\n  "temperature": 0.7,\n  "max_tokens": 512\n}'
};

export const REPLY_STYLE_GUIDANCE: Record<string, { description: string; example: string }> = {
	concise: {
		description: 'Short answers that get straight to the point.',
		example: 'e.g. Keep replies to a few sentences and use simple words.'
	},
	balanced: {
		description: 'A clear answer with enough explanation to help you understand.',
		example: 'e.g. Start with the answer, then add a brief explanation. Reply in Kiswahili.'
	},
	detailed: {
		description: 'Full explanations with useful steps, context, and examples.',
		example: 'e.g. Explain each step and include practical examples.'
	}
};

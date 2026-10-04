<script lang="ts">
	import { validateNumericSetting, parseCustomParameters } from '$lib/services/generation-settings';
	import { RotateCcw } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Checkbox } from '$lib/components/ui/checkbox';
	import { Input } from '$lib/components/ui/input';
	import Label from '$lib/components/ui/label/label.svelte';
	import * as Select from '$lib/components/ui/select';
	import { Textarea } from '$lib/components/ui/textarea';
	import { SETTING_CONFIG_DEFAULT, SETTING_CONFIG_INFO } from '$lib/constants/settings-config';
	import { supportsVision } from '$lib/stores/server.svelte';
	import { getParameterInfo } from '$lib/stores/settings.svelte';
	import { ParameterSyncService } from '$lib/services/parameter-sync';
	import ParameterSourceIndicator from './ParameterSourceIndicator.svelte';
	import type { Component } from 'svelte';
	import { toast } from 'svelte-sonner';

	interface Props {
		fields: SettingsFieldConfig[];
		localConfig: SettingsConfigType;
		onConfigChange: (key: string, value: string | boolean) => void;
		onThemeChange?: (theme: string) => void;
	}

	let { fields, localConfig, onConfigChange, onThemeChange }: Props = $props();

	// Status line under an action button after it has run, keyed by field.
	let actionStatus = $state<Record<string, string>>({});
	let actionBusy = $state<Record<string, boolean>>({});

	async function runAction(field: SettingsFieldConfig) {
		if (!field.action || actionBusy[field.key]) return;
		actionBusy[field.key] = true;
		try {
			actionStatus[field.key] = await field.action();
			toast.success(actionStatus[field.key]);
		} catch (error) {
			actionStatus[field.key] = error instanceof Error ? error.message : 'Something went wrong.';
			toast.error(actionStatus[field.key]);
		} finally {
			actionBusy[field.key] = false;
		}
	}

	function resetField(field: SettingsFieldConfig, defaultValue: unknown) {
		onConfigChange(field.key, String(defaultValue));
		toast.success(`${field.label}: default selected. Save to keep it.`);
	}

	// Helper function to get parameter source info for syncable parameters
	function getParameterSourceInfo(key: string) {
		if (!ParameterSyncService.canSyncParameter(key)) {
			return null;
		}

		return getParameterInfo(key);
	}
</script>

{#each fields as field (field.key)}
	<div class="space-y-2">
		{#if field.type === 'input'}
			{@const paramInfo = getParameterSourceInfo(field.key)}
			{@const numericError = field.numeric
				? validateNumericSetting(field.key, localConfig[field.key])
				: undefined}
			{@const currentValue = String(localConfig[field.key] ?? '')}
			{@const propsDefault = paramInfo?.serverDefault}
			{@const isCustomRealTime = (() => {
				if (!paramInfo) return false;
				const comparisonDefault = propsDefault ?? SETTING_CONFIG_DEFAULT[field.key];

				// Apply same rounding logic for real-time comparison
				const inputValue = currentValue;
				const numericInput = parseFloat(inputValue);
				const normalizedInput = !isNaN(numericInput)
					? Math.round(numericInput * 1000000) / 1000000
					: inputValue;
				const normalizedDefault =
					typeof comparisonDefault === 'number'
						? Math.round(comparisonDefault * 1000000) / 1000000
						: comparisonDefault;

				return normalizedInput !== normalizedDefault;
			})()}

			<div class="flex flex-wrap items-center gap-2">
				<Label for={field.key} class="text-sm font-medium">
					{field.label}
				</Label>
				{#if field.technicalName}<span class="font-mono text-xs text-muted-foreground"
						>{field.technicalName}</span
					>{/if}
				{#if isCustomRealTime}
					<ParameterSourceIndicator />
				{/if}
			</div>

			{#if field.key === 'max_tokens'}
				<select
					aria-label="Reply length mode"
					class="mb-2 w-full rounded-md border border-input bg-background px-3 py-2 text-sm md:max-w-md"
					value={Number(localConfig.max_tokens) === -1 ? 'unlimited' : 'limited'}
					onchange={(e) =>
						onConfigChange(
							'max_tokens',
							e.currentTarget.value === 'unlimited'
								? '-1'
								: String(typeof propsDefault === 'number' && propsDefault > 0 ? propsDefault : 2048)
						)}
				>
					<option value="unlimited">No fixed limit</option><option value="limited"
						>Set a reply limit</option
					>
				</select>
			{/if}
			{#if field.key === 'max_tokens' && Number(localConfig.max_tokens) === -1 && paramInfo}
				<button
					type="button"
					class="text-xs text-muted-foreground underline"
					onclick={() => resetField(field, propsDefault ?? SETTING_CONFIG_DEFAULT[field.key])}
					>Reset reply length to default</button
				>
			{/if}
			{#if field.key !== 'max_tokens' || Number(localConfig.max_tokens) !== -1}
				<div class="relative w-full md:max-w-md">
					<Input
						id={field.key}
						type={field.numeric ? 'number' : field.key === 'apiKey' ? 'password' : 'text'}
						min={field.numeric?.min}
						max={field.numeric?.max}
						step={field.numeric?.step ?? 'any'}
						aria-invalid={numericError ? true : undefined}
						aria-describedby={numericError ? `${field.key}-error` : `${field.key}-help`}
						value={currentValue}
						oninput={(e) => {
							// Update local config immediately for real-time badge feedback
							onConfigChange(field.key, e.currentTarget.value);
						}}
						placeholder={field.key === 'userName'
							? 'Your name (optional)'
							: `Default: ${SETTING_CONFIG_DEFAULT[field.key] ?? 'none'}`}
						class="w-full {paramInfo ? 'pr-10' : ''}"
					/>
					{#if paramInfo}
						<button
							type="button"
							onclick={() => {
								const defaultValue = propsDefault ?? SETTING_CONFIG_DEFAULT[field.key];
								resetField(field, defaultValue);
							}}
							class="absolute top-1/2 right-2 inline-flex h-5 w-5 -translate-y-1/2 items-center justify-center rounded transition-colors hover:bg-muted"
							aria-label="Reset {field.label.toLowerCase()} to model default"
							title="Reset to model default"
						>
							<RotateCcw class="h-3 w-3" />
						</button>
					{/if}
				</div>
			{/if}
			{#if numericError}<p id="{field.key}-error" role="alert" class="text-xs text-destructive">
					{numericError}
				</p>{/if}
			{#if paramInfo}<p class="text-xs text-muted-foreground">
					{isCustomRealTime
						? 'Your value'
						: propsDefault === undefined
							? 'App default'
							: 'Model default'}{#if propsDefault !== undefined}
						· Default: {propsDefault === -1 && field.key === 'max_tokens'
							? 'No fixed limit'
							: propsDefault}{/if}
				</p>{/if}
			{#if field.help || SETTING_CONFIG_INFO[field.key]}
				<p id="{field.key}-help" class="mt-1 text-xs text-muted-foreground">
					{field.help || SETTING_CONFIG_INFO[field.key]}
				</p>
			{/if}
		{:else if field.type === 'textarea'}
			{@const customError = (() => {
				if (field.key !== 'custom') return '';
				try {
					parseCustomParameters(localConfig.custom);
					return '';
				} catch (error) {
					return error instanceof Error ? error.message : 'Check extra request settings.';
				}
			})()}
			<Label for={field.key} class="block text-sm font-medium">
				{field.label}
			</Label>
			{#if field.technicalName}<p class="font-mono text-xs text-muted-foreground">
					{field.technicalName}
				</p>{/if}

			<Textarea
				id={field.key}
				value={String(localConfig[field.key] ?? '')}
				oninput={(e) => onConfigChange(field.key, e.currentTarget.value)}
				aria-invalid={customError ? true : undefined}
				aria-describedby={customError ? `${field.key}-error` : undefined}
				placeholder={field.key === 'userName'
					? 'Your name (optional)'
					: `Default: ${SETTING_CONFIG_DEFAULT[field.key] ?? 'none'}`}
				class="min-h-[100px] max-h-[360px] w-full resize-none overflow-y-auto md:max-w-2xl"
			/>
			{#if customError}<p id="{field.key}-error" role="alert" class="text-xs text-destructive">
					{customError}
				</p>{/if}
			{#if field.help || SETTING_CONFIG_INFO[field.key]}
				<p class="mt-1 text-xs text-muted-foreground">
					{field.help || SETTING_CONFIG_INFO[field.key]}
				</p>
			{/if}
		{:else if field.type === 'select'}
			{@const selectedOption = field.options?.find(
				(opt: { value: string; label: string; icon?: Component }) =>
					opt.value === localConfig[field.key]
			)}
			{@const paramInfo = getParameterSourceInfo(field.key)}
			{@const currentValue = localConfig[field.key]}
			{@const propsDefault = paramInfo?.serverDefault}
			{@const isCustomRealTime = (() => {
				if (!paramInfo) return false;
				const comparisonDefault = propsDefault ?? SETTING_CONFIG_DEFAULT[field.key];

				// For select fields, do direct comparison (no rounding needed)
				return currentValue !== propsDefault;
			})()}

			<div class="flex flex-wrap items-center gap-2">
				<Label for={field.key} class="text-sm font-medium">
					{field.label}
				</Label>
				{#if isCustomRealTime}
					<ParameterSourceIndicator />
				{/if}
			</div>

			<Select.Root
				type="single"
				value={currentValue}
				onValueChange={(value) => {
					if (field.key === 'theme' && value && onThemeChange) {
						onThemeChange(value);
					} else {
						onConfigChange(field.key, value);
					}
				}}
			>
				<div class="relative w-full md:w-auto md:max-w-md">
					<Select.Trigger class="w-full">
						<div class="flex flex-wrap items-center gap-2">
							{#if selectedOption?.icon}
								{@const IconComponent = selectedOption.icon}
								<IconComponent class="h-4 w-4" />
							{/if}

							{selectedOption?.label || `Select ${field.label.toLowerCase()}`}
						</div>
					</Select.Trigger>
					{#if isCustomRealTime}
						<button
							type="button"
							onclick={() => {
								const defaultValue = propsDefault ?? SETTING_CONFIG_DEFAULT[field.key];
								resetField(field, defaultValue);
							}}
							class="absolute top-1/2 right-8 inline-flex h-5 w-5 -translate-y-1/2 items-center justify-center rounded transition-colors hover:bg-muted"
							aria-label="Reset to default"
							title="Reset to default"
						>
							<RotateCcw class="h-3 w-3" />
						</button>
					{/if}
				</div>
				<Select.Content>
					{#if field.options}
						{#each field.options as option (option.value)}
							<Select.Item value={option.value} label={option.label}>
								<div class="flex flex-wrap items-center gap-2">
									{#if option.icon}
										{@const IconComponent = option.icon}
										<IconComponent class="h-4 w-4" />
									{/if}
									{option.label}
								</div>
							</Select.Item>
						{/each}
					{/if}
				</Select.Content>
			</Select.Root>
			{#if field.help || SETTING_CONFIG_INFO[field.key]}
				<p class="mt-1 text-xs text-muted-foreground">
					{field.help || SETTING_CONFIG_INFO[field.key]}
				</p>
			{/if}
		{:else if field.type === 'checkbox'}
			<div class="flex items-start space-x-3">
				<Checkbox
					id={field.key}
					checked={Boolean(localConfig[field.key])}
					onCheckedChange={(checked) => onConfigChange(field.key, checked)}
					class="mt-1"
				/>

				<div class="space-y-1">
					<label for={field.key} class="cursor-pointer text-sm leading-none font-medium">
						{field.label}
					</label>

					{#if field.help || SETTING_CONFIG_INFO[field.key]}
						<p class="text-xs text-muted-foreground">
							{field.help || SETTING_CONFIG_INFO[field.key]}
						</p>
					{:else if field.key === 'pdfAsImage' && !supportsVision()}
						<p class="text-xs text-muted-foreground">
							For non-vision models, PDFs will be processed as text automatically.
						</p>
					{/if}
				</div>
			</div>
		{:else if field.type === 'action'}
			<div class="space-y-1">
				<Label class="text-sm font-medium">{field.label}</Label>
				{#if field.help || SETTING_CONFIG_INFO[field.key]}
					<p class="text-xs text-muted-foreground">
						{field.help || SETTING_CONFIG_INFO[field.key]}
					</p>
				{/if}
				<div class="flex items-center gap-3 pt-1">
					<Button
						size="sm"
						variant="outline"
						disabled={actionBusy[field.key]}
						onclick={() => runAction(field)}
					>
						{field.actionLabel ?? field.label}
					</Button>
					{#if actionStatus[field.key]}
						<span class="text-xs text-muted-foreground">{actionStatus[field.key]}</span>
					{/if}
				</div>
			</div>
		{/if}
	</div>
{/each}

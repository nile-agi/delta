export function modelFailureMessage(name: string, exitCode?: number): string {
	if (exitCode === 132 || (exitCode != null && (exitCode >>> 0) === 0xc000001d)) {
		return `Model "${name}" stopped because a required CPU instruction is unavailable. Review the report for details.`;
	}
	return `Model "${name}" failed to load. Review the report for details.`;
}

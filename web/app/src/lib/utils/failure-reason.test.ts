import { expect, it } from 'vitest';
import { modelFailureMessage } from './failure-reason';
it('explains illegal-instruction exits without guessing about other failures', () => {
  for (const code of [132, 3221225501, -1073741795])
    expect(modelFailureMessage('Gemma 3 1B', code)).toContain('required CPU instruction');
  expect(modelFailureMessage('Gemma 3 1B', 1)).toBe('Model "Gemma 3 1B" failed to load. Review the report for details.');
});

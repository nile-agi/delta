/** Keep a small anchored picker visible even inside a narrow floating window. */
export function positionPicker(node: HTMLElement) {
	const anchor = node.parentElement!;
	function position() {
		const rect = anchor.getBoundingClientRect();
		const left = Math.max(
			8,
			Math.min(rect.right - node.offsetWidth, window.innerWidth - node.offsetWidth - 8)
		);
		const top =
			rect.bottom + node.offsetHeight + 8 <= window.innerHeight
				? rect.bottom + 4
				: Math.max(8, rect.top - node.offsetHeight - 4);
		node.style.left = `${left}px`;
		node.style.top = `${top}px`;
	}
	position();
	window.addEventListener('resize', position);
	window.addEventListener('scroll', position, true);
	return {
		destroy() {
			window.removeEventListener('resize', position);
			window.removeEventListener('scroll', position, true);
		}
	};
}

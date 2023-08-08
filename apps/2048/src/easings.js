const bezier = (t, p0, p1, p2, p3) => {
	const c = (1 - t) * (1 - t) * (1 - t);
	const b = 3 * ((1 - t) * (1 - t)) * t;
	const a = 3 * (1 - t) * (t * t);
	const d = t * t * t;
	return c * p0 + b * p1 + a * p2 + d * p3;
};

export function ease(t) {
	const p0 = { x: 0, y: 0 };
	const p1 = { x: 0.25, y: 0.1 };
	const p2 = { x: 0.25, y: 1 };
	const p3 = { x: 1, y: 1 };
	return {
		x: bezier(t, p0.x, p1.x, p2.x, p3.x),
		y: bezier(t, p0.y, p1.y, p2.y, p3.y),
	};
}

export function easeIn(t) {
	const p0 = { x: 0, y: 0 };
	const p1 = { x: 0.42, y: 0 };
	const p2 = { x: 1, y: 1 };
	const p3 = { x: 1, y: 1 };
	return {
		x: bezier(t, p0.x, p1.x, p2.x, p3.x),
		y: bezier(t, p0.y, p1.y, p2.y, p3.y),
	};
}

export function easeInOut(t) {
	const p0 = { x: 0, y: 0 };
	const p1 = { x: 0.42, y: 0 };
	const p2 = { x: 0.58, y: 1 };
	const p3 = { x: 1, y: 1 };
	return {
		x: bezier(t, p0.x, p1.x, p2.x, p3.x),
		y: bezier(t, p0.y, p1.y, p2.y, p3.y),
	};
}

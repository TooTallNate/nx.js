export class FPS extends EventTarget {
	last: number;
	rate: number;
	time: number;
	decay: number;
	every: number;
	ticks: number;

	constructor() {
		super();
		this.last = Date.now();
		this.rate = 0;
		this.time = 0;
		this.decay = 0.2;
		this.every = 10;
		this.ticks = 0;
	}

	tick = () => {
		const time = Date.now(),
			diff = time - this.last,
			fps = diff;

		this.ticks += 1;
		this.last = time;
		this.time += (fps - this.time) * this.decay;
		this.rate = 1000 / this.time;
		if (!(this.ticks % this.every)) {
			this.dispatchEvent(new Event('update'));
		}
	};
}

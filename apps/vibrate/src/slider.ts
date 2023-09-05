export class Slider {
	ctx: CanvasRenderingContext2D;
	x: number;
	y: number;
	length: number;
	#value: number;

	constructor(
		ctx: CanvasRenderingContext2D,
		x: number,
		y: number,
		length: number
	) {
		this.ctx = ctx;
		this.x = x;
		this.y = y;
		this.length = length;
		this.#value = 0;
		this.render();
	}

	get value() {
		return this.#value;
	}

	set value(v: number) {
		if (v > 1) v = 1;
		if (v < 0) v = 0;
		this.#value = v;
		this.render();
	}

	render() {
		const { ctx } = this;

		const width = 20;
		const r = width / 2;

		const filledLength = this.length * this.value;
		const emptyLength = this.length - filledLength;

		// clear bounding box
		ctx.beginPath();
		ctx.fillStyle = 'black';
		ctx.fillRect(
			this.x - r - 1,
			this.y - width - 1,
			width * 2 + 2,
			this.length + width * 2 + 2
		);
		//ctx.clearRect(this.x, this.y, width, this.length);

		// filled section
		ctx.fillStyle = 'white';
		ctx.roundRect(this.x, this.y + emptyLength, width, filledLength, [
			0,
			0,
			r,
			r,
		]);
		ctx.fill();

		// empty section
		ctx.beginPath();
		ctx.fillStyle = 'rgba(250, 250, 250, 0.2)';
		ctx.roundRect(this.x, this.y, width, emptyLength, [r, r, 0, 0]);
		ctx.fill();

		// value circle
		ctx.beginPath();
		ctx.arc(this.x + r + 0.5, this.y + emptyLength, width, 0, 2 * Math.PI);
		ctx.fillStyle = 'black';
		ctx.fill();

		// TODO: remove - should not be necessary
		ctx.beginPath();
		ctx.arc(this.x + r, this.y + emptyLength, width, 0, 2 * Math.PI);

		ctx.strokeStyle = 'white';
		ctx.stroke();
	}
}

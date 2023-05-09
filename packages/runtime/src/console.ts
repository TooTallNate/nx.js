export class Console {
	log(input: unknown) {
		Switch.print(
			`${typeof input === 'string' ? input : JSON.stringify(input)}\n`
		);
	}
}

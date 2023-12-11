import { $ } from './$';

export class Env {
	get(name: string) {
		return $.getenv(name);
	}

	set(name: string, value: string) {
		$.setenv(name, value);
	}

	delete(name: string) {
		$.unsetenv(name);
	}

	toObject() {
		return $.envToObject();
	}
}

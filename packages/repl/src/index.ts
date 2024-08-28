import { REPL as BaseREPL } from './repl';

export class REPL extends BaseREPL {
	inspect(v: unknown): string {
		return Switch.inspect(v);
	}
	escape(): void {
		Switch.exit();
	}
}

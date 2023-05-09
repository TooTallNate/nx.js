export class Console {
    log(input: unknown) {
        Switch.print(`${String(input)}\n`);
    }
}

export const console = new Console();
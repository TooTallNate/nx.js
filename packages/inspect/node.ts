import { createInspect } from './src/index';

export const inspect = createInspect();

console.log(inspect(Promise.resolve(42)));
console.log(inspect(/test/g));
console.log(inspect({ a: 1, b: 2, 'c d': 3 }));
console.log(inspect(async () => 42));

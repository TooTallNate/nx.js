import { cursor, erase } from 'sisteransi';
import { red, blue, green, white, bold, bgRed } from 'kleur/colors';

console.log();
console.log(red('red text'));

console.log();
console.log(bold(blue('howdy partner')));

console.log();
console.log(`${bold(white(bgRed('[ERROR]')))} ${red('Something happened')}`);

const printTime = (move = true) => {
	console.log(
		`${move ? `${cursor.up(1)}${erase.line}` : ''}${green(
			'Current time:'
		)} ${new Date().toISOString()}`
	);
};
console.log();
printTime(false);
Switch.addEventListener('frame', () => printTime());

import { GameManager } from './game_manager';
import { StorageManager } from './storage_manager';
import { CanvasActuator } from './canvas_actuator';
import { InputManager } from './input_manager';
import clearSansPath from './fonts/ClearSans-Regular.ttf';
import clearSansBoldPath from './fonts/ClearSans-Bold.ttf';

const clearSansData = Switch.readFileSync(
	new URL(clearSansPath, Switch.entrypoint),
);
const clearSansBoldData = Switch.readFileSync(
	new URL(clearSansBoldPath, Switch.entrypoint),
);
fonts.add(new FontFace('Clear Sans', clearSansData));
fonts.add(new FontFace('Clear Sans', clearSansBoldData, { weight: 'bold' }));

new GameManager(4, InputManager, CanvasActuator, StorageManager);

import { GameManager } from './game_manager';
import { StorageManager } from './storage_manager';
import { CanvasActuator } from './canvas_actuator';
import { InputManager } from './input_manager';
import rPath from './fonts/ClearSans-Regular.ttf';
import bPath from './fonts/ClearSans-Bold.ttf';

const clearSansData = Switch.readFileSync(new URL(rPath, import.meta.url));
const clearSansBoldData = Switch.readFileSync(new URL(bPath, import.meta.url));
fonts.add(new FontFace('Clear Sans', clearSansData));
fonts.add(new FontFace('Clear Sans', clearSansBoldData, { weight: 'bold' }));

new GameManager(4, InputManager, CanvasActuator, StorageManager);

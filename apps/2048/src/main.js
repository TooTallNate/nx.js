import { GameManager } from './game_manager';
import { StorageManager } from './storage_manager';
import { CanvasActuator } from './canvas_actuator';
import { InputManager } from './input_manager';

new GameManager(4, InputManager, CanvasActuator, StorageManager);

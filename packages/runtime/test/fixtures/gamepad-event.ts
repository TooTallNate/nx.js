import { test } from '../src/tap';

// `GamepadEvent` is a standard Web API (present in Chrome and nx.js). These
// assertions exercise the event shape + dispatch path that drives nx.js's
// `gamepadconnected` / `gamepaddisconnected` events, and run identically in
// both environments. The real serial-backed `Gamepad.id` and hotplug
// detection are device-only (verified on-device), so they are not asserted
// here.

test('GamepadEvent type and gamepad property', (t) => {
	const ev = new GamepadEvent('gamepadconnected', { gamepad: null });
	t.equal(ev.type, 'gamepadconnected', 'type is set');
	t.equal(ev.gamepad, null, 'gamepad property is exposed');
});

test('GamepadEvent dispatches on an EventTarget', (t) => {
	const target = new EventTarget();
	let received: GamepadEvent | null = null;
	target.addEventListener('gamepaddisconnected', ((e: GamepadEvent) => {
		received = e;
	}) as EventListener);
	const ev = new GamepadEvent('gamepaddisconnected', { gamepad: null });
	target.dispatchEvent(ev);
	t.ok(received, 'listener received the GamepadEvent');
	t.equal(
		(received as unknown as GamepadEvent | null)?.type,
		'gamepaddisconnected',
		'received event has correct type',
	);
});

test('GamepadEvent is an instance of Event', (t) => {
	const ev = new GamepadEvent('gamepadconnected', { gamepad: null });
	t.ok(ev instanceof Event, 'GamepadEvent extends Event');
});

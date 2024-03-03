import React, { useEffect, useState } from 'react';
import { Rect } from 'react-tela';

export function App() {
	const [r, setR] = useState(0);
	const [pos, setPos] = useState({ x: 0, y: 0 });
	useEffect(() => {
		function frame() {
			setR((r) => r + 1);
			requestAnimationFrame(frame);
		}
		//requestAnimationFrame(frame);
	}, []);
	return (
		<Rect
			x={pos.x}
			y={pos.y}
			width={400}
			height={300}
			fill="green"
			rotate={r}
			onTouchMove={(e) => {
				const touch = e.changedTouches[0];
				setPos({ x: touch.clientX, y: touch.clientY });
			}}
		></Rect>
	);
}

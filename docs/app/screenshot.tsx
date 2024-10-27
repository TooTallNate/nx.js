import Image, { ImageProps } from 'next/image';
import SwitchImg from '@/screenshots/switch.png';

export function Screenshot(props: ImageProps) {
	return (
		<div className='relative inline-block'>
			<Image
				width={1280}
				height={720}
				{...props}
				className='absolute top-[8.2%] left-[17.6%] m-0 w-[64.9%]'
			/>
			<Image
				src={SwitchImg}
				alt='Screenshot'
				width={1280}
				height={720}
				className='drop-shadow-md m-0'
			/>
		</div>
	);
}

import Link from 'next/link';
import Logo from './logo';
import Discord from './discord';
import GitHub from './github';
import GbaTemp from './gbatemp';
import Bitcoin from './bitcoin';
import Ethereum from './ethereum';

export default function HomePage() {
	return (
		<main className='flex h-screen flex-col justify-center items-center text-center gap-6'>
			<Logo className='w-64' />
			<div className='flex flex-col gap-2'>
				<h1 className='text-3xl font-bold'>nx.js</h1>
				<h2 className='text-lg'>
					JavaScript runtime for Nintendo Switch homebrew applications
				</h2>
			</div>
			<p className='text-muted-foreground'>
				Go to the{' '}
				<Link
					href='/runtime'
					className='text-foreground font-semibold underline'
				>
					Getting Started
				</Link>{' '}
				page to jump into the documentation.
			</p>
			<div className='flex text-5xl gap-8'>
				<a
					href='https://github.com/TooTallNate/nx.js'
					target='_blank'
					title='GitHub Repository'
				>
					<GitHub className='hover:drop-shadow-[0_0_2px_rgba(0,0,0,0.5)] dark:hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.5)] transition-all' />
				</a>
				<a
					href='https://discord.gg/MMmn73nsGz'
					target='_blank'
					title='Discord Server'
				>
					<Discord className='fill-gray-700 dark:fill-white hover:drop-shadow-[0_0_2px_rgba(0,0,0,0.4)] dark:hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.4)] transition-all' />
				</a>
				<a
					href='https://gbatemp.net/threads/nx-js-javascript-runtime-for-nintendo-switch-homebrew-applications.639171/'
					target='_blank'
					title='GBATemp Thread'
				>
					<GbaTemp
						className='fill-gray-600 dark:fill-white hover:drop-shadow-[0_0_1.5px_rgba(0,0,0,0.4)] dark:hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.6)] transition-all'
						eyesClassName='fill-[#ff9600]'
					/>
				</a>
			</div>

			<div>
				<h3 className='text-lg font-bold'>Donate</h3>
				<div className='text-muted-foreground text-sm'>
					<Bitcoin className='inline-block' />{' '}
					1CeNHAi1XXsGL5dWSdR9tTMko9MtcvMvCy
				</div>
				<div className='text-muted-foreground text-sm'>
					<Ethereum className='inline-block' />{' '}
					0xc411D343B7Ba8a2CAaEC23facBd3bE49CCa7bb39
				</div>
			</div>
		</main>
	);
}

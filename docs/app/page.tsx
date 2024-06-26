import Link from 'next/link';
import Logo from './logo';
import Discord from './discord';
import GitHub from './github';
import GbaTemp from './gbatemp';

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
				Go to{' '}
				<Link href='/docs' className='text-foreground font-semibold underline'>
					/docs
				</Link>{' '}
				to read the documentation.
			</p>
			<div className='flex text-5xl gap-8'>
				<a
					href='https://github.com/TooTallNate/nx.js'
					target='_blank'
					title='GitHub Repository'
				>
					<GitHub className='fill-white hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.5)] transition-all' />
				</a>
				<a
					href='https://discord.gg/MMmn73nsGz'
					target='_blank'
					title='Discord Server'
				>
					<Discord className='fill-white hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.4)] transition-all' />
				</a>
				<a
					href='https://gbatemp.net/threads/nx-js-javascript-runtime-for-nintendo-switch-homebrew-applications.639171/'
					target='_blank'
					title='GBATemp Thread'
				>
					<GbaTemp className='hover:drop-shadow-[0_0_8px_rgba(255,255,255,0.6)] transition-all' />
				</a>
			</div>
		</main>
	);
}

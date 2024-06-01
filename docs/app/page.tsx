import Link from 'next/link';
import Logo from './logo';

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
				You can open{' '}
				<Link href='/docs' className='text-foreground font-semibold underline'>
					/docs
				</Link>{' '}
				and see the documentation.
			</p>
		</main>
	);
}

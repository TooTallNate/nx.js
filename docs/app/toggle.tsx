import { RootToggle } from 'fumadocs-ui/components/layout/root-toggle';
export function Toggle() {
	return (
		<RootToggle
			options={[
				{
					title: 'Documentation',
					description: 'Pages in folder 1',
					url: '/docs',
					icon: 'docs',
				},
				{
					title: 'API Reference',
					description: 'Pages in folder 2',
					url: '/api',
					icon: 'api',
				},
			]}
		/>
	);
}

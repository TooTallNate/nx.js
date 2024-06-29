import { TbGlobe, TbBrowser, TbBrandPowershell } from 'react-icons/tb';
import { RootToggle } from 'fumadocs-ui/components/layout/root-toggle';

export function Toggle() {
	return (
		<RootToggle
			options={[
				{
					title: 'Core Runtime',
					description: 'Global APIs',
					url: '/runtime',
					icon: <TbGlobe size={24} />,
				},
				{
					title: '@nx.js/http',
					description: 'HTTP server npm package',
					url: '/http',
					icon: <TbBrowser size={24} />,
				},
				{
					title: '@nx.js/repl',
					description: 'REPL class npm package',
					url: '/repl',
					icon: <TbBrandPowershell size={24} />,
				},
			]}
		/>
	);
}

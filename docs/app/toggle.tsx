import {
	TbGlobe,
	TbBrowser,
	TbBraces,
	TbBrandPowershell,
} from 'react-icons/tb';
import { RootToggle } from 'fumadocs-ui/components/layout/root-toggle';

export function Toggle() {
	return (
		<RootToggle
			options={[
				{
					title: 'Runtime',
					description: 'Global APIs',
					url: '/runtime',
					icon: <TbGlobe size={24} />,
				},
				{
					title: '@nx.js/constants',
					description: 'Constants and enums',
					url: '/constants',
					icon: <TbBraces size={24} />,
				},
				{
					title: '@nx.js/http',
					description: 'HTTP server for nx.js',
					url: '/http',
					icon: <TbBrowser size={24} />,
				},
				{
					title: '@nx.js/repl',
					description: 'Read-Eval-Print Loop for nx.js',
					url: '/repl',
					icon: <TbBrandPowershell size={24} />,
				},
			]}
		/>
	);
}

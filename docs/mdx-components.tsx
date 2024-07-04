import defaultComponents from 'fumadocs-ui/mdx';
import { Tab, Tabs } from 'fumadocs-ui/components/tabs';
import { Callout } from 'fumadocs-ui/components/callout';
import {
	Pre,
	CodeBlock,
	type CodeBlockProps,
} from 'fumadocs-ui/components/codeblock';
import { Children, cloneElement } from 'react';
import {
	TbInfoCircle,
	TbBulb,
	TbAlertTriangle,
	TbHandStop,
	TbAlertOctagon,
} from 'react-icons/tb';
import type { MDXComponents } from 'mdx/types';

type CalloutProps = React.ComponentProps<typeof Callout>;

export function useMDXComponents(components: MDXComponents): MDXComponents {
	return {
		...defaultComponents,
		pre: ({ title, className, icon, allowCopy, ...props }: CodeBlockProps) => (
			<CodeBlock title={title} icon={icon} allowCopy={allowCopy}>
				<Pre className={`'max-h-[400px] ${className || ''}`} {...props} />
			</CodeBlock>
		),
		Tab,
		InstallTabs: ({
			items,
			children,
		}: {
			items: string[];
			children: React.ReactNode;
		}) => (
			<Tabs items={items} id='package-manager'>
				{children}
			</Tabs>
		),
		blockquote: (props) => {
			let icon: CalloutProps['icon'] = undefined;
			let title: CalloutProps['title'] = undefined;
			const children = mapStringChildren(props.children, (str) => {
				return str.replace(/^(?:\[!([A-Z]+)\]|([A-Z]+):)/, (_, t1, t2) => {
					const t = t1 || t2;
					if (t === 'NOTE') {
						title = <span className='text-blue-500'>Note</span>;
						icon = (
							<div className='border-l-blue-500 border-l-2 text-blue-500 pl-1 text-lg'>
								<TbInfoCircle />
							</div>
						);
					} else if (t === 'TIP') {
						title = <span className='text-emerald-500'>Tip</span>;
						icon = (
							<div className='border-l-emerald-500 border-l-2 text-emerald-500 pl-2 text-lg'>
								<TbBulb />
							</div>
						);
					} else if (t === 'IMPORTANT') {
						title = <span className='text-purple-500'>Important</span>;
						icon = (
							<div className='border-l-purple-500 border-l-2 text-purple-500 pl-2 text-lg'>
								<TbAlertOctagon />
							</div>
						);
					} else if (t === 'WARNING') {
						title = <span className='text-yellow-500'>Warning</span>;
						icon = (
							<div className='border-l-yellow-500 border-l-2 text-yellow-500 pl-2 text-lg'>
								<TbAlertTriangle />
							</div>
						);
					} else if (t === 'CAUTION') {
						title = <span className='text-rose-500'>Caution</span>;
						icon = (
							<div className='border-l-rose-500 border-l-2 text-rose-500 pl-2 text-lg'>
								<TbHandStop />
							</div>
						);
					}
					return '';
				});
			});
			return (
				<Callout title={title} icon={icon}>
					{children}
				</Callout>
			);
		},
		...components,
	};
}

function mapStringChildren(
	children: React.ReactNode,
	fn: (str: string) => string,
): React.ReactNode {
	if (typeof children === 'string') {
		return fn(children);
	}
	if (typeof children !== 'object' || children === null) {
		return children;
	}
	if (Array.isArray(children)) {
		return Children.map(children, (child) => mapStringChildren(child, fn));
	}
	// @ts-expect-error
	return cloneElement(children, {
		// @ts-expect-error
		children: mapStringChildren(children.props.children, fn),
	});
}

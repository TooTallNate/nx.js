import defaultComponents from 'fumadocs-ui/mdx';
import { Tab, Tabs } from 'fumadocs-ui/components/tabs';
import { Callout } from 'fumadocs-ui/components/callout';
import {
	Pre,
	CodeBlock,
	type CodeBlockProps,
} from 'fumadocs-ui/components/codeblock';
import type { MDXComponents } from 'mdx/types';
import { Children, cloneElement } from 'react';

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
			let type: CalloutProps['type'] = 'info';
			let title: CalloutProps['title'] = undefined;
			const children = mapStringChildren(props.children, (str) => {
				return str.replace(/^(?:\[!([A-Z]+)\]|([A-Z]+):)/, (_, t1, t2) => {
					const t = t1 || t2;
					if (t === 'NOTE') {
						type = 'info';
						title = 'Note';
					} else if (t === 'TIP') {
						type = undefined;
						title = 'Tip';
						icon = '💡';
					} else if (t === 'IMPORTANT') {
						type = undefined;
						title = 'Important';
						icon = '⚠️';
					} else if (t === 'WARNING') {
						type = 'warn';
						title = 'Warning';
					} else if (t === 'CAUTION') {
						type = 'error';
						title = 'Caution';
					}
					return '';
				});
			});
			return (
				<Callout type={type} title={title} icon={icon}>
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

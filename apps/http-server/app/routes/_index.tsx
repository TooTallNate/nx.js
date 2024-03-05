import { Link, useLoaderData } from "@remix-run/react";
import { LoaderFunctionArgs } from "@remix-run/server-runtime";

export async function loader(args: LoaderFunctionArgs) {
	console.log(args);
	return { version: Switch.version.nxjs };
}

export default function Index() {
	const data = useLoaderData<typeof loader>();
	return (
		<div style={{ fontFamily: "system-ui, sans-serif", lineHeight: "1.8" }}>
			<h1>Welcome to Remix on nx.js v{data.version}</h1>
			<Link to="/other">Other</Link>
		</div>
	);
}

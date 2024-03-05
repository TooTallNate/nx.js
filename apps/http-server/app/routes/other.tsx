import { Link } from "@remix-run/react";

export default function Index() {
	return (
		<div style={{ fontFamily: "system-ui, sans-serif", lineHeight: "1.8" }}>
			<h1>Other page</h1>
			<Link to="/">Home</Link>
		</div>
	);
}

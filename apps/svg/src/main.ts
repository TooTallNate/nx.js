import { Canvg } from 'canvg';
import { DOMParser } from '@xmldom/xmldom';

// From: https://commons.wikimedia.org/wiki/File:Ghostscript_Tiger.svg
const svgData = Switch.readFileSync(
	new URL('Ghostscript_Tiger.svg', Switch.entrypoint),
);
const svg = new TextDecoder().decode(svgData);

const ctx = screen.getContext('2d');
ctx.fillStyle = '#555';
ctx.fillRect(0, 0, screen.width, screen.height);

const v = Canvg.fromString(ctx, svg, { DOMParser });
v.render({ ignoreClear: true });

import { Canvg } from 'canvg';
import { DOMParser } from '@xmldom/xmldom';

// From: https://commons.wikimedia.org/wiki/File:Ghostscript_Tiger.svg
const svgData = Switch.readFileSync(
	new URL('Ghostscript_Tiger.svg', Switch.entrypoint)
);
const svg = new TextDecoder().decode(svgData);

const canvas = Switch.screen;
const ctx = canvas.getContext('2d');

ctx.fillStyle = '#333';
ctx.fillRect(0, 0, canvas.width, canvas.height);

const v = Canvg.fromString(ctx, svg, { DOMParser });
v.render();

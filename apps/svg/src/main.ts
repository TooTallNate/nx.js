// From: https://commons.wikimedia.org/wiki/File:Ghostscript_Tiger.svg
import svg from './Ghostscript_Tiger.svg';
import { Canvg } from 'canvg';
import { DOMParser } from '@xmldom/xmldom';

const ctx = screen.getContext('2d');
ctx.fillStyle = '#555';
ctx.fillRect(0, 0, screen.width, screen.height);

const v = Canvg.fromString(ctx, svg, { DOMParser });
v.render({ ignoreClear: true });

import { def } from '../utils';
import * as streams from 'web-streams-polyfill/ponyfill/es2018';
for (const k of Object.keys(streams)) {
	def(k, streams[k as keyof typeof streams]);
}

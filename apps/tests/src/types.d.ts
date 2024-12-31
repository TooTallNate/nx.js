/**
 * `NXJS` env var is defined via esbuild's `--define` option.
 */
declare var process: {
	env: {
		NXJS: string;
	};
};

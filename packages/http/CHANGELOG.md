# @nx.js/http

## 0.0.5

### Patch Changes

- Add "repository" and "homepage" fields to `package.json` ([`dfcedb338987565f93273e929b558a8214e32e06`](https://github.com/TooTallNate/nx.js/commit/dfcedb338987565f93273e929b558a8214e32e06))

## 0.0.4

### Patch Changes

- Update TypeScript to v5.5 ([#136](https://github.com/TooTallNate/nx.js/pull/136))

## 0.0.3

### Patch Changes

- Add ability to configure response headers in `createStaticFileHandler()` ([`d471fec11708f60a4dce442e24eb9318b301ea74`](https://github.com/TooTallNate/nx.js/commit/d471fec11708f60a4dce442e24eb9318b301ea74))

## 0.0.2

### Patch Changes

- Fix path traversal outside of root bug in static file handler ([`1e6bb617ed866d0c400088a7c0bed8202da7e967`](https://github.com/TooTallNate/nx.js/commit/1e6bb617ed866d0c400088a7c0bed8202da7e967))

- Refactor HTTP server implementation ([`f887e5ad4f1c160b264492d49af487c8c2c6167b`](https://github.com/TooTallNate/nx.js/commit/f887e5ad4f1c160b264492d49af487c8c2c6167b))

  - Support Keep-Alive by default
  - Support `Content-Length` and `Transfer-Encoding: chunked` bodies
  - Add tests using `vitest`

- Support streaming responses in HTTP server ([`bc9499f0795bf081b0ce606cf54e7b87b20fdaa2`](https://github.com/TooTallNate/nx.js/commit/bc9499f0795bf081b0ce606cf54e7b87b20fdaa2))

## 0.0.1

### Patch Changes

- Add initial `@nx.js/http` package ([#103](https://github.com/TooTallNate/nx.js/pull/103))

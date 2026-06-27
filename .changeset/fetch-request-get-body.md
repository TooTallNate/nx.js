---
"@nx.js/runtime": patch
---

fix: `fetch(new Request(url))` no longer throws "Body not allowed for GET or HEAD requests"

When a bodyless `GET`/`HEAD` `Request` was passed back into the `Request`
constructor (as `fetch()` does internally), the input `Request` object was
treated as the body before extraction, so the GET/HEAD body guard saw the
(always-truthy) wrapper object and wrongly threw. The guard now unwraps the
real body from an input `Request`/`Body` before checking it.

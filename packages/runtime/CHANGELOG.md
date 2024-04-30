# nxjs-runtime

## 0.0.44

### Patch Changes

- Add `Switch.SaveData#freeSpace()` and `Switch.SaveData#totalSpace()` ([`2311d0b47eddb9c98476ec3e109e60d7ffa86e90`](https://github.com/TooTallNate/nx.js/commit/2311d0b47eddb9c98476ec3e109e60d7ffa86e90))

## 0.0.43

## 0.0.42

### Patch Changes

- Fix reported source buffer size in `fs.writeFileSync()` error message ([`eee23840e04778aa4db64e5942ae657cb9e62c86`](https://github.com/TooTallNate/nx.js/commit/eee23840e04778aa4db64e5942ae657cb9e62c86))

- Improvements to `localStorage` based on web platform tests ([`36ed28f54c8a404b143edc6d5668590eee3942db`](https://github.com/TooTallNate/nx.js/commit/36ed28f54c8a404b143edc6d5668590eee3942db))

- Add `Switch.Profile.current` and `Switch.Profile.select()` (replaces `Switch.currentProfile()` and `Switch.selectProfile()`) ([`4018d0446bbc3f7714addca98007ff973c01e486`](https://github.com/TooTallNate/nx.js/commit/4018d0446bbc3f7714addca98007ff973c01e486))

- Use correct replacement character for lone surrogates in `TextEncoder#encode()` ([`1cbfc49ac02f8834f5c479964f9272b27931fe53`](https://github.com/TooTallNate/nx.js/commit/1cbfc49ac02f8834f5c479964f9272b27931fe53))

- Convert `console` into a `Console` class ([`33eb3b0b7c99be7136b0d314970d61c50aab7069`](https://github.com/TooTallNate/nx.js/commit/33eb3b0b7c99be7136b0d314970d61c50aab7069))

- Define `inspect.keys` for `Switch.Application` ([`cba5ff3d9a6fa5e5ee306ea7fb372eb9cd1a612f`](https://github.com/TooTallNate/nx.js/commit/cba5ff3d9a6fa5e5ee306ea7fb372eb9cd1a612f))

- Add `keys`, `values`, and `entries` symbols for `Switch.inspect` ([`84f2c090162eac084b0c489b4cce750c4bb515cf`](https://github.com/TooTallNate/nx.js/commit/84f2c090162eac084b0c489b4cce750c4bb515cf))

- Bind `console` methods to `this` ([`b672db9607446e6651966bb3df4225942cb6540b`](https://github.com/TooTallNate/nx.js/commit/b672db9607446e6651966bb3df4225942cb6540b))

- Add `Switch.SaveData` class (replaces `Switch.FsDev`) ([#113](https://github.com/TooTallNate/nx.js/pull/113))

- Allow `console.print()` to be monkey patched ([`4648806875843dc7a72bd7171c62782d9ad8f3cf`](https://github.com/TooTallNate/nx.js/commit/4648806875843dc7a72bd7171c62782d9ad8f3cf))

- Replace `Switch.profiles` with `Switch.Profile` iterable ([`d60ae8c3f8167af492dad96991ae5cb42b7342e8`](https://github.com/TooTallNate/nx.js/commit/d60ae8c3f8167af492dad96991ae5cb42b7342e8))

- Implement `TextEncoder#encodeInto()` function ([`3f2e22d8291d094adb23040e89d479ad925824bc`](https://github.com/TooTallNate/nx.js/commit/3f2e22d8291d094adb23040e89d479ad925824bc))

- Implement `CustomEvent` class ([`1b9a8d94a7b82164cfe44bd3a3f39667bdac99db`](https://github.com/TooTallNate/nx.js/commit/1b9a8d94a7b82164cfe44bd3a3f39667bdac99db))

## 0.0.41

### Patch Changes

- Fix setting non-string values on `localStorage` ([`d4d0ccf2144d736114aa30731cf9351ce8382c54`](https://github.com/TooTallNate/nx.js/commit/d4d0ccf2144d736114aa30731cf9351ce8382c54))

- Export `Switch.FsDev` class ([`ef2801cf05ba16fff2dc007fd206e36abe8649db`](https://github.com/TooTallNate/nx.js/commit/ef2801cf05ba16fff2dc007fd206e36abe8649db))

- Add `Switch.appletType()` ([`e74f83a4df004af0e6dda6915cf04b49c80d75a9`](https://github.com/TooTallNate/nx.js/commit/e74f83a4df004af0e6dda6915cf04b49c80d75a9))

- Make `name` parameter of `Application#mountSaveData()` optional ([`e87cae9b77874afaadd1bf82db4a41989b2adf33`](https://github.com/TooTallNate/nx.js/commit/e87cae9b77874afaadd1bf82db4a41989b2adf33))

- Add `Switch.operationMode()` ([`696580cfe1f660768fd2b45a49fd3c3af8036632`](https://github.com/TooTallNate/nx.js/commit/696580cfe1f660768fd2b45a49fd3c3af8036632))

- Add `FsDev#url`, remove `FsDev#name` ([`8d00088d6b1c39dc1b909837fa45e6a0ea8e6f8a`](https://github.com/TooTallNate/nx.js/commit/8d00088d6b1c39dc1b909837fa45e6a0ea8e6f8a))

- Add `Application#createCacheData()` and `Application#mountCacheData()` ([`eae68c1715d7e87b58286f3796e2e672dd38a4d6`](https://github.com/TooTallNate/nx.js/commit/eae68c1715d7e87b58286f3796e2e672dd38a4d6))

## 0.0.40

### Patch Changes

- Add `Switch.setMediaPlaybackState()` to disable auto-lock and screen dimming ([`8631e5d477e6ae8c05e2b00b67b73d58d6a4f60a`](https://github.com/TooTallNate/nx.js/commit/8631e5d477e6ae8c05e2b00b67b73d58d6a4f60a))

- Implement `new Switch.Profile()` constructor ([`d30e385dd5037b8fc18c6b4709e21657057e6d7b`](https://github.com/TooTallNate/nx.js/commit/d30e385dd5037b8fc18c6b4709e21657057e6d7b))

- Make `Application#id` return the "PresenceGroupId" instead of "SaveDataOwnerId" ([`70ebcd580add2583e08c06cfa7b3e20a1ad6e065`](https://github.com/TooTallNate/nx.js/commit/70ebcd580add2583e08c06cfa7b3e20a1ad6e065))

- Implement `new Switch.Application` constructor, remove `fromId()` and `fromNro()` ([`ce9c398f79867d4042ef81e3782655b145e9dcd8`](https://github.com/TooTallNate/nx.js/commit/ce9c398f79867d4042ef81e3782655b145e9dcd8))

## 0.0.39

### Patch Changes

- Fix `FormData` test and add URL encoded POST test ([`7b6f8b52214b43ba0192e637a19f2ab4e9db5cda`](https://github.com/TooTallNate/nx.js/commit/7b6f8b52214b43ba0192e637a19f2ab4e9db5cda))

- Fix `URLSearchParams` iterator functions ([`56e3e1517d0d52292a90d5afd33ca608ac251c8e`](https://github.com/TooTallNate/nx.js/commit/56e3e1517d0d52292a90d5afd33ca608ac251c8e))

- Set `allowHalfOpen: true` for TCP servers ([`75c9051d04857d471f7db40df1c661616e850593`](https://github.com/TooTallNate/nx.js/commit/75c9051d04857d471f7db40df1c661616e850593))

- Add tests for `URL` and `URLSearchParams` from web platform tests ([`c0f4fc697c36eba9734ae9481a55117fc6e7564a`](https://github.com/TooTallNate/nx.js/commit/c0f4fc697c36eba9734ae9481a55117fc6e7564a))

- Support "application/x-www-form-urlencoded" Content-Type in `Body#formData()` ([`2d36f595aab4ed8c9455667f5a51181a2ae4623b`](https://github.com/TooTallNate/nx.js/commit/2d36f595aab4ed8c9455667f5a51181a2ae4623b))

- Close TCP socket when writable stream is closed ([`7e740829375beb454503568a09dc12fd7d0f384c`](https://github.com/TooTallNate/nx.js/commit/7e740829375beb454503568a09dc12fd7d0f384c))

- Set "content-length" header in `Response.json()` ([`46ecc024194d96549e0041f8a1f63a5560019dff`](https://github.com/TooTallNate/nx.js/commit/46ecc024194d96549e0041f8a1f63a5560019dff))

- Rename `Application.fromNRO()` to `Application.fromNro()` ([`8122869a12dcc376ea0e1a7985b818696bc611ac`](https://github.com/TooTallNate/nx.js/commit/8122869a12dcc376ea0e1a7985b818696bc611ac))

## 0.0.38

### Patch Changes

- Add `Application.fromId()` ([`c53849472db74142797a874550a9266c6eb502a0`](https://github.com/TooTallNate/nx.js/commit/c53849472db74142797a874550a9266c6eb502a0))

- Properly flush entire buffer in TCP `write()` ([#105](https://github.com/TooTallNate/nx.js/pull/105))

- Copy buffer for `Application#icon` and `Application#nacp` ([`eb1d82a085f82afd6fed8e0c36ea5d146a164405`](https://github.com/TooTallNate/nx.js/commit/eb1d82a085f82afd6fed8e0c36ea5d146a164405))

## 0.0.37

### Patch Changes

- Return `null` in `Switch.readFile()` and `Switch.stat()` if file does not exist ([`b58f7837fbc515edc8b157afce5a2049bf4c697e`](https://github.com/TooTallNate/nx.js/commit/b58f7837fbc515edc8b157afce5a2049bf4c697e))

- Add `DOMMatrix` and `DOMMatrixReadOnly` ([#92](https://github.com/TooTallNate/nx.js/pull/92))

- Add stub types to make `screen` compatible with react-tela `render()` ([`aaa102460c8583d3f309385a2e07550a53bbc9b6`](https://github.com/TooTallNate/nx.js/commit/aaa102460c8583d3f309385a2e07550a53bbc9b6))

- Add Canvas `setTransform()` ([#92](https://github.com/TooTallNate/nx.js/pull/92))

- Implement `path` parameter of Canvas `fill()` and `stroke()` ([`74f074dea66e6c8acea477c9c9a081720c4dc5ce`](https://github.com/TooTallNate/nx.js/commit/74f074dea66e6c8acea477c9c9a081720c4dc5ce))

- Add custom inspect for `DOMMatrix` ([`611f8e1be878a868f701794049a64e0358452c60`](https://github.com/TooTallNate/nx.js/commit/611f8e1be878a868f701794049a64e0358452c60))

- Convert `Touch` into a proper class ([`31ee39f0d2758663a98bd7855dc538482e60b14a`](https://github.com/TooTallNate/nx.js/commit/31ee39f0d2758663a98bd7855dc538482e60b14a))

- Remove poll watchers when closing TCP socket ([`ef3882923417c415e6ef0601c4974b64fe114a09`](https://github.com/TooTallNate/nx.js/commit/ef3882923417c415e6ef0601c4974b64fe114a09))

- Add Canvas `isPointInPath()` ([`8ea15f522f29f6b7b7727c2841d9c56591db4f2d`](https://github.com/TooTallNate/nx.js/commit/8ea15f522f29f6b7b7727c2841d9c56591db4f2d))

- Add more DOM compat to `screen` (`offsetWidth`, `offsetHeight`, `offsetTop`, `offsetLeft`) ([`8d689f8410d92adb45ddb541244a68b6b44787bb`](https://github.com/TooTallNate/nx.js/commit/8d689f8410d92adb45ddb541244a68b6b44787bb))

- Apply transformation matrix in Canvas `isPointInPath()` and `isPointInStroke()` ([`65a360fa85765c59c18e14d275773e7bf2bddedf`](https://github.com/TooTallNate/nx.js/commit/65a360fa85765c59c18e14d275773e7bf2bddedf))

- Add Canvas `isPointInStroke()` ([`28b1283d4f0ef973a5078cd4234caa257d3b9c6f`](https://github.com/TooTallNate/nx.js/commit/28b1283d4f0ef973a5078cd4234caa257d3b9c6f))

## 0.0.36

## 0.0.35

## 0.0.34

## 0.0.33

### Patch Changes

- Add `Switch.Application.fromNRO()` ([`6d77ee3023f77ec2aee0d96d14a07a98022957a1`](https://github.com/TooTallNate/nx.js/commit/6d77ee3023f77ec2aee0d96d14a07a98022957a1))

- Add `Switch.Application.self` ([`d149e690fa8f7a8beea08fc45c45bc6b48d9388d`](https://github.com/TooTallNate/nx.js/commit/d149e690fa8f7a8beea08fc45c45bc6b48d9388d))

- Detect `-0` in `Switch.inspect()` ([`d317d4a7a017fa54154921674ea88dc16a87bb1d`](https://github.com/TooTallNate/nx.js/commit/d317d4a7a017fa54154921674ea88dc16a87bb1d))

- Add type declaration for `queueMicrotask()` ([`f9562c0bb1ce854483d08e38ba685f129a40705f`](https://github.com/TooTallNate/nx.js/commit/f9562c0bb1ce854483d08e38ba685f129a40705f))

- Ensure `console.trace()` prints a trailing newline ([`5ab6126806dcfcad7e7faeda5acf70561c588126`](https://github.com/TooTallNate/nx.js/commit/5ab6126806dcfcad7e7faeda5acf70561c588126))

- Add `URL.canParse()` ([`7c3d2e0877941f28a5bf374f74422a3f4c8d88cc`](https://github.com/TooTallNate/nx.js/commit/7c3d2e0877941f28a5bf374f74422a3f4c8d88cc))

- Add `Application#version` property ([`4c9260e73f62e84f32d642e6399436902d67b3ec`](https://github.com/TooTallNate/nx.js/commit/4c9260e73f62e84f32d642e6399436902d67b3ec))

- Remove `Switch.applications`, merge it into `Switch.Application` ([`67f3853a75b36e736721008a617b172cbd4d1e28`](https://github.com/TooTallNate/nx.js/commit/67f3853a75b36e736721008a617b172cbd4d1e28))

- Use `ada-url` for `URL` and `URLSearchParams` - remove "core-js" dependency ([#89](https://github.com/TooTallNate/nx.js/pull/89))

- Include app's name and version in `navigator.userAgent` ([`97bc87d71543c17e5dd0c120a9f0a747e458d14c`](https://github.com/TooTallNate/nx.js/commit/97bc87d71543c17e5dd0c120a9f0a747e458d14c))

## 0.0.32

### Patch Changes

- Fix `Switch.remove()` and `Switch.removeSync()` when path does not exist ([`8311c6ebe340ab75308d82aaa0d7c2a8acdddb44`](https://github.com/TooTallNate/nx.js/commit/8311c6ebe340ab75308d82aaa0d7c2a8acdddb44))

## 0.0.31

### Patch Changes

- Add `TouchList` class ([`910cec0f598543d18943f00eaae35f2415945ffa`](https://github.com/TooTallNate/nx.js/commit/910cec0f598543d18943f00eaae35f2415945ffa))

## 0.0.30

### Patch Changes

- Fix `Switch.statSync()` ([`df7b564f2e6ee5a65a50afdc1c6d00d9c813984b`](https://github.com/TooTallNate/nx.js/commit/df7b564f2e6ee5a65a50afdc1c6d00d9c813984b))

- Fix `Switch.removeSync()` on files ([`977c19f13836e7cd0c243576ad2655db88bc49f1`](https://github.com/TooTallNate/nx.js/commit/977c19f13836e7cd0c243576ad2655db88bc49f1))

## 0.0.29

### Patch Changes

- Handle `"<input>"` as filename in stack trace ([`a6ac8db6b9ad87574cac9d0686e9df80e4387616`](https://github.com/TooTallNate/nx.js/commit/a6ac8db6b9ad87574cac9d0686e9df80e4387616))

- Set circular reference index for `Switch.inspect()` ([`b8da2c13f4615ddd8836734ac66e19c634576c09`](https://github.com/TooTallNate/nx.js/commit/b8da2c13f4615ddd8836734ac66e19c634576c09))

## 0.0.28

### Patch Changes

- Remove `null` from return type on `Switch.currentProfile({ required: true })` ([`cf32e5f878fe0d9ef35dcbfc057defbc4ed8d829`](https://github.com/TooTallNate/nx.js/commit/cf32e5f878fe0d9ef35dcbfc057defbc4ed8d829))

- Set `Response#redirected` property ([`95d9a3fea041cda06a62840f44b7f9c7bd16f070`](https://github.com/TooTallNate/nx.js/commit/95d9a3fea041cda06a62840f44b7f9c7bd16f070))

- Implement `crypto.subtle.digest()` for "sha-1" and "sha-256" ([`8510c7503c02e09ace469e5cbf29171791dc110a`](https://github.com/TooTallNate/nx.js/commit/8510c7503c02e09ace469e5cbf29171791dc110a))

- Implement fetch `redirect` handling ([`acc84059ad156298c8f918840701acf0582e7159`](https://github.com/TooTallNate/nx.js/commit/acc84059ad156298c8f918840701acf0582e7159))

## 0.0.27

### Patch Changes

- Remove `Socket` and `Server` from global scope ([`b0a4fa4509427f3c689839070afdfe2a9f49050e`](https://github.com/TooTallNate/nx.js/commit/b0a4fa4509427f3c689839070afdfe2a9f49050e))

- Set a default Title ID on the `nxjs.nro` file ([`b9e48e66e991cbdee98dd37e51a560d620addd06`](https://github.com/TooTallNate/nx.js/commit/b9e48e66e991cbdee98dd37e51a560d620addd06))

- Add `Application#createSaveData()` and `Application#mountSaveData()` ([`8faad62abf1d58d42b425827f23b4fadf40d27d9`](https://github.com/TooTallNate/nx.js/commit/8faad62abf1d58d42b425827f23b4fadf40d27d9))

- Fix global `addEventListener()` generic fallback type ([`4cd1854360689a19509d09f2e91b11a32f639a28`](https://github.com/TooTallNate/nx.js/commit/4cd1854360689a19509d09f2e91b11a32f639a28))

- Overhaul filesystem operations: ([`14657f0f6d14c411bfe050e51e3a8a245fcd9af2`](https://github.com/TooTallNate/nx.js/commit/14657f0f6d14c411bfe050e51e3a8a245fcd9af2))

  - Added `Switch.mkdirSync()`, `Switch.removeSync()`, `Switch.statSync()`
  - Read operations return `null` for `ENOENT`, instead of throwing an error
  - `Switch.remove()` and `Switch.removeSync()` work with directories, and delete recursively
  - `Switch.writeFileSync()` creates parent directories recursively as needed

- Add `Switch.profiles`, `Switch.currentProfile()`, `Switch.selectProfile()` ([#77](https://github.com/TooTallNate/nx.js/pull/77))

- Set the proper byte length of the `Application#icon` ([`4bb8a5c7b77e27d0be8123cb1934814a0a89f85b`](https://github.com/TooTallNate/nx.js/commit/4bb8a5c7b77e27d0be8123cb1934814a0a89f85b))

- Mark `Switch` and `WebAssembly` namespaces as non-enumerable ([`055c9f3b0b2ac35536604aa2dcb1efb6cf399bad`](https://github.com/TooTallNate/nx.js/commit/055c9f3b0b2ac35536604aa2dcb1efb6cf399bad))

- Fix successful TCP connect ([`43190bbf5b127b179dabc30ac86038005350ca5e`](https://github.com/TooTallNate/nx.js/commit/43190bbf5b127b179dabc30ac86038005350ca5e))

- Declare globals with `var` instead of `const` to make them visible on `globalThis` ([`e6e1cb042ba030acf362f90efd350ae186622fda`](https://github.com/TooTallNate/nx.js/commit/e6e1cb042ba030acf362f90efd350ae186622fda))

- Add `required: true` option to `Switch.currentProfile()`, and cache result for future calls ([`0f84ee1b4d823bac57de2e6c163d43917cc4757d`](https://github.com/TooTallNate/nx.js/commit/0f84ee1b4d823bac57de2e6c163d43917cc4757d))

## 0.0.26

### Patch Changes

- Set esbuild and TypeScript compile target to "es2022" ([`62951c61fb846aeb2201b21f8c6a03c8adae96e8`](https://github.com/TooTallNate/nx.js/commit/62951c61fb846aeb2201b21f8c6a03c8adae96e8))

- Add `ImageBitmap` class ([`d9db93b6c2b5dc33f817893f2c1aa142736009c6`](https://github.com/TooTallNate/nx.js/commit/d9db93b6c2b5dc33f817893f2c1aa142736009c6))

- Add `Sensor` base class ([`6ab19dcb6f5922a25805f3b1decfe02959bde362`](https://github.com/TooTallNate/nx.js/commit/6ab19dcb6f5922a25805f3b1decfe02959bde362))

- Make `IRSensor` use `ImageBitmap` instead of `ImageData` ([`f8d7beed76a2331ee83ef015046bd214a2156dda`](https://github.com/TooTallNate/nx.js/commit/f8d7beed76a2331ee83ef015046bd214a2156dda))

- Rename `ListenOpts` interface to `ListenOptions` ([`2964d88e253dccbb1fedd676bbcb68017530eef7`](https://github.com/TooTallNate/nx.js/commit/2964d88e253dccbb1fedd676bbcb68017530eef7))

- Propagate TCP `connect()` error to JS ([`9c3b9655a3360490906eb5106953ed9855536c53`](https://github.com/TooTallNate/nx.js/commit/9c3b9655a3360490906eb5106953ed9855536c53))

- Use `quickjs-ng` ([`ba52a51ca85c86649f36b13d41f2ae173c953de6`](https://github.com/TooTallNate/nx.js/commit/ba52a51ca85c86649f36b13d41f2ae173c953de6))

- Add `accept` shorthand for `Switch.listen()` ([`150467b4fd4f4c73e1d6d22578a45ba904289ea7`](https://github.com/TooTallNate/nx.js/commit/150467b4fd4f4c73e1d6d22578a45ba904289ea7))

- Return same Promise instance for `navigator.getBattery()` ([`41081fb0f01fb79cacc453d0832822a394c56df4`](https://github.com/TooTallNate/nx.js/commit/41081fb0f01fb79cacc453d0832822a394c56df4))

- Add `AmbientLightSensor` class ([`d7482c0931ae04f3270f447e1dcbd62954318a9a`](https://github.com/TooTallNate/nx.js/commit/d7482c0931ae04f3270f447e1dcbd62954318a9a))

- Add `Switch.IRSensor` class ([`804b9f379e8f0da33e6bd726da8a1d2584c2d354`](https://github.com/TooTallNate/nx.js/commit/804b9f379e8f0da33e6bd726da8a1d2584c2d354))

- Add `Switch.Application` class and `Switch.applications` iterator ([#72](https://github.com/TooTallNate/nx.js/pull/72))

- Fix printing error if `runtime.js` throws an error ([`91db41d4c0c86fe413a4330aeedb994a98b53a5c`](https://github.com/TooTallNate/nx.js/commit/91db41d4c0c86fe413a4330aeedb994a98b53a5c))

- Mark `id` as optional in `clearTimeout()` and `clearInterval()` ([`98654782f18c16e770124eaaefeef87e4b822396`](https://github.com/TooTallNate/nx.js/commit/98654782f18c16e770124eaaefeef87e4b822396))

- Return 16 for `navigator.maxTouchPoints` ([`e23ae02b96306915dccf299dfcb2fd31c75e0c1c`](https://github.com/TooTallNate/nx.js/commit/e23ae02b96306915dccf299dfcb2fd31c75e0c1c))

- Implement Canvas `roundRect()` in C ([#74](https://github.com/TooTallNate/nx.js/pull/74))

- Remove unused `ConnectOpts` interface ([`143b53126e199287ef86e506fafff3765a57c4a2`](https://github.com/TooTallNate/nx.js/commit/143b53126e199287ef86e506fafff3765a57c4a2))

- Add support for the `maxWidth` option in Canvas `fillText()`/`strokeText()` ([`09e1ac24a16d4e2ea9f484141cef7cc37373e241`](https://github.com/TooTallNate/nx.js/commit/09e1ac24a16d4e2ea9f484141cef7cc37373e241))

- Re-implement source mapping using CallSite API ([`59d3dec8427224137b4d6bcdf5a2701375741e9c`](https://github.com/TooTallNate/nx.js/commit/59d3dec8427224137b4d6bcdf5a2701375741e9c))

- Support `color` option in `Switch.IRSensor` ([`0ad81bb0c768903259d962fd66cace83aefccdd5`](https://github.com/TooTallNate/nx.js/commit/0ad81bb0c768903259d962fd66cace83aefccdd5))

- Add `depth` to `inspect()` ([`cca4251d4d569254b5a9d3079c881e6c3f10712f`](https://github.com/TooTallNate/nx.js/commit/cca4251d4d569254b5a9d3079c881e6c3f10712f))

- Refactor runtime type definition generator using AST modifications ([#73](https://github.com/TooTallNate/nx.js/pull/73))

## 0.0.25

### Patch Changes

- Dispatch `touchstart`, `touchmove`, and `touchend` events on the `screen` object ([`07619a955dc1c6884c011d6ec64ecd563fb911bb`](https://github.com/TooTallNate/nx.js/commit/07619a955dc1c6884c011d6ec64ecd563fb911bb))

- Use `performance.now()` in `requestAnimationFrame()` callback ([`6a40a1ea7bccbf0a9df83146c10ac65744fc4e4f`](https://github.com/TooTallNate/nx.js/commit/6a40a1ea7bccbf0a9df83146c10ac65744fc4e4f))

- Add missing `TextMetrics` type ([`475261f1cf04703a9bb443f52b18b7de81a1b066`](https://github.com/TooTallNate/nx.js/commit/475261f1cf04703a9bb443f52b18b7de81a1b066))

- Refactor to prevent `INTERNAL_SYMBOL` from leaking into the public types ([`2200e6eb8385db41600d34344dbd73ebc57b49a9`](https://github.com/TooTallNate/nx.js/commit/2200e6eb8385db41600d34344dbd73ebc57b49a9))

- Add Canvas `textBaseline` ([`df93c6fe8632504db94b596b04d957ca959fb815`](https://github.com/TooTallNate/nx.js/commit/df93c6fe8632504db94b596b04d957ca959fb815))

- Move rendering mode handling to C side ([`b14161f9baaf54de93f6a046aee428617fc72198`](https://github.com/TooTallNate/nx.js/commit/b14161f9baaf54de93f6a046aee428617fc72198))

- Move `keydown` and `keyup` events to the `window` object ([`04008ba8972c81f799546ad3785d708e2c2d2673`](https://github.com/TooTallNate/nx.js/commit/04008ba8972c81f799546ad3785d708e2c2d2673))

- Add `console.printErr()` ([`1085e6d854ea54fc27fcbd5a6cdf82c64b29aa70`](https://github.com/TooTallNate/nx.js/commit/1085e6d854ea54fc27fcbd5a6cdf82c64b29aa70))

- Add `console.print()` ([`ff4b8fa78a4e3c02ad918493f2dfc3688a3dda42`](https://github.com/TooTallNate/nx.js/commit/ff4b8fa78a4e3c02ad918493f2dfc3688a3dda42))

- Add Canvas `textAlign` ([`81275ee86bbb43bb967382e30a074e3dbb2ab2bb`](https://github.com/TooTallNate/nx.js/commit/81275ee86bbb43bb967382e30a074e3dbb2ab2bb))

- Add `performance.timeOrigin` and `performance.now()` ([`0433772037a99c7825d354649827f2bda272a7a4`](https://github.com/TooTallNate/nx.js/commit/0433772037a99c7825d354649827f2bda272a7a4))

- Move `Switch.vibrate()` to `navigator.vibrate()` ([`b0e81f33e5d1834e27a3b7bc3f9dc92bf473d478`](https://github.com/TooTallNate/nx.js/commit/b0e81f33e5d1834e27a3b7bc3f9dc92bf473d478))

- Convert `Switch` global into a proper "namespace" ([#70](https://github.com/TooTallNate/nx.js/pull/70))

- Move `Switch.fonts` to global `fonts` ([`52fb3910b6c571bfa65a7cb929b7ad4934f26abb`](https://github.com/TooTallNate/nx.js/commit/52fb3910b6c571bfa65a7cb929b7ad4934f26abb))

- Fix Canvas `font`, `fillStyle`, `strokeStyle` after `ctx.restore()` ([#68](https://github.com/TooTallNate/nx.js/pull/68))

- Don't throw when setting Canvas font upon parse failure ([`9f6c1cc508e3b89743db4feb985a10923b63b51e`](https://github.com/TooTallNate/nx.js/commit/9f6c1cc508e3b89743db4feb985a10923b63b51e))

- Remove `Switch.print()` method ([`5681e40f56d00a178ecfb71221406ea800ff97cc`](https://github.com/TooTallNate/nx.js/commit/5681e40f56d00a178ecfb71221406ea800ff97cc))

- Move `buttondown` and `buttonup` events to the `window` object ([`8c305fc1d8f77f283e4bb4e1e7889f7db220c273`](https://github.com/TooTallNate/nx.js/commit/8c305fc1d8f77f283e4bb4e1e7889f7db220c273))

- Make `console.debug()` write to the debug log file ([`a151db893bd0dbe1c51305da8e0772d73578314d`](https://github.com/TooTallNate/nx.js/commit/a151db893bd0dbe1c51305da8e0772d73578314d))

## 0.0.24

### Patch Changes

- Make Canvas `fill()` and `stroke()` preserve the drawing path ([`b6b8423c37f81cd8f48b8248f1cd0618f810af5b`](https://github.com/TooTallNate/nx.js/commit/b6b8423c37f81cd8f48b8248f1cd0618f810af5b))

- Use HarfBuzz for Canvas text placement and measurement ([#67](https://github.com/TooTallNate/nx.js/pull/67))

- Ensure `Path2D` constructor is defined globally ([`20cfe438e5985aab12c2660698b31c8f6468cf69`](https://github.com/TooTallNate/nx.js/commit/20cfe438e5985aab12c2660698b31c8f6468cf69))

- Fix Canvas `putImageData()` ([`1bb003b5a9eaecb2f996666e83ef97741a1e766f`](https://github.com/TooTallNate/nx.js/commit/1bb003b5a9eaecb2f996666e83ef97741a1e766f))

- Add Canvas `strokeText()` ([`f3c52b6567522a32869467e840e599d678c0fdbb`](https://github.com/TooTallNate/nx.js/commit/f3c52b6567522a32869467e840e599d678c0fdbb))

- Fix PNG and WebP image alpha channel handling ([`60c1f0bfe17c062f6294f10801dfe16aad046536`](https://github.com/TooTallNate/nx.js/commit/60c1f0bfe17c062f6294f10801dfe16aad046536))

## 0.0.23

### Patch Changes

- Add `CanvasRenderingContext2D#font` getter ([`e81ae48`](https://github.com/TooTallNate/nx.js/commit/e81ae48a71444249fb3bc96c20632af689144260))

- Log unhandled errors / promise rejections to the debug log file ([`2055162`](https://github.com/TooTallNate/nx.js/commit/20551622ae681174cccaaebc23346562907e3eb2))

- Add `Switch.version.mbedtls` ([`6c18661`](https://github.com/TooTallNate/nx.js/commit/6c18661fbf60d3472e5f411fa64276b7eb16935a))

- Use mbedtls to support Socket TLS `secureTransport: 'on'` ([#58](https://github.com/TooTallNate/nx.js/pull/58))

- Fix `resolve()` being inadventently added as a global function ([`275602f`](https://github.com/TooTallNate/nx.js/commit/275602f1404cd4a650cbf0c9d29b6b0f6a292221))

- Make `Event#preventDefault()` only work when `cancelable: true` ([`28addd4`](https://github.com/TooTallNate/nx.js/commit/28addd49da71cd710cad6b475dea5828b438f018))

- - Make `screen` implement the Canvas API ([#63](https://github.com/TooTallNate/nx.js/pull/63))
  - Add `OffscreenCanvas` and `OffscreenCanvasRenderingContext2D`
  - Remove `Switch.screen`

- Add Canvas `imageSmoothingQuality` ([`885de44`](https://github.com/TooTallNate/nx.js/commit/885de447309826af5c8a510d862cd953dcdda605))

- Add Canvas `imageSmoothingEnabled` ([`a55fcf2`](https://github.com/TooTallNate/nx.js/commit/a55fcf2612f69788ca694e7684c615f02b774e93))

- Add Canvas `globalCompositeOperation` ([`50e4168`](https://github.com/TooTallNate/nx.js/commit/50e4168707595f755111bafac3f4e0a02e066834))

- Add Canvas `clearRect()`, fix `strokeRect()` ([`94697da`](https://github.com/TooTallNate/nx.js/commit/94697da04d5449af9abd23a653b2d71f55a7a833))

- Add initial global `screen` object ([`057c5bf`](https://github.com/TooTallNate/nx.js/commit/057c5bf76290b8444c86eb1370320ee2bf61a035))

- Fix bug where `inspect()` would print "[Circular]" for object sub-properties ([`a197c84`](https://github.com/TooTallNate/nx.js/commit/a197c84d48badab5e0443bdf70e3c8a0d1222d6f))

- Add `navigator.platform` ([`79b5f00`](https://github.com/TooTallNate/nx.js/commit/79b5f00756bc71207500d51347adebd10096094d))

- Vendor `EventTarget` implementation ([`56189ae`](https://github.com/TooTallNate/nx.js/commit/56189ae177241296bf261d85eef817d8ed2cbd22))

- Add support for `https:` protocol in `fetch()` ([`cd8557b`](https://github.com/TooTallNate/nx.js/commit/cd8557b6a913e5db7b38f12ceba6d1123f0bd925))

- Enable color (emoji) fonts ([`412adc1`](https://github.com/TooTallNate/nx.js/commit/412adc1a5cdc5df4148a1b06c774aede029d5c93))

- Add `window` as alias to `globalThis` ([`06bdc88`](https://github.com/TooTallNate/nx.js/commit/06bdc88f4fd6119212efce69d0500087f604da42))

- Add `requestAnimationFrame()`, `cancelAnimationFrame()`, and global "unload" event ([`4e4f6ec`](https://github.com/TooTallNate/nx.js/commit/4e4f6ec13872b65f8d17e53ed2d4a5164f5d61f0))

  This is technically a breaking change since the `Switch` "frame" and "exit" events are no longer dispatched.

  To migrate from the "frame" event, use a `requestAnimationFrame` loop.
  To migrate from the Switch "exit" event, use a global "unload" event.

- Decode file path URL in filesystem operations ([`9fa8051`](https://github.com/TooTallNate/nx.js/commit/9fa8051a608876e7d6d4fd642c2e89f216ca5f8c))

## 0.0.22

### Patch Changes

- Add `navigator.virtualKeyboard` ([#55](https://github.com/TooTallNate/nx.js/pull/55))

- Fix memory leak in TCP `write()` function ([`8cffbfa`](https://github.com/TooTallNate/nx.js/commit/8cffbfab27e17c8c58cef49efed8510b50ec28fd))

- Fix false-positive errors in FS sync functions ([`c40faa9`](https://github.com/TooTallNate/nx.js/commit/c40faa92718f8d2197c2045afbc5fd24451b510a))

- Add initial TCP server API ([#54](https://github.com/TooTallNate/nx.js/pull/54))

- Print with multiple lines when inspecting large objects ([`59beb73`](https://github.com/TooTallNate/nx.js/commit/59beb73b1e1fb49880b471f6a47438f7313e0021))

- Add `DOMRectReadOnly` and `DOMRect` ([`ea2c8e9`](https://github.com/TooTallNate/nx.js/commit/ea2c8e9df3893f7b6ea7786a297c31308c3adb93))

- Add `ListenOpts` interface ([`798d601`](https://github.com/TooTallNate/nx.js/commit/798d601236f60bb0bb06e7c94810e4aa57d11a97))

- Add `Switch.networkInfo()` function ([`9dd5af9`](https://github.com/TooTallNate/nx.js/commit/9dd5af972c9459a5754bca4ae12094603399b00b))

- Resize poll file descriptors array when necessary ([`a2dddad`](https://github.com/TooTallNate/nx.js/commit/a2dddadde942096836683c3306eb7b96d23da15f))

- Detect `class` values in `inspect()` ([`4cc683e`](https://github.com/TooTallNate/nx.js/commit/4cc683e344da6c5f0c5c5578b88448d003a95b9d))

- Add `Socket` class, which is returned by `Switch.connect()` ([#57](https://github.com/TooTallNate/nx.js/pull/57))

- Support returning non-string in `inspect.custom` function ([`bde1ab0`](https://github.com/TooTallNate/nx.js/commit/bde1ab090be09a1fbc2a87a8fb959ca0133ab6e2))

## 0.0.21

### Patch Changes

- Add "utf8" as an accepted encoding for `TextDecoder` ([`b433831`](https://github.com/TooTallNate/nx.js/commit/b433831d7ad5491ebdfeff3ecc67b3f778c4d0da))

- Add `console.trace()` ([`cc6a443`](https://github.com/TooTallNate/nx.js/commit/cc6a443ff4b1e89414f098acda05dba5d6df31dc))

- Add `navigator.getBattery()` API ([`4d8f380`](https://github.com/TooTallNate/nx.js/commit/4d8f380357e2ce402b39ae8833d52d48fe6a7a0a))

- Make `globalThis` inherit from `EventTarget` ([`7f7d961`](https://github.com/TooTallNate/nx.js/commit/7f7d9610d74446b9af924ce05d0cb904891a53a5))

- Implement `WebAssembly.Memory#grow()` ([`ec79b19`](https://github.com/TooTallNate/nx.js/commit/ec79b199adb683d5f36a80b8c602dbe5b90176aa))

- Add "unhandledrejection" global event when a Promise is not caught ([`4eff094`](https://github.com/TooTallNate/nx.js/commit/4eff094b21fb48fa097ef5606e0902ded8d04f43))

- Fix reference to `BufferSource` type in WASM namespace ([`7541564`](https://github.com/TooTallNate/nx.js/commit/754156410b500159714c445e908b9bd2b9c72942))

- Add `navigator.maxTouchPoints` ([`2ef1bf8`](https://github.com/TooTallNate/nx.js/commit/2ef1bf8126be35610719cb0ecff9243a57c26e14))

- Add a few `EventTarget` related types / interfaces ([`a3b7ab6`](https://github.com/TooTallNate/nx.js/commit/a3b7ab664aad054a7081397c7df1a3a349d27fc0))

- Implement `import.meta` ([`612a208`](https://github.com/TooTallNate/nx.js/commit/612a2080f441045a728c4f109ce68312d78691ec))

- Free the arguments for thread pool callbacks ([`ddcc4b9`](https://github.com/TooTallNate/nx.js/commit/ddcc4b9ac093be4750b89d208c63019fe753b85c))

- Add type definitions for `EventTarget` interface on `globalThis` ([`e0f2439`](https://github.com/TooTallNate/nx.js/commit/e0f243931a75cdc9a778995a3d099a440e66da11))

- Continue event loop when `event.preventDefault()` is called for "error" or "unhandledrejection" ([`99ccc71`](https://github.com/TooTallNate/nx.js/commit/99ccc713db7f86ae99f6e3adc512df4f8a6b417f))

- Add source map tracing in error stack traces ([#48](https://github.com/TooTallNate/nx.js/pull/48))

- Add `ErrorEventInit` interface ([`99cf6dd`](https://github.com/TooTallNate/nx.js/commit/99cf6dd2989ea010d6763ebddaeb83310572c4ad))

- Add "error" global event when an Error is not caught ([`436f4d2`](https://github.com/TooTallNate/nx.js/commit/436f4d26cf5c93921738c99db19adb5172a49382))

- Implement initial `WebAssembly.Table` export functionality ([`9ef02b1`](https://github.com/TooTallNate/nx.js/commit/9ef02b117eac69b8563c0ac0877a52c6e639e2ed))

- Implement WASM imported function return value ([`a4fb327`](https://github.com/TooTallNate/nx.js/commit/a4fb32771f11eba2f00f1060626c18e9547ff889))

## 0.0.20

### Patch Changes

- Handle `WebAssembly.Memory` buffer growing ([`b950962`](https://github.com/TooTallNate/nx.js/commit/b95096252ac58ef69bf26ec05c492731fc840456))

- Delete the `leaks.txt` file when empty ([`11288bf`](https://github.com/TooTallNate/nx.js/commit/11288bfbc896042e9dc8dcc1a50024fdb6f8f513))

- Fix string memory leak in `Switch.writeFileSync()` ([`aa87ace`](https://github.com/TooTallNate/nx.js/commit/aa87acede559dab6d7a75153521cfdee46d8d95a))

- Fix NULL segfault when WASM module contains functions / globals without a name ([`d433882`](https://github.com/TooTallNate/nx.js/commit/d4338827bb0d0dc1407a35c1226032c46fbc6efd))

- Write QuickJS leaks detection output to `leaks.txt` file ([`78e4790`](https://github.com/TooTallNate/nx.js/commit/78e4790f3a6782d2f3938e7f7316a4133528b733))

- Implement WASM "memory" type import ([`ba0479a`](https://github.com/TooTallNate/nx.js/commit/ba0479a597ac160e05951fb38f50fa74300df959))

- Use Promise introspection APIs from updated QuickJS fork ([`a7c23d7`](https://github.com/TooTallNate/nx.js/commit/a7c23d7d35b64becad2f4374c86314bcef99b2c5))

- Fix string memory leak in `Switch.readFileSync()` ([`d5e032e`](https://github.com/TooTallNate/nx.js/commit/d5e032ebee780344e16e000da0e4d34b22e97b38))

- Fix another crash-on-exit ref counting issue ([`254d799`](https://github.com/TooTallNate/nx.js/commit/254d799b230a86c6e05e505ab79949273d6c6c44))

- Fix object memory leak in `CanvasRenderingContext2D` ([`cf578d8`](https://github.com/TooTallNate/nx.js/commit/cf578d8efe6ca7ccbe3baec9b15d04d36f1558d6))

## 0.0.19

### Patch Changes

- Add `navigator` global with `userAgent` property ([`2331b1e`](https://github.com/TooTallNate/nx.js/commit/2331b1e5ae8e3f0b13cfa03bb65997de4061dc91))

- Add `Switch.version.wasm3` ([`8ad22c4`](https://github.com/TooTallNate/nx.js/commit/8ad22c470baf40db1099d306fa27af55f01b6329))

- Make `data:` scheme URLs work with `fetch()` ([`9a9dee4`](https://github.com/TooTallNate/nx.js/commit/9a9dee4e819e9c623606e65aa54661cbd665d1d5))

- Make the `console` functions handle printf formatters and multiple arguments ([`d22e5d2`](https://github.com/TooTallNate/nx.js/commit/d22e5d290107c0a200f1638e796fe57ac936a949))

- Make `fetch()` URL resolve relative to `Switch.entrypoint` ([`cee00d0`](https://github.com/TooTallNate/nx.js/commit/cee00d0959fc163acbd1d989c5d9abdb19467c05))

- Free JS references to make process exit cleanly ([`dfa97ae`](https://github.com/TooTallNate/nx.js/commit/dfa97ae8f87cf99680cae5f64661110b0e1de3e8))

- Fix setting `WebAssembly.Global` value before being "bound" to a WASM module ([`0e3264e`](https://github.com/TooTallNate/nx.js/commit/0e3264e6a2ac6aaae7f01a7edca93d58aa409229))

## 0.0.18

### Patch Changes

- Add `console.debug()` as an alias for `console.log()` ([`81c6692`](https://github.com/TooTallNate/nx.js/commit/81c669297868ca19e21287d4ffd6ba06443480a3))

- Add custom inspector for `URL` instances ([`b44ff24`](https://github.com/TooTallNate/nx.js/commit/b44ff2480a55d7f8a7424fd0ede60b82c8eef732))

- Add docs for global `crypto` instance ([`b0b02b7`](https://github.com/TooTallNate/nx.js/commit/b0b02b78ae5c705fb391f1b486cfad9f22561ced))

- Add `WebAssembly` (WASM) implementation ([#12](https://github.com/TooTallNate/nx.js/pull/12))

- Fix `setTimeout()` / `clearTimeout()` bundle defined name ([`05a0ad6`](https://github.com/TooTallNate/nx.js/commit/05a0ad65fe09e06c5f621335193a6f7692127561))

- Fix spacing when rendering Error stack in `inspect()` ([`7d5d94c`](https://github.com/TooTallNate/nx.js/commit/7d5d94c205fe1039a7664e443dadbd0cd058601a))

- Make `ReadableStream` work with `for await...of` loops ([`27cb966`](https://github.com/TooTallNate/nx.js/commit/27cb9661b232d0cfeb12745cc189a2662d57fab5))

- Fix `inspect()` when `constructor` is falsy ([`daae521`](https://github.com/TooTallNate/nx.js/commit/daae52108957062ff37d13524b72321a58331060))

- Make `Switch.cwd()` return a string ([`7acbfad`](https://github.com/TooTallNate/nx.js/commit/7acbfad091a8626771a3e98c1bb2e157133ea7ce))

## 0.0.17

### Patch Changes

- Mark `DOMPoint.fromPoint()` as static ([`2c31aea`](https://github.com/TooTallNate/nx.js/commit/2c31aea00700e7aff1785eac76017e4d720e2cc9))

- Add esbuild bundle script ([`1e20a42`](https://github.com/TooTallNate/nx.js/commit/1e20a42b4c8f971fb72594682e691aca2fc56450))

- Remove private class field usage from `Blob` ([`f1a4837`](https://github.com/TooTallNate/nx.js/commit/f1a483751f49cc1a766937f6e9e7c6efa2f372ca))

- Move a few Event-related interfaces in-house ([`93c5c59`](https://github.com/TooTallNate/nx.js/commit/93c5c59603fa09269de1f4f7afea74d392bd154d))

- Update `typescript` to v5.2.2 ([`7e03f10`](https://github.com/TooTallNate/nx.js/commit/7e03f10787a30087d40509fef563c1349bb9b860))

- More robust filtering of `globalThis` interfaces on compiled type definitions ([`07801fe`](https://github.com/TooTallNate/nx.js/commit/07801fe9dbd1d04ed6e97ac5f8fdfb3fb1a0c349))

- Add support for DOMPoint value for `radii` parameter in `Canvas#roundRect()` ([`deee32f`](https://github.com/TooTallNate/nx.js/commit/deee32fe4aa62beb10eb92f5d2f1ca8afb394066))

## 0.0.16

### Patch Changes

- Use TypeDoc to generate docs website ([`ef53cef`](https://github.com/TooTallNate/nx.js/commit/ef53cef1c57e66f0855df92977cc6f0d4a17de27))

- Rename `Switch` class to `SwitchClass` ([`ef758a6`](https://github.com/TooTallNate/nx.js/commit/ef758a69df898a12ceaab809e9bd5327cb8c2041))

- Add docs for `URLSearchParams` ([`4560eef`](https://github.com/TooTallNate/nx.js/commit/4560eef63b0edcc5ba40aeba5a0880e4ccc67287))

- Remove `INTERNAL_SYMBOL` from `CanvasRenderingContext2D` ([`583cfd4`](https://github.com/TooTallNate/nx.js/commit/583cfd458e615264ae11a5a3a84f6c8450c2c38d))

- Remove instances of `globalThis` from types ([`62ceed4`](https://github.com/TooTallNate/nx.js/commit/62ceed41402bcddfc2216c2782a702789f3204f9))

- Disallow "body" for `GET`/`HEAD` requests ([`e60d92a`](https://github.com/TooTallNate/nx.js/commit/e60d92affebcb7a819d5f32c27aeabc52fc7b0b1))

- Add `Switch.vibrate()` ([`e742556`](https://github.com/TooTallNate/nx.js/commit/e742556c9110f786be80a2b756cff7b1c32f4506))

- Add "png", "turbojpeg" and "webp" to `Switch.version` ([`6596d91`](https://github.com/TooTallNate/nx.js/commit/6596d919f7ef8dd97f06cb1cb9ab48a4aed0bc32))

- Remove "es2020.intl" since `Intl` is not implemented in QuickJS ([`69f38a2`](https://github.com/TooTallNate/nx.js/commit/69f38a24f1b1bc19d60361a125992b16f7cd27e3))

- Begin moving DOM interfaces in-house ([`c45b11b`](https://github.com/TooTallNate/nx.js/commit/c45b11b172555b0970b6d2b2cd9efb1361e2d904))

- Add `DOMPoint` and `DOMPointReadOnly` ([`8919b14`](https://github.com/TooTallNate/nx.js/commit/8919b14e5b8c41309cc131b371cc98cec1a95424))

- Clean up some libnx services upon exit ([`3cfb62a`](https://github.com/TooTallNate/nx.js/commit/3cfb62a3894afccf15b24436725ed53c0e126c17))

- Add docs for `EventTarget` ([`6dd003d`](https://github.com/TooTallNate/nx.js/commit/6dd003d15eaac45daec17f393b52d7e3ad5ba783))

- Use `dts-bundle-generator` to create runtime type definitions ([`4da0301`](https://github.com/TooTallNate/nx.js/commit/4da0301296828119305d5c40539514901d2f9fee))

- Add types for all Web Streams classes/interfaces ([`6f050e8`](https://github.com/TooTallNate/nx.js/commit/6f050e8560c628afdb565fa60347fc885f4b9156))

- Fix buffer size compile warning ([`0860f46`](https://github.com/TooTallNate/nx.js/commit/0860f463e392ab34a66a8006fc035795a1903e69))

- Add `Headers#getSetCookie()` ([`60ac070`](https://github.com/TooTallNate/nx.js/commit/60ac070d109aa89fb00ff800cf1f424d2614e1aa))

## 0.0.15

## 0.0.14

### Patch Changes

- Add `File` ([`192b1d1`](https://github.com/TooTallNate/nx.js/commit/192b1d13ea61c8a062003a95a06739cb795d4d3c))

- Add `FormData` ([`f21f042`](https://github.com/TooTallNate/nx.js/commit/f21f042c18032bc5d23540d4fd7fbe9ce2fd2e6c))

- Remove `Symbol.hasInstance` from `Blob` ([`6f5ef58`](https://github.com/TooTallNate/nx.js/commit/6f5ef58c77bfa9a456c690fcc708253a1e1c36d3))

- Fix `CanvasRenderingContext2D` class to be compatible with global type definition ([`019adac`](https://github.com/TooTallNate/nx.js/commit/019adac50c66e269de1319d68d68f3a185769f7d))

- Updated logo with vector ([#20](https://github.com/TooTallNate/nx.js/pull/20))

- Set `Symbol.toStringTag` on all global classes ([`e888b51`](https://github.com/TooTallNate/nx.js/commit/e888b51fbb83f13cd1fba03f4459e9b775037e5a))

- Add support for `blob:` URLs in `fetch()` ([`9f663e9`](https://github.com/TooTallNate/nx.js/commit/9f663e9a0e0ae1c20d095c324993e3fbdbce28f3))

- Add `ImageData` as a global ([`a2473e6`](https://github.com/TooTallNate/nx.js/commit/a2473e6d1db3d7b20a299089382562bed8de4d69))

- Fix Canvas `ctx.font` setter to not throw upon empty string ([`4ddb225`](https://github.com/TooTallNate/nx.js/commit/4ddb225c6280c1f7885f73120e8e1687a69ec41c))

- Add `URL.createObjectURL()` and `URL.revokeObjectURL()` ([`9905c8a`](https://github.com/TooTallNate/nx.js/commit/9905c8abbd1781fa9a3fffee650702aa9a5fe14f))

- Support `FormData` encode/decode in `Body` class ([`1df3bfe`](https://github.com/TooTallNate/nx.js/commit/1df3bfe09a3ea717071c80dae3a42c46caab022b))

- Fix passing `File` instance to `FormData` ([`ed38043`](https://github.com/TooTallNate/nx.js/commit/ed380430119d61285b41877bef4351295e0fe0f1))

- Support `path` parameter in Canvas `fill()` method ([`756b5cf`](https://github.com/TooTallNate/nx.js/commit/756b5cf98cc29e50ca4c554ac6ffd6d64e9d0039))

- Add `crypto.getRandomValues()` and `crypto.randomUUID()` ([`f4d007c`](https://github.com/TooTallNate/nx.js/commit/f4d007c15160c99caf020c7f5b49c93bf3a54fdf))

- Prompt for package manager (pnpm, npm or yarn) and install dependencies ([`a43c1ac`](https://github.com/TooTallNate/nx.js/commit/a43c1acae2d6b6c1a32f26ef6d88b0daa891dff3))

- Add `Path2D` ([`be90649`](https://github.com/TooTallNate/nx.js/commit/be90649022f7b7c56209cdcad338330dd618d2c9))

## 0.0.13

### Patch Changes

- Add `Image` (currently only PNG support) ([`0227d73`](https://github.com/TooTallNate/nx.js/commit/0227d73da26eaa65b38283e0c68282476617f648))

- Add Canvas `drawImage()` ([`9efc267`](https://github.com/TooTallNate/nx.js/commit/9efc267b4ebf3648ad199103665ea1ece4df73d6))

- Add support for WebP in `Image` ([`0fad48d`](https://github.com/TooTallNate/nx.js/commit/0fad48d0bb8acf2518972cc2aeb79b61e5c99d9d))

- Add Canvas `globalAlpha` ([`ccb10f5`](https://github.com/TooTallNate/nx.js/commit/ccb10f5da28440da6e59e2efbd42776875f70cfa))

- Add support for JPEG in `Image` ([`5fb65a4`](https://github.com/TooTallNate/nx.js/commit/5fb65a445beb33750eac0e68ecc84f981d226aed))

## 0.0.12

### Patch Changes

- Add `Switch.writeFileSync()` function ([`7ac3cca`](https://github.com/TooTallNate/nx.js/commit/7ac3cca46f9042399c8164ede4eceacb82c0f4bc))

- Add `Blob` ([`9c1b3d1`](https://github.com/TooTallNate/nx.js/commit/9c1b3d1e19f55285b7cff2b487221a045419c954))

- Fix Canvas `putImageData()` ([`8d7552b`](https://github.com/TooTallNate/nx.js/commit/8d7552bc30089becddbca2f1b2799fb44f11f88a))

- Fix Canvas `getImageData()` ([`c10a105`](https://github.com/TooTallNate/nx.js/commit/c10a105e0ccbe3004d51be65bb61c94cf7547943))

- Add Canvas `clip()` ([`af7a606`](https://github.com/TooTallNate/nx.js/commit/af7a606b1c2ec7f747f602b1374efd30f75a65e2))

- Add Canvas `roundRect()` ([`45321a6`](https://github.com/TooTallNate/nx.js/commit/45321a654995c45cb43baf72b388ee2d4d768e11))

- Better organization for polyfills ([`31dd7ad`](https://github.com/TooTallNate/nx.js/commit/31dd7ad2ecb83808da659df684a0c7713fc2e797))

- Add Canvas `save()` and `restore()` ([`34f2756`](https://github.com/TooTallNate/nx.js/commit/34f275647e3e556ac9cda82e7504112ba0f4dbe6))

- Add Canvas `strokeRect()` ([`2050801`](https://github.com/TooTallNate/nx.js/commit/20508014f63147b817d7b0615f5c02270e99864e))

- Add initial `fetch()` implementation. Includes globals: ([`f4ecc23`](https://github.com/TooTallNate/nx.js/commit/f4ecc23a8db1311ad73130646e180a0e028c1cde))

  - `Headers`
  - `Request`
  - `Response`
  - `fetch()`

- Add support for selecting fonts with modifications (weight, etc.) ([`67e97ce`](https://github.com/TooTallNate/nx.js/commit/67e97ce384a8c5a2d1a4fce8ea5cb52033ba2278))

## 0.0.11

### Patch Changes

- Add `ErrorEvent` polyfill ([`af2a932`](https://github.com/TooTallNate/nx.js/commit/af2a9329a575642ea7a5d547cd906af03dd406e7))

- Add Canvas `beginPath()`, `closePath()`, `fill()`, `stroke()`, `moveTo()`, `lineTo()`, `rect()` ([`a54f605`](https://github.com/TooTallNate/nx.js/commit/a54f60509d618b5128cd29e11f62fdc2348ee3d7))

- Move all dependencies to dev, since they get bundled ([`a4ea4f2`](https://github.com/TooTallNate/nx.js/commit/a4ea4f2134d6afc4f8149120f03b0ace88b7d623))

- Set app title to `nx.js` and add logo to `.nro` file ([`d4b1996`](https://github.com/TooTallNate/nx.js/commit/d4b19961b3799fe3f9725a5670329f11a8673452))

- Add Canvas `lineJoin`, `lineCap`, and `lineDashOffset` ([`26ee360`](https://github.com/TooTallNate/nx.js/commit/26ee36002169b356787e6629490e9eaf834ed5e9))

- Add Canvas `strokeStyle` ([`c0ba341`](https://github.com/TooTallNate/nx.js/commit/c0ba34116470bf79f9594c2de0db00f930c39cc6))

- Add `clientX` and `clientY` to touch events ([`dcc0519`](https://github.com/TooTallNate/nx.js/commit/dcc0519dd234d827255ffc05c08637c05c023ada))

- Add Canvas `getTransform()`, `bezierCurveTo()`, and `quadraticCurveTo()` ([`eb6ae49`](https://github.com/TooTallNate/nx.js/commit/eb6ae49b99a8e4e546c5ac9e76469d3ea28c55ea))

- Prevent further code execution upon initialization error ([`254ae9c`](https://github.com/TooTallNate/nx.js/commit/254ae9cfeea64acad08cf5707d239dbc2d9ab1c1))

- Add Canvas `setTransform()`, `resetTransform()`, `getLineDash()`, `setLineDash()`, and `lineWidth` getter ([`1e15670`](https://github.com/TooTallNate/nx.js/commit/1e156707875245e23f3d9d9ef3c15171cd6be209))

- Add `TextEncoder` polyfill and use better `TextDecoder` implementation ([`fe7fba0`](https://github.com/TooTallNate/nx.js/commit/fe7fba019ab22e11d6beabdd492fd7741ed5eed2))

- Add Canvas `arc()`, `arcTo()`, and `ellipse()` ([`6bf58b5`](https://github.com/TooTallNate/nx.js/commit/6bf58b582b6036879bd5afb20d83a1bbb2fd2ff7))

- Add initial `poll()` implementation ([#15](https://github.com/TooTallNate/nx.js/pull/15))

  - Adds `Switch.connect()`
  - Adds `Switch.read()`
  - Adds `Swtich.write()`

- Add Canvas `transform()` ([`8afa279`](https://github.com/TooTallNate/nx.js/commit/8afa279e3f51f4aae1c318f81ed8fcf736a23173))

- Add canvas context `rotate()`, `translate()` and `scale()` ([`674cc05`](https://github.com/TooTallNate/nx.js/commit/674cc05fd2888809fbd89c5a17473552bc3daf61))

- Fix `read_file()` NULL termination off-by-one error ([`902de42`](https://github.com/TooTallNate/nx.js/commit/902de42dfc0e968bf2e549048552e0fd3c160980))

- Accept `BufferSource` in `Switch.read()` and string in `Switch.write()` ([`b3a0810`](https://github.com/TooTallNate/nx.js/commit/b3a0810d0482e781c00799f8e589a4c6958f3686))

- Add Canvas `miterLimit` ([`02722a5`](https://github.com/TooTallNate/nx.js/commit/02722a553dcd900eb2822b13b96630e502540d8f))

## 0.0.10

### Patch Changes

- 6074fb2: Add web streams polyfill
- 835efcf: Fix infinite loop upon circular references in `Switch.inspect()`
- 150e55c: Add `Switch.chdir()`
- 209ebb8: Fix indexing in `Switch.argv`
- 01befe9: Fix `Switch.readDirSync()` throwing an error upon failure
- b250662: Set NACP version for output `nxjs.nro`
- 806737b: Add `Switch.stat()`
- b0dde04: Fix memory leak in `Switch.env.toObject()`
- 168a024: Add `Promise` introspection to `Switch.inspect()`
- 95e5954: Add `Switch.remove()`
- 26d38f0: Add `Switch.env.delete()` and add error handling for env `get()` and `set()`
- 091683b: Add class name when inspecting objects in `Switch.inspect()`
- 97afca5: Add `Switch.version` object
- ce8f8ac: Fix resolving more than one IP address in `Switch.resolveDns()`
- 901d941: Add `AbortController` and `AbortSignal` polyfills

## 0.0.9

### Patch Changes

- fd5b863: Disable err printing on thpool
  (for some reason, this fixes printing without a newline, for example on the REPL app)
- 19e11c7: Add threadpool, with new asynchronous functions:
  - `Switch.readFile() -> Promise<ArrayBuffer>`
  - `Switch.resolveDns() -> Promise<string[]>`
- b050eb6: Fix segfault upon exit when rendering in "console" mode

## 0.0.8

### Patch Changes

- c553611: Properly initialize FontFace class ID
- 82452c4: Add `console.error()`
- ab44617: Add `Switch.entrypoint`
- 7de26ff: Render `undefined` as grey
- 3cb2a94: Add support for `Promise` fulfillment
- d3b658b: Add `console.warn()`
- 94a90f1: Add `Switch.inspect()`
- b22fd00: Add yellow coloring to `console.warn()`
- 81d325b: Add Error inspection and custom inspect symbol
- dcdeefb: Add `Switch.argv` TypeScript definition
- 91ec2ec: Add `Switch.native.appletGetAppletType()` and `AppletType` enum to constants package

## 0.0.7

### Patch Changes

- 02b388c: Make `console.log` be a bound function
- b084cf7: Fix backslash key mapping
- 9b52d78: Filter out "." and ".." from `Switch.readDirSync()`

## 0.0.6

### Patch Changes

- 0cc389c: NULL terminate the source code file reads
- 24e9dff: Add initial keyboard support with `keydown` and `keyup` events
- d0819bf: Remove test Class ID code

## 0.0.5

### Patch Changes

- 4bc3271: .

## 0.0.4

### Patch Changes

- b889930: Debugging release workflow

## 0.0.3

### Patch Changes

- a0867ca: Debug release workflow

## 0.0.2

### Patch Changes

- 343c3d7: Begin implementing `ctx.measureText()`

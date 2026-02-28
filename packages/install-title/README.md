# `@nx.js/install-title`

Title installer for [nx.js](https://nxjs.n8.io). Supports installing Nintendo Switch titles from NSP, NSZ, XCI, and XCZ files.

## Usage

```typescript
import { install } from '@nx.js/install-title';
import { NcmStorageId } from '@nx.js/ncm';

const file = Switch.file('sdmc:/game.nsp');

for await (const step of install(file, NcmStorageId.SdCard)) {
  switch (step.type) {
    case 'start':
      console.log(`Starting: ${step.name}`);
      break;
    case 'end':
      console.log(
        `Done: ${step.name} (${(step.timeEnd - step.timeStart).toFixed(0)}ms)`,
      );
      break;
    case 'progress':
      console.log(`${step.name}: ${step.processed}/${step.total}`);
      break;
  }
}

console.log('Installation complete!');
```

## Supported Formats

| Format | Description                                |
| ------ | ------------------------------------------ |
| NSP    | Nintendo Submission Package (PFS0)         |
| NSZ    | Compressed NSP (contains NCZ-compressed NCAs) |
| XCI    | GameCard Image (HFS0 partitions)           |
| XCZ    | Compressed XCI (contains NCZ-compressed NCAs) |

The format is auto-detected from magic bytes. You can also specify it explicitly:

```typescript
for await (const step of install(file, NcmStorageId.SdCard, { format: 'xci' })) {
  // ...
}
```

## API

### `install(data, storageId, options?)`

Install a title from a `Blob` containing an NSP, NSZ, XCI, or XCZ file. The format is auto-detected from magic bytes unless `options.format` is specified. Yields `Step` progress events as the installation proceeds.

- **`data`** — The file data as a `Blob` (e.g. `Switch.File`).
- **`storageId`** — Target storage (e.g. `NcmStorageId.SdCard`).
- **`options.format`** — Optional format hint: `'nsp'`, `'nsz'`, `'xci'`, or `'xcz'`.

Returns an `AsyncIterable<Step>`.

### `installNsp(data, storageId)`

Convenience wrapper around `install()` that explicitly sets the format to `'nsp'`.

### Step Events

The `install()` generator yields progress events:

- **`StepStart`** — `{ type: 'start', name, timeStart }`
- **`StepEnd`** — `{ type: 'end', name, timeStart, timeEnd }`
- **`StepProgress`** — `{ type: 'progress', name, processed, total }`

## How It Works

1. **Parse container** — NSP/NSZ uses PFS0 (`@tootallnate/nsp`), XCI/XCZ uses HFS0 (`@tootallnate/xci`)
2. **Install content meta** — Installs `.cnmt.nca` files, mounts them to read CNMT metadata
3. **Import tickets** — Imports `.tik` and `.cert` files via the ES service
4. **Install NCAs** — Writes NCA files to content storage via NCM placeholders. NCZ files are transparently decompressed and re-encrypted on the fly via `@tootallnate/ncz`
5. **Push application records** — Registers the title with the NS service

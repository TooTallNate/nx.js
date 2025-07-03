# Changelog

## 0.0.1

Initial release of `@nx.js/hosversion`.

### Features

- **`hosversionGet()`**: Get the current HOS version as a 32-bit integer, mirroring libnx's `hosversionGet()` function
- **`hosversionAtLeast(major, minor, micro)`**: Check if the current HOS version is at least the specified version, mirroring libnx's `hosversionAtLeast()` function
- **`hosversionBelow(major, minor, micro)`**: Convenience function to check if the current HOS version is below the specified version
- **`HOSVER_MAJOR(version)`**: Extract the major version component from a HOS version integer, mirroring libnx's `HOSVER_MAJOR()` macro
- **`HOSVER_MINOR(version)`**: Extract the minor version component from a HOS version integer, mirroring libnx's `HOSVER_MINOR()` macro
- **`HOSVER_MICRO(version)`**: Extract the micro version component from a HOS version integer, mirroring libnx's `HOSVER_MICRO()` macro
- **`HOSVER_MAKE(major, minor, micro)`**: Create a HOS version integer from major, minor, and micro components
- **`hosversionToString(version)`**: Convert a HOS version integer to a string representation
- **`hosversionSet(major, minor, micro)`**: Override the cached HOS version for testing purposes, mirroring libnx's `hosversionSet()` function
- **`hosversionReset()`**: Reset the cached HOS version to force re-parsing from `Switch.version.hos`
- **`hosversionParse(version)`**: Parse a HOS version integer into its components

### Key Implementation Details

- Uses **inverse logic** compared to the C version: derives version information from `Switch.version.hos` instead of system calls
- `Switch.version.hos` is the source of truth for version information
- All other functions like `hosversionAtLeast()` are based off of `hosversionGet()`
- Maintains API compatibility with switchbrew/libnx hosversion interface
- Supports caching for performance
- Comprehensive TypeScript types and documentation
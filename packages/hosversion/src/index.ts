/**
 * HOS version detection utilities for nx.js, mirroring the switchbrew/libnx hosversion interface.
 * 
 * This package provides functions to detect and compare Nintendo Switch Horizon OS (HOS) versions,
 * using the same API as the libnx hosversion functions but with inverse logic - deriving the
 * version information from `Switch.version.hos` instead of system calls.
 * 
 * @example
 * ```typescript
 * import { hosversionGet, hosversionAtLeast, HOSVER_MAJOR, HOSVER_MINOR, HOSVER_MICRO } from '@nx.js/hosversion';
 * 
 * // Get the current HOS version as a 32-bit integer
 * const version = hosversionGet();
 * 
 * // Extract version components
 * const major = HOSVER_MAJOR(version);
 * const minor = HOSVER_MINOR(version);
 * const micro = HOSVER_MICRO(version);
 * 
 * console.log(`HOS version: ${major}.${minor}.${micro}`);
 * 
 * // Check if running on at least HOS 14.0.0
 * if (hosversionAtLeast(14, 0, 0)) {
 *   console.log('Running on HOS 14.0.0 or later');
 * }
 * ```
 */

/**
 * Cached HOS version value to avoid repeated parsing
 */
let cachedHosVersion: number | null = null;

/**
 * Parse the HOS version string and convert it to the 32-bit integer format
 * used by libnx. Format: (major << 16) | (minor << 8) | micro
 * 
 * @param versionString - The version string in "major.minor.micro" format
 * @returns The version as a 32-bit integer
 */
function parseHosVersionString(versionString: string): number {
  const parts = versionString.split('.').map(Number);
  
  if (parts.length !== 3 || parts.some(isNaN)) {
    throw new Error(`Invalid HOS version format: ${versionString}. Expected "major.minor.micro"`);
  }
  
  const [major, minor, micro] = parts;
  
  // Validate ranges (reasonable limits)
  if (major < 0 || major > 255 || minor < 0 || minor > 255 || micro < 0 || micro > 255) {
    throw new Error(`HOS version components out of range: ${versionString}. Each component must be 0-255`);
  }
  
  return (major << 16) | (minor << 8) | micro;
}

/**
 * Gets the HOS version as a 32-bit integer, mirroring libnx's `hosversionGet()`.
 * 
 * This function uses the inverse logic of the C version - instead of querying
 * the system for version information, it derives the version from the
 * `Switch.version.hos` property which is the source of truth in nx.js.
 * 
 * @returns The HOS version as a 32-bit integer in the format: (major << 16) | (minor << 8) | micro
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * // If Switch.version.hos is "14.1.2", this returns 0x0E0102 (921858)
 * ```
 */
export function hosversionGet(): number {
  if (cachedHosVersion === null) {
    cachedHosVersion = parseHosVersionString(Switch.version.hos);
  }
  return cachedHosVersion;
}

/**
 * Checks if the current HOS version is at least the specified version,
 * mirroring libnx's `hosversionAtLeast()`.
 * 
 * @param major - Major version component
 * @param minor - Minor version component  
 * @param micro - Micro version component
 * @returns `true` if the current HOS version is greater than or equal to the specified version
 * 
 * @example
 * ```typescript
 * // Check if running on HOS 14.0.0 or later
 * if (hosversionAtLeast(14, 0, 0)) {
 *   console.log('Can use features introduced in HOS 14.0.0');
 * }
 * 
 * // Check for a specific patch version
 * if (hosversionAtLeast(13, 2, 1)) {
 *   console.log('Running on HOS 13.2.1 or later');
 * }
 * ```
 */
export function hosversionAtLeast(major: number, minor: number, micro: number): boolean {
  const targetVersion = (major << 16) | (minor << 8) | micro;
  const currentVersion = hosversionGet();
  return currentVersion >= targetVersion;
}

/**
 * Checks if the current HOS version is below the specified version.
 * This is a convenience function that's the inverse of `hosversionAtLeast()`.
 * 
 * @param major - Major version component
 * @param minor - Minor version component
 * @param micro - Micro version component
 * @returns `true` if the current HOS version is less than the specified version
 * 
 * @example
 * ```typescript
 * // Check if running on a version before HOS 14.0.0
 * if (hosversionBelow(14, 0, 0)) {
 *   console.log('Consider upgrading to HOS 14.0.0 or later');
 * }
 * ```
 */
export function hosversionBelow(major: number, minor: number, micro: number): boolean {
  return !hosversionAtLeast(major, minor, micro);
}

/**
 * Extracts the major version component from a HOS version integer,
 * mirroring libnx's `HOSVER_MAJOR()` macro.
 * 
 * @param version - The HOS version as a 32-bit integer
 * @returns The major version component (0-255)
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * const major = HOSVER_MAJOR(version);
 * console.log(`Major version: ${major}`);
 * ```
 */
export function HOSVER_MAJOR(version: number): number {
  return (version >> 16) & 0xFF;
}

/**
 * Extracts the minor version component from a HOS version integer,
 * mirroring libnx's `HOSVER_MINOR()` macro.
 * 
 * @param version - The HOS version as a 32-bit integer
 * @returns The minor version component (0-255)
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * const minor = HOSVER_MINOR(version);
 * console.log(`Minor version: ${minor}`);
 * ```
 */
export function HOSVER_MINOR(version: number): number {
  return (version >> 8) & 0xFF;
}

/**
 * Extracts the micro version component from a HOS version integer,
 * mirroring libnx's `HOSVER_MICRO()` macro.
 * 
 * @param version - The HOS version as a 32-bit integer
 * @returns The micro version component (0-255)
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * const micro = HOSVER_MICRO(version);
 * console.log(`Micro version: ${micro}`);
 * ```
 */
export function HOSVER_MICRO(version: number): number {
  return version & 0xFF;
}

/**
 * Creates a HOS version integer from major, minor, and micro components.
 * This is a utility function that's the inverse of the HOSVER_* extraction functions.
 * 
 * @param major - Major version component (0-255)
 * @param minor - Minor version component (0-255)
 * @param micro - Micro version component (0-255)
 * @returns The HOS version as a 32-bit integer
 * 
 * @example
 * ```typescript
 * const version = HOSVER_MAKE(14, 1, 2);
 * console.log(`Version: ${version}`); // 921858 (0x0E0102)
 * ```
 */
export function HOSVER_MAKE(major: number, minor: number, micro: number): number {
  if (major < 0 || major > 255 || minor < 0 || minor > 255 || micro < 0 || micro > 255) {
    throw new Error(`Version components out of range. Each component must be 0-255. Got: ${major}.${minor}.${micro}`);
  }
  return (major << 16) | (minor << 8) | micro;
}

/**
 * Converts a HOS version integer back to a string representation.
 * This is a utility function for debugging and display purposes.
 * 
 * @param version - The HOS version as a 32-bit integer
 * @returns The version as a string in "major.minor.micro" format
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * const versionString = hosversionToString(version);
 * console.log(`Current HOS version: ${versionString}`);
 * ```
 */
export function hosversionToString(version: number): string {
  const major = HOSVER_MAJOR(version);
  const minor = HOSVER_MINOR(version);
  const micro = HOSVER_MICRO(version);
  return `${major}.${minor}.${micro}`;
}

/**
 * Sets the cached HOS version for testing purposes.
 * This allows you to override the version detection for unit tests.
 * 
 * Note: This function is provided for compatibility with libnx's `hosversionSet()`,
 * but in most cases you shouldn't need to use it since nx.js gets the version
 * from the system automatically.
 * 
 * @param major - Major version component
 * @param minor - Minor version component
 * @param micro - Micro version component
 * 
 * @example
 * ```typescript
 * // Override version for testing
 * hosversionSet(13, 0, 0);
 * console.log(hosversionAtLeast(14, 0, 0)); // false
 * 
 * // Reset to actual system version
 * hosversionReset();
 * ```
 */
export function hosversionSet(major: number, minor: number, micro: number): void {
  cachedHosVersion = HOSVER_MAKE(major, minor, micro);
}

/**
 * Resets the cached HOS version, forcing the next call to `hosversionGet()`
 * to re-parse the version from `Switch.version.hos`.
 * 
 * This is mainly useful for testing or if you need to ensure you have the
 * latest version information.
 * 
 * @example
 * ```typescript
 * // Reset cached version
 * hosversionReset();
 * 
 * // This will re-parse Switch.version.hos
 * const version = hosversionGet();
 * ```
 */
export function hosversionReset(): void {
  cachedHosVersion = null;
}

/**
 * Type representing a HOS version as a 32-bit integer.
 * This is the format used by all hosversion functions.
 */
export type HosVersion = number;

/**
 * Interface representing the components of a HOS version.
 */
export interface HosVersionComponents {
  /** Major version component (0-255) */
  major: number;
  /** Minor version component (0-255) */
  minor: number;
  /** Micro version component (0-255) */
  micro: number;
}

/**
 * Parses a HOS version integer into its components.
 * 
 * @param version - The HOS version as a 32-bit integer
 * @returns An object with major, minor, and micro components
 * 
 * @example
 * ```typescript
 * const version = hosversionGet();
 * const { major, minor, micro } = hosversionParse(version);
 * console.log(`HOS ${major}.${minor}.${micro}`);
 * ```
 */
export function hosversionParse(version: HosVersion): HosVersionComponents {
  return {
    major: HOSVER_MAJOR(version),
    minor: HOSVER_MINOR(version),
    micro: HOSVER_MICRO(version),
  };
}
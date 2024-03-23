// Read save data for this app using the `localStorage` API
const launchCount = localStorage.launchCount;
const lastOpened = localStorage.lastOpened;

console.log(`App launch count: ${launchCount ?? 0}`);
console.log(`App last opened:  ${lastOpened ? new Date(Number(lastOpened)) : 'never'}`);
console.log();

// Write data data for this app using the `localStorage` API
localStorage.launchCount = parseInt(launchCount ?? '0', 10) + 1;
localStorage.lastOpened = Date.now();

console.log('Updated save data with new count and timestamp.')
console.log('Relaunch the app to see the new values.');

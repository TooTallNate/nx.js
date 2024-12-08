---
"@nx.js/runtime": patch
---

Make `localStorage` be `undefined` if the "save_data_owner_id" property in the app's NACP is not set

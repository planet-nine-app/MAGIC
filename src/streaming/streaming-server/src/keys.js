import fs from 'fs';

// File-backed key persistence - the desktop analog of the NVS storage the
// embedded gateways use (../../embedded/espnow-target/, .../ble-sessionless/)
// so this process keeps the same Sessionless identity (and fount
// registration) across restarts instead of re-registering as a new user
// every time.

const load = (keysFile) => {
  if (!fs.existsSync(keysFile)) return null;
  return JSON.parse(fs.readFileSync(keysFile, 'utf-8'));
};

const save = (keysFile, data) => {
  fs.writeFileSync(keysFile, JSON.stringify(data, null, 2));
};

export default { load, save };

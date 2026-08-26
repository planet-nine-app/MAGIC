import OBSWebSocket from 'obs-websocket-js';

// Drives OBS Studio via obs-websocket (protocol v5, built into OBS 28+) in
// response to a resolved MAGIC spell. Connecting is best-effort: if OBS
// isn't running or obs-websocket isn't enabled, this gateway still resolves
// spells against fount just fine - it just can't produce the visual/audio
// side effect, which is logged clearly rather than crashing the process.
class Effects {
  constructor(config) {
    this.config = config;
    this.obs = new OBSWebSocket();
    this.connected = false;
  }

  async connect() {
    try {
      await this.obs.connect(this.config.OBS_URL, this.config.OBS_PASSWORD || undefined);
      this.connected = true;
      console.log(`🎥 Connected to OBS at ${this.config.OBS_URL}`);
    } catch (err) {
      this.connected = false;
      console.warn(`⚠️  Could not connect to OBS (${err.message}) - effects will be skipped until it's reachable.`);
    }

    this.obs.on('ConnectionClosed', () => {
      this.connected = false;
      console.warn('⚠️  OBS connection closed.');
    });
  }

  /// Fires the OBS action configured for `effectName` in config.EFFECTS.
  /// Unknown effect names, or OBS being unreachable, are both non-fatal -
  /// the spell has already resolved successfully by the time this runs, so
  /// a missing visual effect shouldn't read as a failed cast to the caster.
  async trigger(effectName) {
    const effect = this.config.EFFECTS[effectName];
    if (!effect) {
      console.warn(`⚠️  No effect configured for "${effectName}" - add it to config.js EFFECTS.`);
      return { triggered: false, reason: 'unknown effect' };
    }

    if (!this.connected) {
      console.warn(`⚠️  Effect "${effectName}" requested but OBS isn't connected - skipping.`);
      return { triggered: false, reason: 'obs not connected' };
    }

    try {
      switch (effect.type) {
        case 'scene':
          await this.obs.call('SetCurrentProgramScene', { sceneName: effect.sceneName });
          break;

        case 'sourceFlash':
          await this.obs.call('SetSceneItemEnabled', {
            sceneName: effect.sceneName,
            sceneItemId: await this.sceneItemId(effect.sceneName, effect.sourceName),
            sceneItemEnabled: true
          });
          setTimeout(async () => {
            try {
              await this.obs.call('SetSceneItemEnabled', {
                sceneName: effect.sceneName,
                sceneItemId: await this.sceneItemId(effect.sceneName, effect.sourceName),
                sceneItemEnabled: false
              });
            } catch (err) {
              console.warn(`⚠️  Failed to revert "${effectName}": ${err.message}`);
            }
          }, this.config.HOLD_MS);
          break;

        case 'hotkey':
          await this.obs.call('TriggerHotkeyByName', { hotkeyName: effect.hotkeyName });
          break;

        default:
          console.warn(`⚠️  Unknown effect type "${effect.type}" for "${effectName}".`);
          return { triggered: false, reason: 'unknown effect type' };
      }

      console.log(`✨ Triggered OBS effect "${effectName}" (${effect.type})`);
      return { triggered: true };
    } catch (err) {
      console.warn(`⚠️  OBS call failed for effect "${effectName}": ${err.message}`);
      return { triggered: false, reason: err.message };
    }
  }

  async sceneItemId(sceneName, sourceName) {
    const { sceneItemId } = await this.obs.call('GetSceneItemId', { sceneName, sourceName });
    return sceneItemId;
  }
}

export default Effects;

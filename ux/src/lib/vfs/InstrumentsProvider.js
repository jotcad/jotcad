import { Selector } from '../../../../fs/src/vfs_browser.js';

const DEFAULT_CONFIG = {
  instruments: [
    { name: "Acoustic Grand Piano", wave: -2, pulse_width: 128, attack: 5, decay: 1500, sustain: 0, release: 250, volume_scale: 240 },
    { name: "Bright Acoustic Piano", wave: -4, pulse_width: 80, attack: 5, decay: 1200, sustain: 0, release: 200, volume_scale: 110 },
    { name: "Electric Grand Piano", wave: -2, pulse_width: 128, attack: 5, decay: 1800, sustain: 20, release: 300, volume_scale: 240 },
    { name: "Honky-tonk Piano", wave: -4, pulse_width: 60, attack: 5, decay: 1000, sustain: 0, release: 150, volume_scale: 110 },
    { name: "Electric Piano 1", wave: -1, pulse_width: 128, attack: 10, decay: 2000, sustain: 50, release: 400, volume_scale: 255 },
    { name: "Electric Piano 2", wave: -2, pulse_width: 128, attack: 15, decay: 2200, sustain: 80, release: 500, volume_scale: 240 },
    { name: "Harpsichord", wave: -4, pulse_width: 32, attack: 2, decay: 600, sustain: 0, release: 100, volume_scale: 110 },
    { name: "Clavinet", wave: -4, pulse_width: 48, attack: 5, decay: 500, sustain: 20, release: 150, volume_scale: 110 },
    { name: "Celesta", wave: -1, pulse_width: 128, attack: 5, decay: 800, sustain: 0, release: 200, volume_scale: 255 },
    { name: "Glockenspiel", wave: -1, pulse_width: 128, attack: 2, decay: 1200, sustain: 0, release: 300, volume_scale: 255 },
    { name: "Music Box", wave: -1, pulse_width: 128, attack: 10, decay: 1500, sustain: 0, release: 400, volume_scale: 255 },
    { name: "Vibraphone", wave: -1, pulse_width: 128, attack: 15, decay: 2500, sustain: 50, release: 600, volume_scale: 255 },
    { name: "Marimba", wave: -2, pulse_width: 128, attack: 5, decay: 400, sustain: 0, release: 100, volume_scale: 240 },
    { name: "Xylophone", wave: -2, pulse_width: 128, attack: 2, decay: 300, sustain: 0, release: 50, volume_scale: 240 },
    { name: "Tubular Bells", wave: -1, pulse_width: 128, attack: 10, decay: 3000, sustain: 10, release: 1000, volume_scale: 255 },
    { name: "Dulcimer", wave: -2, pulse_width: 128, attack: 5, decay: 800, sustain: 0, release: 200, volume_scale: 240 },
    { name: "Drawbar Organ", wave: -1, pulse_width: 128, attack: 10, decay: 10, sustain: 255, release: 50, volume_scale: 255 },
    { name: "Percussive Organ", wave: -1, pulse_width: 128, attack: 5, decay: 200, sustain: 180, release: 80, volume_scale: 255 },
    { name: "Rock Organ", wave: -3, pulse_width: 128, attack: 10, decay: 10, sustain: 255, release: 80, volume_scale: 120 },
    { name: "Church Organ", wave: -2, pulse_width: 128, attack: 50, decay: 20, sustain: 255, release: 150, volume_scale: 240 },
    { name: "Reed Organ", wave: -4, pulse_width: 128, attack: 40, decay: 20, sustain: 240, release: 100, volume_scale: 110 },
    { name: "Accordion", wave: -4, pulse_width: 96, attack: 30, decay: 30, sustain: 230, release: 120, volume_scale: 110 },
    { name: "Harmonica", wave: -4, pulse_width: 110, attack: 40, decay: 40, sustain: 220, release: 150, volume_scale: 110 },
    { name: "Tango Accordion", wave: -3, pulse_width: 128, attack: 30, decay: 30, sustain: 240, release: 100, volume_scale: 120 },
    { name: "Acoustic Guitar (nylon)", wave: -2, pulse_width: 128, attack: 10, decay: 1200, sustain: 0, release: 200, volume_scale: 240 },
    { name: "Acoustic Guitar (steel)", wave: -2, pulse_width: 128, attack: 5, decay: 1500, sustain: 0, release: 250, volume_scale: 240 },
    { name: "Electric Guitar (jazz)", wave: -1, pulse_width: 128, attack: 15, decay: 1800, sustain: 50, release: 300, volume_scale: 255 },
    { name: "Electric Guitar (clean)", wave: -4, pulse_width: 140, attack: 8, decay: 1600, sustain: 40, release: 250, volume_scale: 110 },
    { name: "Electric Guitar (muted)", wave: -4, pulse_width: 160, attack: 4, decay: 200, sustain: 0, release: 50, volume_scale: 110 },
    { name: "Overdriven Guitar", wave: -3, pulse_width: 128, attack: 10, decay: 50, sustain: 255, release: 150, volume_scale: 120 },
    { name: "Distortion Guitar", wave: -3, pulse_width: 128, attack: 8, decay: 80, sustain: 255, release: 200, volume_scale: 120 },
    { name: "Guitar harmonics", wave: -1, pulse_width: 128, attack: 5, decay: 1000, sustain: 0, release: 400, volume_scale: 255 },
    { name: "Acoustic Bass", wave: -2, pulse_width: 128, attack: 20, decay: 1000, sustain: 0, release: 200, volume_scale: 240 },
    { name: "Electric Bass (finger)", wave: -1, pulse_width: 128, attack: 15, decay: 1200, sustain: 20, release: 150, volume_scale: 255 },
    { name: "Electric Bass (pick)", wave: -4, pulse_width: 180, attack: 10, decay: 1000, sustain: 10, release: 120, volume_scale: 110 },
    { name: "Fretless Bass", wave: -2, pulse_width: 128, attack: 30, decay: 1500, sustain: 40, release: 250, volume_scale: 240 },
    { name: "Slap Bass 1", wave: -4, pulse_width: 40, attack: 5, decay: 400, sustain: 0, release: 100, volume_scale: 110 },
    { name: "Slap Bass 2", wave: -3, pulse_width: 128, attack: 8, decay: 500, sustain: 10, release: 120, volume_scale: 120 },
    { name: "Synth Bass 1", wave: -4, pulse_width: 64, attack: 10, decay: 300, sustain: 128, release: 150, volume_scale: 110 },
    { name: "Synth Bass 2", wave: -3, pulse_width: 128, attack: 15, decay: 450, sustain: 160, release: 180, volume_scale: 120 },
    { name: "Violin", wave: -3, pulse_width: 128, attack: 80, decay: 100, sustain: 220, release: 300, volume_scale: 120 },
    { name: "Viola", wave: -3, pulse_width: 128, attack: 100, decay: 120, sustain: 200, release: 350, volume_scale: 120 },
    { name: "Cello", wave: -3, pulse_width: 128, attack: 120, decay: 150, sustain: 210, release: 400, volume_scale: 120 },
    { name: "Contrabass", wave: -3, pulse_width: 128, attack: 150, decay: 200, sustain: 190, release: 500, volume_scale: 120 },
    { name: "Tremolo Strings", wave: -3, pulse_width: 128, attack: 20, decay: 50, sustain: 255, release: 100, volume_scale: 120 },
    { name: "Pizzicato Strings", wave: -2, pulse_width: 128, attack: 5, decay: 150, sustain: 0, release: 50, volume_scale: 240 },
    { name: "Orchestral Harp", wave: -2, pulse_width: 128, attack: 10, decay: 2000, sustain: 0, release: 300, volume_scale: 240 },
    { name: "Timpani", wave: -5, pulse_width: 128, attack: 15, decay: 600, sustain: 0, release: 150, volume_scale: 70 },
    { name: "String Ensemble 1", wave: -3, pulse_width: 128, attack: 200, decay: 500, sustain: 220, release: 600, volume_scale: 120 },
    { name: "String Ensemble 2", wave: -2, pulse_width: 128, attack: 250, decay: 600, sustain: 200, release: 800, volume_scale: 240 },
    { name: "SynthStrings 1", wave: -3, pulse_width: 128, attack: 150, decay: 400, sustain: 240, release: 700, volume_scale: 120 },
    { name: "SynthStrings 2", wave: -4, pulse_width: 128, attack: 180, decay: 450, sustain: 220, release: 900, volume_scale: 110 },
    { name: "Choir Aahs", wave: -1, pulse_width: 128, attack: 150, decay: 300, sustain: 230, release: 500, volume_scale: 255 },
    { name: "Voice Oohs", wave: -1, pulse_width: 128, attack: 200, decay: 350, sustain: 210, release: 600, volume_scale: 255 },
    { name: "Synth Voice", wave: -2, pulse_width: 128, attack: 120, decay: 400, sustain: 220, release: 500, volume_scale: 240 },
    { name: "Orchestra Hit", wave: -3, pulse_width: 128, attack: 5, decay: 150, sustain: 50, release: 150, volume_scale: 120 },
    { name: "Trumpet", wave: -3, pulse_width: 128, attack: 40, decay: 100, sustain: 240, release: 200, volume_scale: 120 },
    { name: "Trombone", wave: -3, pulse_width: 128, attack: 60, decay: 120, sustain: 230, release: 250, volume_scale: 120 },
    { name: "Tuba", wave: -3, pulse_width: 128, attack: 80, decay: 150, sustain: 220, release: 300, volume_scale: 120 },
    { name: "Muted Trumpet", wave: -4, pulse_width: 32, attack: 30, decay: 90, sustain: 200, release: 180, volume_scale: 110 },
    { name: "French Horn", wave: -2, pulse_width: 128, attack: 80, decay: 200, sustain: 210, release: 400, volume_scale: 240 },
    { name: "Brass Section", wave: -3, pulse_width: 128, attack: 50, decay: 150, sustain: 255, release: 250, volume_scale: 120 },
    { name: "SynthBrass 1", wave: -3, pulse_width: 128, attack: 40, decay: 200, sustain: 220, release: 350, volume_scale: 120 },
    { name: "SynthBrass 2", wave: -4, pulse_width: 96, attack: 60, decay: 250, sustain: 210, release: 400, volume_scale: 110 },
    { name: "Soprano Sax", wave: -3, pulse_width: 128, attack: 50, decay: 120, sustain: 230, release: 200, volume_scale: 120 },
    { name: "Alto Sax", wave: -3, pulse_width: 128, attack: 60, decay: 130, sustain: 220, release: 220, volume_scale: 120 },
    { name: "Tenor Sax", wave: -3, pulse_width: 128, attack: 70, decay: 140, sustain: 210, release: 240, volume_scale: 120 },
    { name: "Baritone Sax", wave: -3, pulse_width: 128, attack: 80, decay: 150, sustain: 200, release: 260, volume_scale: 120 },
    { name: "Oboe", wave: -4, pulse_width: 80, attack: 45, decay: 110, sustain: 220, release: 200, volume_scale: 110 },
    { name: "English Horn", wave: -4, pulse_width: 96, attack: 50, decay: 120, sustain: 210, release: 220, volume_scale: 110 },
    { name: "Bassoon", wave: -4, pulse_width: 64, attack: 60, decay: 150, sustain: 200, release: 250, volume_scale: 110 },
    { name: "Clarinet", wave: -4, pulse_width: 128, attack: 30, decay: 100, sustain: 230, release: 180, volume_scale: 110 },
    { name: "Piccolo", wave: -1, pulse_width: 128, attack: 20, decay: 80, sustain: 240, release: 150, volume_scale: 255 },
    { name: "Flute", wave: -1, pulse_width: 128, attack: 30, decay: 100, sustain: 230, release: 200, volume_scale: 255 },
    { name: "Recorder", wave: -1, pulse_width: 128, attack: 35, decay: 110, sustain: 220, release: 220, volume_scale: 255 },
    { name: "Pan Flute", wave: -1, pulse_width: 128, attack: 45, decay: 130, sustain: 210, release: 250, volume_scale: 255 },
    { name: "Blown Bottle", wave: -1, pulse_width: 128, attack: 60, decay: 150, sustain: 200, release: 300, volume_scale: 255 },
    { name: "Shakuhachi", wave: -1, pulse_width: 128, attack: 50, decay: 140, sustain: 215, release: 280, volume_scale: 255 },
    { name: "Whistle", wave: -1, pulse_width: 128, attack: 25, decay: 90, sustain: 220, release: 180, volume_scale: 255 },
    { name: "Ocarina", wave: -1, pulse_width: 128, attack: 40, decay: 120, sustain: 225, release: 220, volume_scale: 255 },
    { name: "Lead 1 (square)", wave: -4, pulse_width: 128, attack: 5, decay: 10, sustain: 255, release: 100, volume_scale: 110 },
    { name: "Lead 2 (sawtooth)", wave: -3, pulse_width: 128, attack: 5, decay: 10, sustain: 255, release: 100, volume_scale: 120 },
    { name: "Lead 3 (calliope)", wave: -1, pulse_width: 128, attack: 20, decay: 50, sustain: 240, release: 150, volume_scale: 255 },
    { name: "Lead 4 (chiff)", wave: -4, pulse_width: 160, attack: 15, decay: 60, sustain: 230, release: 180, volume_scale: 110 },
    { name: "Lead 5 (charang)", wave: -3, pulse_width: 128, attack: 8, decay: 40, sustain: 240, release: 150, volume_scale: 120 },
    { name: "Lead 6 (voice)", wave: -1, pulse_width: 128, attack: 80, decay: 100, sustain: 220, release: 300, volume_scale: 255 },
    { name: "Lead 7 (fifths)", wave: -3, pulse_width: 128, attack: 10, decay: 80, sustain: 230, release: 200, volume_scale: 120 },
    { name: "Lead 8 (bass + lead)", wave: -4, pulse_width: 96, attack: 8, decay: 150, sustain: 240, release: 150, volume_scale: 110 },
    { name: "Pad 1 (new age)", wave: -2, pulse_width: 128, attack: 300, decay: 500, sustain: 200, release: 1000, volume_scale: 240 },
    { name: "Pad 2 (warm)", wave: -1, pulse_width: 128, attack: 400, decay: 600, sustain: 220, release: 1200, volume_scale: 255 },
    { name: "Pad 3 (polysynth)", wave: -3, pulse_width: 128, attack: 150, decay: 400, sustain: 210, release: 800, volume_scale: 120 },
    { name: "Pad 4 (choir)", wave: -1, pulse_width: 128, attack: 350, decay: 500, sustain: 230, release: 1100, volume_scale: 255 },
    { name: "Pad 5 (bowed)", wave: -3, pulse_width: 128, attack: 250, decay: 450, sustain: 200, release: 900, volume_scale: 120 },
    { name: "Pad 6 (metallic)", wave: -4, pulse_width: 48, attack: 200, decay: 500, sustain: 210, release: 1000, volume_scale: 110 },
    { name: "Pad 7 (halo)", wave: -1, pulse_width: 128, attack: 450, decay: 700, sustain: 190, release: 1500, volume_scale: 255 },
    { name: "Pad 8 (sweep)", wave: -3, pulse_width: 128, attack: 500, decay: 800, sustain: 180, release: 1800, volume_scale: 120 },
    { name: "FX 1 (rain)", wave: -5, pulse_width: 128, attack: 10, decay: 3000, sustain: 50, release: 2000, volume_scale: 70 },
    { name: "FX 2 (soundtrack)", wave: -2, pulse_width: 128, attack: 500, decay: 1000, sustain: 180, release: 2500, volume_scale: 240 },
    { name: "FX 3 (crystal)", wave: 1, pulse_width: 128, attack: 5, decay: 200, sustain: 0, release: 1000, wavetable_id: 0, volume_scale: 140 },
    { name: "FX 4 (atmosphere)", wave: -1, pulse_width: 128, attack: 400, decay: 800, sustain: 200, release: 1500, volume_scale: 255 },
    { name: "FX 5 (brightness)", wave: -3, pulse_width: 128, attack: 300, decay: 600, sustain: 210, release: 1200, volume_scale: 120 },
    { name: "FX 6 (goblins)", wave: -4, pulse_width: 16, attack: 100, decay: 400, sustain: 150, release: 800, volume_scale: 110 },
    { name: "FX 7 (echoes)", wave: 1, pulse_width: 128, attack: 20, decay: 300, sustain: 100, release: 1000, wavetable_id: 1, volume_scale: 150 },
    { name: "FX 8 (sci-fi)", wave: -5, pulse_width: 128, attack: 200, decay: 1000, sustain: 100, release: 1200, volume_scale: 70 },
    { name: "Sitar", wave: -4, pulse_width: 32, attack: 10, decay: 1200, sustain: 0, release: 400, volume_scale: 110 },
    { name: "Banjo", wave: -4, pulse_width: 48, attack: 3, decay: 400, sustain: 0, release: 100, volume_scale: 110 },
    { name: "Shamisen", wave: -4, pulse_width: 64, attack: 4, decay: 600, sustain: 0, release: 150, volume_scale: 110 },
    { name: "Koto", wave: -2, pulse_width: 128, attack: 5, decay: 800, sustain: 0, release: 250, volume_scale: 240 },
    { name: "Kalimba", wave: -1, pulse_width: 128, attack: 2, decay: 500, sustain: 0, release: 120, volume_scale: 255 },
    { name: "Bagpipe", wave: -4, pulse_width: 128, attack: 100, decay: 50, sustain: 240, release: 300, volume_scale: 110 },
    { name: "Fiddle", wave: -3, pulse_width: 128, attack: 60, decay: 100, sustain: 220, release: 250, volume_scale: 120 },
    { name: "Shanai", wave: -4, pulse_width: 96, attack: 40, decay: 80, sustain: 230, release: 200, volume_scale: 110 },
    { name: "Tinkle Bell", wave: -1, pulse_width: 128, attack: 2, decay: 300, sustain: 0, release: 500, volume_scale: 255 },
    { name: "Agogo", wave: -2, pulse_width: 128, attack: 2, decay: 400, sustain: 0, release: 200, volume_scale: 240 },
    { name: "Steel Drums", wave: -2, pulse_width: 128, attack: 5, decay: 800, sustain: 0, release: 300, volume_scale: 240 },
    { name: "Woodblock", wave: -2, pulse_width: 128, attack: 2, decay: 150, sustain: 0, release: 50, volume_scale: 240 },
    { name: "Taiko Drum", wave: -1, pulse_width: 128, attack: 10, decay: 600, sustain: 0, release: 150, volume_scale: 255 },
    { name: "Melodic Tom", wave: -1, pulse_width: 128, attack: 8, decay: 800, sustain: 0, release: 200, volume_scale: 255 },
    { name: "Synth Drum", wave: 4, pulse_width: 128, attack: 2, decay: 500, sustain: 0, release: 150, volume_scale: 130 },
    { name: "Reverse Cymbal", wave: -5, pulse_width: 128, attack: 1000, decay: 50, sustain: 200, release: 50, volume_scale: 70 },
    { name: "Guitar Fret Noise", wave: -5, pulse_width: 128, attack: 5, decay: 150, sustain: 0, release: 50, volume_scale: 70 },
    { name: "Breath Noise", wave: -5, pulse_width: 128, attack: 100, decay: 400, sustain: 50, release: 200, volume_scale: 70 },
    { name: "Seashore", wave: -5, pulse_width: 128, attack: 2000, decay: 2000, sustain: 100, release: 2000, volume_scale: 70 },
    { name: "Bird Tweet", wave: -1, pulse_width: 128, attack: 20, decay: 150, sustain: 50, release: 100, volume_scale: 255 },
    { name: "Telephone Ring", wave: 4, pulse_width: 128, attack: 5, decay: 10, sustain: 255, release: 50, volume_scale: 100 },
    { name: "Helicopter", wave: -5, pulse_width: 128, attack: 5, decay: 50, sustain: 255, release: 100, volume_scale: 70 },
    { name: "Applause", wave: -5, pulse_width: 128, attack: 500, decay: 1000, sustain: 150, release: 1000, volume_scale: 70 },
    { name: "Gunshot", wave: -5, pulse_width: 128, attack: 2, decay: 800, sustain: 0, release: 100, volume_scale: 70 }
  ]
};

export const getInstrumentsConfig = () => {
  const saved = localStorage.getItem('jot_instruments_config');
  if (saved) {
    try {
      return JSON.parse(saved);
    } catch (e) {
      console.error('[InstrumentsProvider] Failed to parse saved config, using default', e);
    }
  }
  return DEFAULT_CONFIG;
};

export const setInstrumentsConfig = (config) => {
  localStorage.setItem('jot_instruments_config', JSON.stringify(config));
};

export const registerInstrumentsProvider = (vfs, mesh) => {
  const schema = {
    path: 'instrument/config',
    description: 'Retrieves current synthesizer instrument configurations.',
    arguments: [],
    outputs: {
      '$out': { type: 'jot:json', description: 'The instruments config JSON object.' }
    }
  };

  vfs.registerProvider('instrument/config', async (v, s) => {
    console.log('[InstrumentsProvider] Resolving instrument/config...');
    const config = getInstrumentsConfig();
    const bytes = new TextEncoder().encode(JSON.stringify(config));

    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      }
    });

    return {
      stream,
      metadata: {
        state: 'AVAILABLE',
        encoding: 'json',
        selector: s.toJSON()
      }
    };
  }, { schema });

  vfs.addSchema('instrument/config', schema);

  if (mesh && mesh.connected) {
    mesh.notify(new Selector('sys/schema'), {
      provider: vfs.id,
      catalog: { 'instrument/config': schema }
    });
  }
};

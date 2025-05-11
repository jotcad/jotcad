import { Volume } from 'memfs';

const volume = new Volume();

export const readFile = volume.promises.readFile;
export const writeFile = volume.promises.writeFile;

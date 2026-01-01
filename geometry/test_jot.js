import { access, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';

const isFilePresent = async (filePath) => {
  try {
    await access(filePath);
    return true;
  } catch (error) {
    return false;
  }
};

export const testJot = async (
  expectedJotFilePath, // Absolute path to the expected JOT file
  observedJotBuffer,   // The actual JOT data as a Buffer
  threshold = 0 // For text comparison, usually 0
) => {
  const observedJotFilePath = path.join(
    path.dirname(expectedJotFilePath),
    `observed.${path.basename(expectedJotFilePath)}`
  );

  // Always write the observed JOT to the location for manual review
  if (observedJotBuffer) {
    await writeFile(observedJotFilePath, observedJotBuffer);
  }
  
  let expectedFileExists = await isFilePresent(expectedJotFilePath);

  if (!expectedFileExists) {
    const message = `testJot: Reference file not found at ${expectedJotFilePath}. Expected file must exist.`;
    console.error(message);
    throw new Error(message);
  }

  const expectedJotBuffer = await readFile(expectedJotFilePath);

  // Compare contents directly
  const areEqual =
    expectedJotBuffer.toString('utf8') === observedJotBuffer.toString('utf8');

  if (areEqual) {
    return true;
  } else {
    const message = `testJot: ${expectedJotFilePath} differs from observed ${observedJotFilePath}`;
    console.log(message);
    throw new Error(message);
  }
};

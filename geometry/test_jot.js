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
    console.log(`testJot: Reference file not found at ${expectedJotFilePath}. Creating it.`);
    await writeFile(expectedJotFilePath, observedJotBuffer);
    return true; // The test "passes" by creating the reference
  }

  const expectedJotBuffer = await readFile(expectedJotFilePath);

  // Compare contents directly
  const areEqual = expectedJotBuffer.toString('utf8') === observedJotBuffer.toString('utf8');

  if (areEqual) {
    // If the test passes and an observedJotBuffer was provided, update the expected reference.
    // This is the original behavior which implicitly "approves" the observed image.
    if (observedJotBuffer) {
      await writeFile(expectedJotFilePath, observedJotBuffer);
    }
    return true;
  } else {
    console.log(
      `testJot: ${expectedJotFilePath} differs from ${observedJotFilePath}`
    );
    return false;
  }
};

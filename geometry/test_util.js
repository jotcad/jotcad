import { createHash } from 'node:crypto';
import path from 'node:path';

// Global counter for unique test directories
let testCounter = 0;

export const getTestDir = (testDescription) => {
  const testId = createHash('sha256')
    .update(testDescription)
    .digest('hex')
    .substring(0, 10);
  const basePath = path.join('/tmp/jotcad/tests', `${testId}-${testCounter++}`);
  return basePath;
};

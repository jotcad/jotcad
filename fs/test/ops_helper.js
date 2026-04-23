import { spawn } from 'node:child_process';

/**
 * Robustly spawns the C++ Native Ops node and waits for it to become healthy.
 * @param {string} opsPath Absolute path to the ops binary.
 * @param {string[]} args Arguments for the binary.
 * @param {number} healthPort The port to check for health.
 * @param {Object} options Additional options (env, etc).
 * @returns {Promise<Object>} The spawned process object.
 */
export async function spawnOpsNode(opsPath, args, healthPort, options = {}) {
  const url = `http://localhost:${healthPort}`;
  const opsProcess = spawn(opsPath, args, {
    env: {
      ...process.env,
      ...options.env,
    },
    stdio: options.stdio || 'inherit',
  });

  let stdout = '';
  let stderr = '';

  if (opsProcess.stdout) {
    opsProcess.stdout.on('data', (data) => {
      stdout += data.toString();
    });
  }
  if (opsProcess.stderr) {
    opsProcess.stderr.on('data', (data) => {
      stderr += data.toString();
    });
  }

  let processError;
  opsProcess.on('error', (err) => {
    processError = err;
  });

  let exitCode = null;
  opsProcess.on('exit', (code) => {
    exitCode = code;
  });

  const getFullErrorReport = (baseMsg) => {
    let msg = baseMsg;
    if (stdout) msg += `\n--- STDOUT ---\n${stdout}`;
    if (stderr) msg += `\n--- STDERR ---\n${stderr}`;
    return msg;
  };

  // Wait for node to be healthy
  let healthy = false;
  const maxRetries = 50;
  for (let i = 0; i < maxRetries; i++) {
    if (processError) {
      throw new Error(
        getFullErrorReport(`C++ Node failed to launch: ${processError.message}`)
      );
    }
    if (exitCode !== null && exitCode !== 0) {
      throw new Error(
        getFullErrorReport(`C++ Node exited prematurely with code ${exitCode}`)
      );
    }

    try {
      const resp = await fetch(`${url}/health`);
      if (resp.ok) {
        const info = await resp.json();
        if (info.status === 'OK') {
          healthy = true;
          break;
        }
      }
    } catch (e) {
      // Expected while starting
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }

  if (!healthy) {
    opsProcess.kill();
    throw new Error(
      getFullErrorReport(
        `C++ Node at ${url} failed to become healthy within ${
          maxRetries * 0.2
        } seconds`
      )
    );
  }

  return opsProcess;
}

/**
 * Standard launcher for integration tests.
 * @param {string} opsPath 
 * @param {number} port 
 * @param {string} storageDir 
 */
export async function launchOpsNode(opsPath, port, storageDir) {
  const child = await spawnOpsNode(opsPath, [port.toString(), storageDir], port, {
    stdio: 'inherit'
  });

  return {
    child,
    stop: async () => {
      child.kill();
      await new Promise(resolve => child.on('exit', resolve));
    }
  };
}

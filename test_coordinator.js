import { spawn } from 'node:child_process';

const suites = [
  {
    name: 'JOT Unit Tests',
    command: 'node',
    args: ['--test', '--test-concurrency=1', 'jot/test/*.js'],
    env: {}
  },
  {
    name: 'GEO C++ Unit Tests',
    command: './run_unit_tests.sh',
    args: [],
    cwd: 'geo/test',
    env: {}
  },
  {
    name: 'FS Unit Tests',
    command: 'node',
    args: ['--test', '--test-concurrency=1', 'fs/test/*.js'],
    env: { TEST_UX_PORT: '3039', TEST_OPS_PORT: '9099' }
  }
];

async function runSuite(suite) {
  console.log(`\n[RUNNING] ${suite.name}...`);
  return new Promise((resolve) => {
    const child = spawn(suite.command, suite.args, {
      cwd: suite.cwd || process.cwd(),
      env: { ...process.env, ...suite.env },
      shell: true
    });

    let output = '';
    child.stdout.on('data', (data) => { output += data; });
    child.stderr.on('data', (data) => { output += data; });

    child.on('close', (code) => {
      if (code !== 0) {
        console.error(`\n[FAILED] ${suite.name} (Exit Code: ${code})`);
        console.error('--- LAST 50 LINES OF OUTPUT ---');
        console.error(output.split('\n').slice(-50).join('\n'));
        console.error('-------------------------------');
        resolve(false);
      } else {
        console.log(`[PASSED] ${suite.name}`);
        resolve(true);
      }
    });
  });
}

async function main() {
  const results = [];
  for (const suite of suites) {
    const passed = await runSuite(suite);
    results.push({ name: suite.name, passed });
  }

  console.log('\n' + '='.repeat(40));
  console.log('FINAL TEST SUMMARY');
  console.log('='.repeat(40));
  
  let allPassed = true;
  for (const res of results) {
    console.log(`${res.passed ? '✅' : '❌'} ${res.name}`);
    if (!res.passed) allPassed = false;
  }
  
  process.exit(allPassed ? 0 : 1);
}

main();

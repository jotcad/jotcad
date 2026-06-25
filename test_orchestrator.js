import { spawn } from 'node:child_process';
import process from 'node:process';

const suites = {
  jot: {
    name: 'JOT Unit Tests',
    command: './geo/compile.sh && node --test --test-concurrency=1 jot/test/*.js',
    args: [],
    env: {},
    timeout: 120000
  },
  geo: {
    name: 'GEO C++ Unit Tests',
    command: './run_unit_tests.sh',
    args: [],
    cwd: 'geo/test',
    env: {},
    timeout: 300000
  },
  fs: {
    name: 'FS Unit Tests',
    command: 'node',
    args: ['--test', '--test-concurrency=1', 'fs/test/*.js'],
    env: { TEST_UX_PORT: '3039', TEST_OPS_PORT: '9099' },
    timeout: 120000
  },
  integration: {
    name: 'Integration Tests',
    command: './geo/compile.sh && node --test --test-concurrency=1 --test-timeout=60000 --test-force-exit integration/*.test.js',
    args: [],
    env: { LOG_LEVEL: 'debug' },
    timeout: 300000
  },
  puppeteer: {
    name: 'Puppeteer Integration Tests',
    command: './geo/compile.sh && npm run build:ux && node --test --test-concurrency=1 --test-timeout=300000 --test-force-exit integration/puppeteer/*.test.js',
    args: [],
    env: {},
    timeout: 360000
  }};

async function runSuite(id, suite) {
  console.log(`\n[RUNNING] ${suite.name} (${id})...`);
  const suiteTimeout = suite.timeout || 120000;
  return new Promise((resolve) => {
    const child = spawn(suite.command, suite.args, {
      cwd: suite.cwd || process.cwd(),
      env: { ...process.env, ...suite.env },
      shell: true,
      detached: true
    });

    let output = '';
    const failures = [];
    const stats = { tests: 0, pass: 0, fail: 0 };
    let timedOut = false;

    const timer = setTimeout(() => {
      timedOut = true;
      console.error(`\n[TIMEOUT] ${suite.name} timed out after ${suiteTimeout}ms! Killing process group...`);
      try {
        process.kill(-child.pid, 'SIGKILL');
      } catch (err) {
        child.kill('SIGKILL');
      }
    }, suiteTimeout);

    child.stdout.on('data', (data) => {
      const str = data.toString();
      output += str;
      process.stdout.write(str);
    });
    child.stderr.on('data', (data) => {
      const str = data.toString();
      output += str;
      process.stderr.write(str);
    });

    child.on('close', (code) => {
      clearTimeout(timer);
      const lines = output.split('\n');
      for (const line of lines) {
          const trimmed = line.trim();
          if (trimmed.startsWith('✖') && !trimmed.includes('failing tests:')) {
              failures.push(trimmed);
          }
          if (trimmed.startsWith('ℹ tests ')) stats.tests = parseInt(trimmed.replace('ℹ tests ', ''), 10);
          if (trimmed.startsWith('ℹ pass ')) stats.pass = parseInt(trimmed.replace('ℹ pass ', ''), 10);
          if (trimmed.startsWith('ℹ fail ')) stats.fail = parseInt(trimmed.replace('ℹ fail ', ''), 10);
          
          // Legacy/Custom format support
          if (trimmed.startsWith('Testing ')) {
              // Only count if not already using 'ℹ' format
              if (stats.tests === 0) {
                  stats.tests++;
                  stats.pass++;
              }
          }
      }

      if (timedOut) {
        if (stats.fail === 0) stats.fail = 1;
        if (stats.tests === 0) stats.tests = 1;
        failures.push(`Suite timed out after ${suiteTimeout}ms`);
        resolve({ passed: false, failures, stats });
      } else if (code !== 0) {
        console.error(`\n[FAILED] ${suite.name} (Exit Code: ${code})`);
        if (stats.fail === 0) stats.fail = 1; // Ensure failure is reflected in stats
        if (stats.tests === 0) stats.tests = 1;
        resolve({ passed: false, failures, stats });
      } else {
        console.log(`\n[PASSED] ${suite.name}`);
        resolve({ passed: true, failures: [], stats });
      }
    });
  });
}

async function main() {
  const args = process.argv.slice(2);
  let selectedIds = args.length > 0 ? args.filter(id => suites[id] || id === 'all') : Object.keys(suites);

  if (selectedIds.includes('all')) {
      selectedIds = Object.keys(suites);
  }

  if (args.length > 0 && selectedIds.length === 0) {
    console.error(`Unknown suites: ${args.join(', ')}`);
    console.error(`Available suites: ${Object.keys(suites).join(', ')}`);
    process.exit(1);
  }

  const results = [];
  for (const id of selectedIds) {
    const suite = suites[id];
    const res = await runSuite(id, suite);
    results.push({ name: suite.name, ...res });
  }

  console.log('\n' + '='.repeat(60));
  console.log('FINAL TEST SUMMARY');
  console.log('='.repeat(60));
  
  let totalTests = 0;
  let totalPass = 0;
  let totalFail = 0;
  let allPassed = true;

  for (const res of results) {
    const { tests, pass, fail } = res.stats;
    totalTests += tests;
    totalPass += pass;
    totalFail += fail;
    
    const statStr = `[Pass: ${pass}, Fail: ${fail}, Total: ${tests}]`;
    if (res.passed) {
        console.log(`✅ PASSED: ${res.name} ${statStr}`);
    } else {
        console.log(`❌ FAILED: ${res.name} ${statStr}`);
        allPassed = false;
        if (res.failures.length > 0) {
            res.failures.forEach(f => console.log(`   ${f}`));
        }
    }
  }
  
  console.log('-'.repeat(60));
  console.log(`TOTAL: ${totalPass} Passed, ${totalFail} Failed, ${totalTests} Total`);
  console.log('='.repeat(60));
  process.exit(allPassed ? 0 : 1);
}

main();

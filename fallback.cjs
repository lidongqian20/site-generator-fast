'use strict';
const { readFileSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { pathToFileURL } = require('node:url');

const INPUT_DIR = process.env.MAKEFUN_INPUT_DIR || '/input';
const OUTPUT_DIR = process.env.MAKEFUN_OUTPUT_DIR || '/output';

(async () => {
  const { run } = await import(pathToFileURL(join(__dirname, 'solve.js')).href);
  const cases = JSON.parse(readFileSync(join(INPUT_DIR, 'cases.json'), 'utf8'));
  const answers = [];
  for (const c of cases) answers.push(run(c.input));
  writeFileSync(join(OUTPUT_DIR, 'answers.json'), JSON.stringify(answers));
})();

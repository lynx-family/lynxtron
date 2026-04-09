const fs = require('node:fs');

const archivePath = process.argv[2];
const hashFile = process.argv[3];
const outputFile = process.argv[4];

const headerHash = fs.readFileSync(hashFile, 'utf8').trim().toLowerCase();
const normalizedArchivePath = archivePath.replace(/\//g, '\\').toLowerCase();

const payload = [
  {
    file: normalizedArchivePath,
    alg: 'sha256',
    value: headerHash,
  },
];

fs.writeFileSync(outputFile, JSON.stringify(payload, null, 2));

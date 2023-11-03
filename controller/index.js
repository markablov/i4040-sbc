/* eslint-disable no-console */

import * as path from 'node:path';
import * as fs from 'node:fs';
import { EventEmitter } from 'node:events';
import { fileURLToPath } from 'node:url';

import { SerialPort } from 'serialport';
import highwayhash from 'highwayhash';

import { compile } from 'i40xx-asm';
import { preprocessFile } from 'i40xx-preprocess';
import { buildRom } from 'i40xx-link';

const args = process.argv.slice(2);
const argsMap = Object.fromEntries(args.map((arg) => arg.replace(/^-/, '').split('=')));

const eventBus = new EventEmitter();

// just 0's
const hashKey = Buffer.alloc(32);

const BAUD_RATE = 115200;
const DEFAULT_PORT_NAME = 'COM3';

const OutCommandType = Object.freeze({ CmdStart: 0x00 });
const InCommandType = Object.freeze({ CmdAck: 0x00, CmdOutput: 0x01, CmdFinish: 0x02 });

/*
 * Send data to port and wait ack
 */
const writeCommandToPort = (port, cmd, data) => new Promise((resolve, reject) => {
  eventBus.once('ack', () => resolve());

  const packet = data?.length
    ? Buffer.concat([Buffer.from([cmd, data.length & 0xFF, data.length >> 8]), data])
    : Buffer.from([cmd, 0x00, 0x00]);

  port.write(packet, (err) => err && reject(err));
});

/*
 * Process received data
 */
const processIncomingData = (port) => {
  let currentBuf = Buffer.alloc(0);

  port.on('data', async (data) => {
    currentBuf = Buffer.concat([currentBuf, data]);

    let dataLen = currentBuf.readUInt16LE(1);
    while (dataLen + 3 <= currentBuf.length) {
      const cmd = currentBuf[0];
      switch (cmd) {
        case InCommandType.CmdFinish:
          process.exit(0);
          break;

        case InCommandType.CmdAck:
          eventBus.emit('ack');
          break;

        case InCommandType.CmdOutput:
          eventBus.emit('output', currentBuf.subarray(3, 3 + dataLen).toString());
          break;

        default:
          console.log('Unknown response!', cmd);
          process.exit(1);
      }

      currentBuf = currentBuf.subarray(3 + dataLen);
      if (currentBuf.length >= 3) {
        dataLen = currentBuf.readUInt16LE(1);
      }
    }
  });
};

/*
 * Open port
 */
const openPort = (portName) => new Promise((resolve, reject) => {
  const port = new SerialPort({ baudRate: BAUD_RATE, path: portName || DEFAULT_PORT_NAME }, (openErr) => {
    if (openErr) {
      reject(openErr);
      return;
    }

    port.on('error', (err) => console.log('Error: ', err.message || err));

    processIncomingData(port);

    resolve(port);
  });
});

/*
 * Function that either reads binary ROMs or compiles ASM source code into binary
 */
const getBlobWithROMs = (filePath) => {
  const fileData = fs.readFileSync(filePath);
  if (filePath.endsWith('.bin')) {
    console.log('Using binary ROM');
    return fileData;
  }

  const sourceCode = preprocessFile(filePath);

  const hash = highwayhash.asHexString(hashKey, Buffer.from(sourceCode, 'utf-8'));
  const hashKeyFileName = filePath.replace(/\.(.*)$/, '.hash');
  const binaryRomFileName = filePath.replace(/\.(.*)$/, '.bin');
  if (fs.existsSync(hashKeyFileName) && fs.existsSync(binaryRomFileName)) {
    const existingHash = fs.readFileSync(hashKeyFileName, 'utf-8');
    if (existingHash === hash) {
      console.log('Source code has not changed from last run, using existing ROM.');
      return fs.readFileSync(binaryRomFileName);
    }
  }

  console.log('Recompile source code...');
  const { blocks, symbols: blockAddressedSymbols, errors } = compile(sourceCode);
  if (errors.length) {
    console.log('COULD NOT PARSE SOURCE CODE!');
    console.log(errors);
    process.exit(1);
  }

  const { roms } = buildRom(blocks, blockAddressedSymbols);
  const binaryRom = Buffer.from(Array.prototype.concat(...roms.map(({ data }) => Array.from(data))));
  fs.writeFileSync(hashKeyFileName, hash);
  fs.writeFileSync(binaryRomFileName, binaryRom);
  return binaryRom;
};

const main = async () => {
  eventBus.on('output', (data) => {
    process.stdout.write(`[${new Date().toISOString()}] ${data}${data.endsWith('\n') ? '' : '\n'}`);
  });

  const port = await openPort(argsMap.port);

  const dirName = path.dirname(fileURLToPath(import.meta.url));
  await writeCommandToPort(port, OutCommandType.CmdStart, getBlobWithROMs(path.resolve(dirName, argsMap.rom)));
};

main()
  .catch((err) => {
    console.error('Error', err.message, err);
    process.exit();
  });

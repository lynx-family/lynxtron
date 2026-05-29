import { EventEmitter } from 'node:events';
import { randomBytes, createHash } from 'node:crypto';
import net from 'node:net';

function encodeClientFrame(payload) {
  const data = Buffer.from(payload);
  const headerLength = data.length < 126 ? 6 : data.length < 65536 ? 8 : 14;
  const frame = Buffer.alloc(headerLength + data.length);
  frame[0] = 0x81;
  if (data.length < 126) {
    frame[1] = 0x80 | data.length;
    randomBytes(4).copy(frame, 2);
    for (let i = 0; i < data.length; i += 1) {
      frame[6 + i] = data[i] ^ frame[2 + (i % 4)];
    }
  } else if (data.length < 65536) {
    frame[1] = 0x80 | 126;
    frame.writeUInt16BE(data.length, 2);
    randomBytes(4).copy(frame, 4);
    for (let i = 0; i < data.length; i += 1) {
      frame[8 + i] = data[i] ^ frame[4 + (i % 4)];
    }
  } else {
    frame[1] = 0x80 | 127;
    frame.writeBigUInt64BE(BigInt(data.length), 2);
    randomBytes(4).copy(frame, 10);
    for (let i = 0; i < data.length; i += 1) {
      frame[14 + i] = data[i] ^ frame[10 + (i % 4)];
    }
  }
  return frame;
}

function decodeFrames(buffer) {
  const messages = [];
  let offset = 0;
  while (buffer.length - offset >= 2) {
    const first = buffer[offset];
    const second = buffer[offset + 1];
    const opcode = first & 0x0f;
    const masked = (second & 0x80) !== 0;
    let length = second & 0x7f;
    let headerLength = 2;
    if (length === 126) {
      if (buffer.length - offset < 4) break;
      length = buffer.readUInt16BE(offset + 2);
      headerLength = 4;
    } else if (length === 127) {
      if (buffer.length - offset < 10) break;
      length = Number(buffer.readBigUInt64BE(offset + 2));
      headerLength = 10;
    }
    const maskOffset = offset + headerLength;
    const payloadOffset = maskOffset + (masked ? 4 : 0);
    if (buffer.length - payloadOffset < length) break;
    const payload = Buffer.from(buffer.subarray(payloadOffset, payloadOffset + length));
    if (masked) {
      const mask = buffer.subarray(maskOffset, maskOffset + 4);
      for (let i = 0; i < payload.length; i += 1) {
        payload[i] ^= mask[i % 4];
      }
    }
    if (opcode === 1) {
      messages.push(payload.toString('utf8'));
    }
    offset = payloadOffset + length;
  }
  return { messages, rest: buffer.subarray(offset) };
}

export async function connectCdp(wsEndpoint) {
  const url = new URL(wsEndpoint);
  const socket = net.connect(Number(url.port || 80), url.hostname);
  const key = randomBytes(16).toString('base64');
  const expectedAccept = createHash('sha1')
    .update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
    .digest('base64');
  const events = new EventEmitter();
  const pending = new Map();
  let nextId = 1;
  let buffer = Buffer.alloc(0);
  let connected = false;

  await new Promise((resolve, reject) => {
    socket.once('connect', () => {
      socket.write(
        [
          `GET ${url.pathname || '/'} HTTP/1.1`,
          `Host: ${url.host}`,
          'Upgrade: websocket',
          'Connection: Upgrade',
          `Sec-WebSocket-Key: ${key}`,
          'Sec-WebSocket-Version: 13',
          '',
          '',
        ].join('\r\n')
      );
    });
    socket.on('data', (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      if (!connected) {
        const headerEnd = buffer.indexOf('\r\n\r\n');
        if (headerEnd === -1) return;
        const header = buffer.subarray(0, headerEnd).toString('utf8');
        if (!header.includes(' 101 ') || !header.includes(expectedAccept)) {
          reject(new Error(`CDP WebSocket handshake failed: ${header}`));
          socket.destroy();
          return;
        }
        connected = true;
        buffer = buffer.subarray(headerEnd + 4);
        resolve();
      }
      if (connected && buffer.length > 0) {
        const decoded = decodeFrames(buffer);
        buffer = decoded.rest;
        for (const message of decoded.messages) {
          const payload = JSON.parse(message);
          if (payload.id && pending.has(payload.id)) {
            const { resolve: resolvePending, reject: rejectPending } = pending.get(payload.id);
            pending.delete(payload.id);
            if (payload.error) {
              rejectPending(Object.assign(new Error(payload.error.message), payload.error));
            } else {
              resolvePending(payload.result);
            }
          } else if (payload.method) {
            events.emit(payload.method, payload.params);
          }
        }
      }
    });
    socket.once('error', reject);
  });

  return {
    on(method, listener) {
      events.on(method, listener);
    },
    send(method, params = {}) {
      const id = nextId++;
      socket.write(encodeClientFrame(JSON.stringify({ id, method, params })));
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
      });
    },
    close() {
      socket.end();
    },
  };
}

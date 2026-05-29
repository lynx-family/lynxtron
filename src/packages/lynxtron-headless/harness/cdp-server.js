import { createHash } from 'node:crypto';
import http from 'node:http';

function encodeServerFrame(payload) {
  const data = Buffer.from(payload);
  if (data.length < 126) {
    return Buffer.concat([Buffer.from([0x81, data.length]), data]);
  }
  if (data.length < 65536) {
    const header = Buffer.alloc(4);
    header[0] = 0x81;
    header[1] = 126;
    header.writeUInt16BE(data.length, 2);
    return Buffer.concat([header, data]);
  }
  const header = Buffer.alloc(10);
  header[0] = 0x81;
  header[1] = 127;
  header.writeBigUInt64BE(BigInt(data.length), 2);
  return Buffer.concat([header, data]);
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

function send(socket, payload) {
  socket.write(encodeServerFrame(JSON.stringify(payload)));
}

export async function createCdpServer({ port = 0, methods, trace }) {
  const sockets = new Set();
  const server = http.createServer((_request, response) => {
    response.writeHead(404);
    response.end();
  });

  server.on('upgrade', (request, socket) => {
    const key = request.headers['sec-websocket-key'];
    if (!key) {
      socket.destroy();
      return;
    }
    const accept = createHash('sha1')
      .update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
      .digest('base64');
    socket.write(
      [
        'HTTP/1.1 101 Switching Protocols',
        'Upgrade: websocket',
        'Connection: Upgrade',
        `Sec-WebSocket-Accept: ${accept}`,
        '',
        '',
      ].join('\r\n')
    );
    sockets.add(socket);
    socket.on('close', () => sockets.delete(socket));
    let buffer = Buffer.alloc(0);
    socket.on('data', async (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      const decoded = decodeFrames(buffer);
      buffer = decoded.rest;
      for (const message of decoded.messages) {
        let requestPayload;
        try {
          requestPayload = JSON.parse(message);
          trace('cdp.request', {
            id: requestPayload.id,
            method: requestPayload.method,
          });
          const handler = methods[requestPayload.method];
          if (!handler) {
            throw Object.assign(new Error(`Unknown CDP method: ${requestPayload.method}`), {
              code: -32601,
            });
          }
          const result = await handler(requestPayload.params || {});
          send(socket, { id: requestPayload.id, result });
          trace('cdp.response', {
            id: requestPayload.id,
            method: requestPayload.method,
          });
        } catch (error) {
          send(socket, {
            id: requestPayload?.id ?? null,
            error: {
              code: error.code || -32000,
              message: error.message || String(error),
            },
          });
          trace('cdp.error', {
            id: requestPayload?.id,
            method: requestPayload?.method,
            message: error.message || String(error),
          });
        }
      }
    });
  });

  await new Promise((resolve) => server.listen(port, '127.0.0.1', resolve));
  const address = server.address();
  const wsEndpoint = `ws://127.0.0.1:${address.port}/devtools/page/lynx`;

  return {
    wsEndpoint,
    emit(method, params = {}) {
      for (const socket of sockets) {
        send(socket, { method, params });
      }
    },
    close() {
      for (const socket of sockets) {
        socket.destroy();
      }
      return new Promise((resolve) => server.close(resolve));
    },
  };
}

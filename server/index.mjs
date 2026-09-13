import cors from 'cors';
import express from 'express';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(__dirname, '..');
const enginePath = path.join(rootDir, 'engine', 'build', 'chess_engine');
const port = Number(process.env.PORT ?? 3001);

class EngineClient {
  constructor(binaryPath) {
    this.binaryPath = binaryPath;
    this.child = null;
    this.pending = [];
    this.current = null;
    this.stdoutBuffer = '';
  }

  ensureStarted() {
    if (this.child) {
      return;
    }
    if (!fs.existsSync(this.binaryPath)) {
      throw new Error(`Engine executable not found at ${this.binaryPath}. Run npm run build:engine first.`);
    }

    this.child = spawn(this.binaryPath, [], {
      cwd: rootDir,
      stdio: ['pipe', 'pipe', 'pipe']
    });

    this.child.stdout.setEncoding('utf8');
    this.child.stdout.on('data', (chunk) => this.handleStdout(chunk));
    this.child.stderr.setEncoding('utf8');
    this.child.stderr.on('data', (chunk) => process.stderr.write(`[engine] ${chunk}`));
    this.child.on('exit', (code) => {
      const error = new Error(`Engine exited with code ${code}`);
      if (this.current) {
        this.current.reject(error);
      }
      for (const item of this.pending) {
        item.reject(error);
      }
      this.child = null;
      this.current = null;
      this.pending = [];
      this.stdoutBuffer = '';
    });
  }

  send(command) {
    this.ensureStarted();
    return new Promise((resolve, reject) => {
      this.pending.push({ command, resolve, reject });
      this.pump();
    });
  }

  pump() {
    if (!this.child || this.current || this.pending.length === 0) {
      return;
    }
    this.current = this.pending.shift();
    this.child.stdin.write(`${this.current.command}\n`);
  }

  handleStdout(chunk) {
    this.stdoutBuffer += chunk;
    let newline = this.stdoutBuffer.indexOf('\n');
    while (newline !== -1) {
      const line = this.stdoutBuffer.slice(0, newline).trim();
      this.stdoutBuffer = this.stdoutBuffer.slice(newline + 1);
      if (line.startsWith('result ') && this.current) {
        try {
          this.current.resolve(JSON.parse(line.slice(7)));
        } catch (error) {
          this.current.reject(error);
        }
        this.current = null;
        this.pump();
      }
      newline = this.stdoutBuffer.indexOf('\n');
    }
  }

  close() {
    if (this.child) {
      this.child.stdin.write('quit\n');
      this.child = null;
    }
  }
}

const app = express();
const engine = new EngineClient(enginePath);

app.use(cors());
app.use(express.json());

app.get('/api/health', (_req, res) => {
  res.json({ ok: true, enginePath, engineBuilt: fs.existsSync(enginePath) });
});

app.post('/api/engine-move', async (req, res) => {
  const fen = String(req.body?.fen ?? '');
  const depth = Math.max(1, Math.min(7, Number(req.body?.depth ?? 3)));

  if (!fen) {
    res.status(400).json({ accepted: false, error: 'FEN is required' });
    return;
  }

  try {
    const result = await engine.send(`best depth ${depth} fen ${fen}`);
    res.status(result.accepted ? 200 : 400).json(result);
  } catch (error) {
    res.status(500).json({ accepted: false, error: error.message });
  }
});

const distDir = path.join(rootDir, 'dist');
if (fs.existsSync(distDir)) {
  app.use(express.static(distDir));
  app.use((req, res, next) => {
    if (req.method !== 'GET') {
      next();
      return;
    }
    res.sendFile(path.join(distDir, 'index.html'));
  });
}

const server = app.listen(port, '127.0.0.1', () => {
  console.log(`Server listening on http://127.0.0.1:${port}`);
});

process.on('SIGINT', () => {
  engine.close();
  server.close(() => process.exit(0));
});

process.on('SIGTERM', () => {
  engine.close();
  server.close(() => process.exit(0));
});

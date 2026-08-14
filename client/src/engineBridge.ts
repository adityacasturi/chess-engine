export type EngineResult = {
  accepted: boolean;
  fen: string;
  engineMove: string;
  status: string;
  score: number;
  nodes: number;
  timeMs: number;
  error?: string;
};

type Pending = {
  resolve: (result: EngineResult) => void;
  reject: (error: Error) => void;
};

type WorkerResponse =
  | { type: 'ready' }
  | { type: 'error'; error: string }
  | { id: number; ok: true; result: EngineResult }
  | { id: number; ok: false; error: string };

async function postJson<T>(url: string, body?: unknown): Promise<T> {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined
  });
  const data = await response.json();
  if (!response.ok) {
    throw new Error((data as { error?: string }).error ?? 'Request failed');
  }
  return data as T;
}

class EngineBridge {
  private worker: Worker | null = null;
  private nextId = 1;
  private pending = new Map<number, Pending>();
  private ready: Promise<boolean> | null = null;

  private ensureStarted(): Promise<boolean> {
    if (this.ready) {
      return this.ready;
    }

    this.ready = new Promise<boolean>((resolve) => {
      let settled = false;
      const finish = (ok: boolean) => {
        if (settled) return;
        settled = true;
        resolve(ok);
      };

      try {
        const worker = new Worker(`${import.meta.env.BASE_URL}engine/worker.js`);
        const timeout = window.setTimeout(() => {
          worker.terminate();
          finish(false);
        }, 15000);

        worker.onmessage = (event: MessageEvent<WorkerResponse>) => {
          const data = event.data;
          if ('type' in data && data.type === 'ready') {
            window.clearTimeout(timeout);
            this.worker = worker;
            finish(true);
            return;
          }
          if ('type' in data && data.type === 'error') {
            window.clearTimeout(timeout);
            worker.terminate();
            finish(false);
            return;
          }
          if (!('id' in data)) {
            return;
          }
          const pending = this.pending.get(data.id);
          if (!pending) {
            return;
          }
          this.pending.delete(data.id);
          if (data.ok) {
            pending.resolve(data.result);
          } else {
            pending.reject(new Error(data.error));
          }
        };

        worker.onerror = () => {
          window.clearTimeout(timeout);
          worker.terminate();
          finish(false);
        };
      } catch {
        finish(false);
      }
    });

    return this.ready;
  }

  async requestMove(fen: string, depth: number): Promise<EngineResult> {
    const useWorker = await this.ensureStarted();
    if (!useWorker || !this.worker) {
      return postJson<EngineResult>('/api/engine-move', { fen, depth });
    }

    return new Promise<EngineResult>((resolve, reject) => {
      const id = this.nextId++;
      this.pending.set(id, { resolve, reject });
      this.worker!.postMessage({ id, fen, depth });
    });
  }
}

export const engineBridge = new EngineBridge();

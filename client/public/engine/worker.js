/* global createChessEngine */

importScripts('./chess_engine.js');

let enginePromise = null;

function getEngine() {
  if (!enginePromise) {
    enginePromise = createChessEngine({
      locateFile(path) {
        return new URL(path, self.location.href).toString();
      }
    });
  }
  return enginePromise;
}

getEngine()
  .then(() => {
    self.postMessage({ type: 'ready' });
  })
  .catch((error) => {
    self.postMessage({ type: 'error', error: String(error && error.message ? error.message : error) });
  });

self.onmessage = async (event) => {
  const { id, fen, depth } = event.data;
  try {
    const Module = await getEngine();
    const json = Module.ccall('engine_best_move', 'string', ['string', 'number'], [fen, depth]);
    self.postMessage({ id, ok: true, result: JSON.parse(json) });
  } catch (error) {
    self.postMessage({
      id,
      ok: false,
      error: String(error && error.message ? error.message : error)
    });
  }
};

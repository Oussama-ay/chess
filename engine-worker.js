importScripts('./build/engine.js');

let engineModule = null;

// Initialize the WebAssembly module, passing locateFile so it can find engine.wasm
self.ChessEngineModule({
  locateFile: function(path) {
    return './build/' + path;
  }
}).then((mod) => {
  engineModule = mod;
  self.postMessage({ type: 'ready' });
}).catch((err) => {
  self.postMessage({ type: 'error', error: 'Failed to initialize engine module: ' + err.message });
});

self.onmessage = function(e) {
  const { fen } = e.data;
  if (!engineModule) {
    self.postMessage({ type: 'error', error: 'Engine not ready yet' });
    return;
  }

  try {
    const rawMove = engineModule.ccall('get_best_move', 'string', ['string'], [fen]);
    self.postMessage({ type: 'move', move: rawMove });
  } catch (err) {
    self.postMessage({ type: 'error', error: err.toString() });
  }
};

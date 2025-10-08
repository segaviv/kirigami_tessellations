// Module Service Worker to run Emscripten (kiri_patterns.wasm) calls off the main thread
// Uses message-based RPC: postMessage({ id, type: 'call', fn, argTypes, args, returnType })

import createModule from './kiri_patterns.js';

let modulePromise = null;

async function getKiriModule() {
    if (!modulePromise) {
        modulePromise = createModule();
    }
    return modulePromise;
}

self.addEventListener('install', (event) => {
    self.skipWaiting();
    getKiriModule();
});

self.addEventListener('activate', (event) => {
    event.waitUntil(self.clients.claim());
});

self.addEventListener('message', async (event) => {
    const port = event.ports && event.ports[0];
    const data = event.data || {};
    if (data) {
        const { id, type, args = [] } = data;
        try {
            const module = await getKiriModule();
            let result;
            switch (type) {
              case 'param':
                result = module.parameterizeMesh(args[0], args[1]);
                break;
              case 'init_opt':
                result = module.initOptimization(args[0], args[1]);
                break;
              case 'set_pattern':
                result = module.createBasePattern(args);
                break;
              case 'open_pattern':
                result = module.createFinalPattern(0, args[0], args[1], args[2]);
                break;
              case 'update_lift_params':
                result = module.updateLiftParams(args[0], args[1]);
                break;
              case 'optimize':
                result = module.runOptimization();
                break;
              case 'get_errors':
                result = module.get_errors();
                break;
              default:
                throw new Error(`Unknown type: ${type}`);
            }
            port && port.postMessage({ id, ok: true, result });
        } catch (err) {
            const errorMsg = (err && err.message) ? err.message : String(err);
            port && port.postMessage({ id, ok: false, error: errorMsg });
        }
    }
});



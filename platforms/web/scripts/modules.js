import * as game from "./game.js";
import * as ui from "./ui.js";
import * as client from "./client.js";

var modules = [ui, game, client];

function handleGlobalResize() {
  const [width, height] = [window.innerWidth, window.innerHeight];

  return { width, height };
}

function initModules() {
  for (const module of modules) {
    const { init, events } = module;
    if (init) {
      init.call(globalThis);
    }
    if (events) {
      //TODO: bind all module events
    }
  }
}

window.onload = () => {
  //NOTE: All modules initialization musto go here
  const { width, height } = handleGlobalResize();
  //create game canvas
  globalThis.canvas = document.body.appendChild(
    document.createElement("canvas"),
  );
  globalThis.ctx2d = globalThis.canvas.getContext("2d");

  Object.assign(globalThis.canvas, { width, height });

  //setup resize events
  window.addEventListener("resize", () => {
    const { width, height } = handleGlobalResize();
    Object.assign(globalThis.canvas, { width, height });
  });

  //setup all modules
  initModules();
};

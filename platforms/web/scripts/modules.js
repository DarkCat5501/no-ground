import { init as initGame } from "./game.js";

function handleGlobalResize() {
	const [width, height] = [window.innerWidth, window.innerHeight];

	return { width, height };
}


window.onload = () => {
	//NOTE: All modules initialization musto go here 

	const { width, height } = handleGlobalResize();
	//create game canvas
	const canvas = document.body.appendChild(document.createElement("canvas"));
	const ctx2d = canvas.getContext("2d");

	Object.assign(canvas, { width, height });

	//setup resize events
	window.addEventListener("resize", () => {
		const { width, height } = handleGlobalResize();
		Object.assign(canvas, { width, height });
	});

	//initialize game state
	initGame(canvas, ctx2d);
};

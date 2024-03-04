var gameState = { //makes the game state part of the global variables
	canvas: null,
	ctx: null,
	frameCallbackId: 0,
	isValid() { return !!(this.canvas && this.ctx); },
	entities: [],
	pushEntity(enitty) {
		this.entities.push(enitty);
	}
};

function gameLoop() {
	/** @type {{ctx:CanvasRenderingContext2D, canvas: HTMLCanvasElement}}**/
	const { ctx, canvas } = gameState;
	const { width, height } = canvas;

	//clear background
	ctx.clearRect(0, 0, width, height);


	//render all entities
	for (const entity of gameState.entities) {
		if (entity.visible && entity.position) {
			//TODO: better rendering of entity
			const { position, color } = entity;

			ctx.lineWidth = 3;
			ctx.strokeStyle = color;

			ctx.beginPath()
			{
				ctx.fillStyle = color ?? "#ff00ff";//debug color
				ctx.arc(position.x, position.y, 10, 0, 2 * Math.PI);
			}
			ctx.stroke();
		}
	}
}

function updateLoop() {
	//TODO: build quard tree and handle physics
	for (const entity of gameState.entities) {
		const { velocity, position } = entity;
		if (velocity && position) {
			position.x += velocity.x
			position.y += velocity.y
		}
	}
}

function startGameLoop() {
	const frameCallback = (frame) => {
		updateLoop(); //update loop
		gameLoop(); //render loop
		gameState.frameCallbackId = requestAnimationFrame(frameCallback);
	}

	requestAnimationFrame(frameCallback);
}

export function init(canvas, ctx) {
	gameState.canvas = canvas;
	gameState.ctx = ctx;

	gameState.pushEntity({ position: { x: 10, y: 10 }, velocity: { x: 0.1, y: 0.1 }, color: "red", visible: true });

	if (gameState.isValid()) {
		startGameLoop();
	}
}

// FOR TESTS ONLY
//checks if the running instance is on a browser or bun 
if (self.window !== undefined) {
	//game is running on browser
} else {
	//game isn't running on the browser
	self.requestAnimationFrame = (handler) => setTimeout(handler, 16);
}

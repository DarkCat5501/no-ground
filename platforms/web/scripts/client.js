//setup initial connection
var conn = { sk: null };
globalThis.conn = conn;

const id = Math.random();
var counter = 0;
export function init() {
  console.log("Init for client");
  // Create WebSocket connection.
  const socket = new WebSocket("ws://localhost:8090");
  // Connection opened
  socket.addEventListener("open", (event) => {
    socket.send("Hello world")
    // setInterval(() => {
    //   console.log("sent hello world")
    // }, 100);
  });

  // Listen for messages
  socket.addEventListener("message", (event) => {
    console.log("Message from server ", event.data);
  });
  conn.sk = socket;
}

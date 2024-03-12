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
    console.log("client connected");
    // const MBlong = new Uint8Array({ length: 50 * 1024 * 1024 }).fill(0x61);
    // console.log(MBlong.byteLength)
    // socket.send(MBlong);
    socket.send("Hello");
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

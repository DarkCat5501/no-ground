import { CStruct, CMemView, CTypes, CArray } from "../platforms/js/cwat";

/**@type {ArrayBuffer|null}*/let buffer = null;
/**@type {Uint8Array|null}*/let u8buffer = null;
/**@type {TextDecoder|null}*/let decoder = null;

export function beforeAll() {
  buffer = new ArrayBuffer(100);
  decoder = new TextDecoder("utf-8");
  u8buffer = new Uint8Array(buffer);

  return true;
}

export function tStructs() {
  u8buffer.fill(0);//clear buffer
  const dataView = new DataView(buffer);
  const vectorStruct = new CStruct({ x: CTypes.i8, y: CTypes.i16, z: CTypes.i32 });
  vectorStruct.set(dataView, { x: 0x41, y: 0x4342, z: 0x47464544 });
  const asString = decoder.decode(buffer).replace(/[\u{0080}-\u{FFFF}\0]/gu, "").trim();
  if (asString !== "ABCDEFG") throw new Error("Found invalid text equivalent")
  const data = vectorStruct.get(dataView);
  return data.x === 0x41 && data.y === 0x4342 && data.z === 0x47464544;
}

export function tMemView() {
  u8buffer.fill(0);//clear buffer
  const memView = new CMemView(buffer);
  const vectorStruct = new CStruct({ x: CTypes.i8, y: CTypes.i16, z: CTypes.i32 });
  //offset the buffer by a rando offset and tries to extract the features
  const offset = Math.ceil(Math.random() * (buffer.byteLength - vectorStruct.size));
  memView.set(offset, vectorStruct, { x: 0x41, y: 0x4342, z: 0x47464544 });
  const data = memView.get(offset, vectorStruct);
  return data.x === 0x41 && data.y === 0x4342 && data.z === 0x47464544;
}

export function tArrays() {
  u8buffer.fill(0);//clear buffer
  const memView = new CMemView(buffer);
  const vectorStruct = new CStruct({ x: CTypes.i8, y: CTypes.i16, z: CTypes.i32 });
  memView.set(0, vectorStruct, { x: 0x41, y: 0x4342, z: 0x47464544 });

  const str24 = CTypes.str(24, decoder);
  const data = memView.get(0, str24).trim();

  if (data !== "ABCDEFG") throw new Error("Found invalid text decoding");
  //TODO: implement setting

  return true;
}

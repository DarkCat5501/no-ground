export class CTypeBase {
	/** @param {number} byte_size - size in bytes of type **/
	constructor(byte_size) { this.bs = byte_size; }

	/**@type {number}*/ get size() { return this.bs; }

	/**@param {DataView} _view @param {number?} _offset*/
	get(_view, _offset = 0) { throw new Error("Usage of unimplemente getter at [CTypeBase]"); }
	/**@param {DataView} _view @param {any} _data @param {number?} _offset*/
	set(_view, _data, _offset = 0) { throw new Error("Usage of unimplemente getter at [CTypeBase]"); }
}

/** @brief C compliant types **/
export class CType extends CTypeBase {
	/**
	 * @param {number} byte_size - size in bytes of type
	 * @param {(view) => Function} getter - getter for variable in memory
	 * @param {(view) => Function} setter - setter for variable in memory
	 * @param {boolean} litt_endian - set if little endian encoded type
	 **/
	constructor(size_bytes, getter, setter, litt_endian = true, offset = 0) {
		super(size_bytes, litt_endian);
		this._gtt = getter;
		this._stt = setter;
		this._ofs = offset;
		this._le = litt_endian;
	}

	/**
	 * @param {DataView} view - memory view
	 * @param {number?} offset - offset of data int memory (also consider own type offset)
	 * @returns {any}
	 * **/
	get(view, offset = 0) {
		return this._gtt(view).call(view, this._ofs + offset, this._le);
	}

	/**
	 * @param {DataView} view - memory view
	 * @param {any} data - data to be set
	 * @param {number?} offset - offset of data int memory (also consider own type offset)
	 **/
	set(view, data, offset = 0) {
		return this._stt(view).call(view, this._ofs + offset, data, this._le);
	}
}

/** @brief gets the length in bytes of a structure definition*/
export function structd_size(struct_def) {
	return Object.entries(struct_def).reduce((s, [_, td]) => (s += td.size), 0);
}

/** @brief definition of a C structure **/
export class CStruct extends CTypeBase {
	/** @param {Record<string,CTypeBase>} fields - structure fields with types **/
	constructor(fields) {
		super(structd_size(fields));
		this._f = fields;
	}

	/**
	 * @override
	 * @param {DataView} view - memory view
	 * @param {number?} offset - offset of data int memory (also consider own type offset)
	 * @returns {any}
	 * **/
	get(view, offset = 0) {
		const data = {};
		Object.entries(this._f).reduce((bs, [f, td]) => {
			data[f] = td.get(view, bs + offset);
			return (bs += td.size);
		}, 0);
		return data;
	}

	/**
	 * @override
	 * @param {DataView} view - memory view
	 * @param {any} data - data to be set
	 * @param {number?} offset - offset of data int memory (also consider own type offset)
	 **/
	set(view, data, offset = 0) {
		Object.entries(this._f).reduce((bs, [f, td]) => {
			const value = data[f] ?? 0;
			td.set(view, value, bs + offset);
			return bs + td.size;
		}, 0);
	}
}

//defines the sizeof function globaly
globalThis.sizeof = function(typedef) {
	if (typedef instanceof CTypeBase) return typedef.size;
	return 0;
};

export const CTypes = Object.freeze({
	//integer types
	i8: new CType(1, (m) => m.getInt8, (m) => m.setInt8),
	u8: new CType(1, (m) => m.getUint8, (m) => m.setUint8),
	i16: new CType(2, (m) => m.getInt16, (m) => m.setUint16),
	u16: new CType(2, (m) => m.getUint16, (m) => m.setUint16),
	i32: new CType(4, (m) => m.getInt32, (m) => m.setInt32),
	u32: new CType(4, (m) => m.getUint32, (m) => m.setUint32),
	i64: new CType(8, (m) => m.getInt64, (m) => m.setInt64),
	u64: new CType(8, (m) => m.getUint64, (m) => m.setUint64),
	//floating point
	f32: new CType(4, (m) => m.getFloat32, (m) => m.setFloat32),
	f64: new CType(8, (m) => m.getFloat64, (m) => m.setFloat64),
	//ptr type
	ptr: new CType(4, (m) => m.getUint32, (m) => m.setUint32), //ptr data from wasm32

	//String type is special because it need to have length defined
	str(length, decoder) {
		return new CArray(this.u8, length,
			(b) => decoder.decode(b).replace(/[\u{0080}-\u{FFFF}\0]/gu, ""));
	},
});


export class CArray extends CTypeBase {
	/**@param {CTypeBase} type @param {number} length @param {()=>} item_map**/
	constructor(type, length, item_map) {
		super(type.size * length);
		this.t = type;
		this.l = length;
		this._m = item_map;
	}

	/**@returns {any[]}  @param {DataView} view @param {number} [offset=0]*/
	get(view, offset = 0) {
		let data = [];
		switch (this.t) {
			case CTypes.u8: data = new Uint8Array(view.buffer, offset + view.byteOffset, this.l); break;
			case CTypes.u16: data = new Uint16Array(view.buffer, offset + view.byteOffset, this.l); break;
			case CTypes.u32: data = new Uint32Array(view.buffer, offset + view.byteOffset, this.l); break;
			case CTypes.f32: data = new Float32Array(view.buffer, offset + view.byteOffset, this.l); break;
			case CTypes.f64: data = new Float64Array(view.buffer, offset + view.byteOffset, this.l); break;
			default:
				for (let item = 0; item < this.l; item++) {
					const ioff = offset + this.t.size * item;
					data.push(this.t.get(view, ioff));
				}
				break;
			case CTypes.u64: return new BigUint64Array(view.buffer, offset, this.l);
		}
		return this._m ? this._m(data) : data;
	}

	/**@param {DataView} view @param {any[]} data @param {number} [offset=0]*/
	set(view, data, offset = 0) {
		//TODO: implement array setting
		throw new Error("Not implemented yet");
	}
}

export class CMemView {
	constructor(buff_view) { this.bf = buff_view }
	/** @type {ArrayBuffer} **/
	get buffer() {
		if (typeof this.bf === "function") return this.bf();
		return this.bf;
	}

	/**
	 * @brief returns a view over the buffer 
	 * @param { number | BigInt } ptr
	 * @param { number | undefined} [size=undefined] 
	 **/
	view(ptr, size = undefined) {
		return new DataView(this.buffer, ptr, size);
	}

	/**
	 * @brief sets data directly into the buffer
	 * @param { number | BigInt } ptr
	 * @param { CType | CStruct } type
	 * @param { any } data
	 **/
	set(ptr, type, data) { type.set(this.view(ptr, type.size), data); }

	/**
	 * @brief gets data directly from the buffer
	 * @param { number | BigInt } ptr
	 * @param { CType | CStruct } type
	 **/
	get(ptr, type) { return type.get(this.view(ptr, type.size)); }
}

export class CModule {
	constructor(mod, ins) {
		this.m = mod instanceof WebAssembly.Module ? mod : mod.module;
		this.i = ins instanceof WebAssembly.Instance ? ins : ("instance" in mod ? mod.instance : null);
		this.mv = new CMemView(() => this.buff);
		this.e = new Map();
	}


	/**@returns {CModule}*/async ins(imports) { this.i = await WebAssembly.instantiate(this.m, imports); return this; }
	/**@type {WebAssembly.Exports}*/ get exs() { return this.i ? this.i.exports : {}; }
	/**@type {WebAssembly.Memory}*/ get mem() { return this.exs.memory; }
	/**@type {number}*/ get heap_base() { return this.exs.__heap_base.value; }
	/**@type {number}*/ get stkH() { return this.exs.__stack_high.value; }
	/**@type {number}*/ get stkL() { return this.exs.__stack_low.value; }
	/**@type {ArrayBuffer}*/ get buff() { return this.mem.buffer; }

	/**
	 * @brief pushes data after __heap_base
	 **/
	pushDt(type, data, offset = 0) { this.mv.set(this.heap_base + offset, type, data); }
	setDt(ptr, type, data) { this.mv.set(ptr, type, data) }
	/**@returns {any}*/ getDt(ptr, type) { return this.mv.get(ptr, type) }
}


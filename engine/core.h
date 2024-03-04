#ifndef __NG_CORE_H__
#define __NG_COER_H__

/**
 * Basic WASM types definitions
 **/

typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

typedef union { struct { i8 r,g,b,a; }; u32 col; } col32;
typedef struct { f32 x,y; } vec2;
typedef struct { f32 x,y,z; } vec3;
typedef struct { f32 x,y,z,w; } vec4;
typedef struct { i32 x,y; } ivec2;
typedef struct { i32 x,y,z; } ivec3;
typedef struct { i32 x,y,z,w; } ivec4;

#endif

#ifndef __NG_CORE_H__
#define __NG_CORE_H__
#include <stddef.h>
#include <assert.h>

typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef unsigned long u64;
typedef float f32;
typedef double f64;

typedef union { struct { i8 r,g,b,a; }; u32 col; } col32;
typedef struct { f32 x,y; } vec2;
typedef struct { f32 x,y,z; } vec3;
typedef struct { f32 x,y,z,w; } vec4;
typedef struct { i32 x,y; } ivec2;
typedef struct { i32 x,y,z; } ivec3;
typedef struct { i32 x,y,z,w; } ivec4;


#ifdef NG_METATYPES

typedef void* (*AllocFn)(void *, size_t);
typedef void (*DeallocFn)(void *);
#define Metadata(MT,x) ( (MT*)( (void*)(x) - sizeof(MT) ) )

//meta information about arrays
typedef struct {
	i32 cap;
	i32 len;
	AllocFn alloc;
	DeallocFn dealloc;
} ArrayMeta;

#define ArrayLen(X) Metadata(ArrayMeta,X)->len
#define ArrayCap(X) Metadata(ArrayMeta,X)->cap
#define ArrayEnd(x) ( (void*)(x) + sizeof(x[0]) * ArrayLen(x) )
#define ArrayLast(x) ( assert(ArrayLen(x) > 0 && "Trying to get item from empty array!"),(x)[ArrayLen(x)-1] )

inline void* ngArrayAlloc(i32 item_size,i32 cap,AllocFn alloc,DeallocFn dealloc);
inline void ngArrayResize(void** array,i32 item_size,i32 cap);
inline void ngArrayFree(void** array);
inline i32 ngArrayPush(void** array, i32 item_size,const void* item);

//removal operations
inline i32 ngArrayPop(void** array, i32 item_size, void* item);
inline i32 ngArrayRemove(void** array, i32 item_size, i32 index,void* item);
inline i32 ngArraySwapAndPop(void** array, i32 item_size, i32 index,void* item);

#ifndef DefaultAlloc
#include <stdlib.h>
#define DefaultAlloc realloc 
#endif

#ifndef DefaultDealloc
#include <stdlib.h>
#define DefaultDealloc free
#endif
#define NewArray(T,sz) (T*)ngArrayAlloc(sizeof(T),sz,DefaultAlloc,DefaultDealloc)
#define DelArray(x) ngArrayFree(X,sizeof(ArrayMeta),Metadata(ArrayMeta,X)->dealloc)
#define ArrayForeach(x) for(i32 it=0;it<ArrayLen(x) && it>=0;it++)
#define ArrayForall(x) for(i32 it=0;it<ArrayCap(x) && it>=0;it++)
#define ArrayFill(x,v) do{ArrayForall(x){ x[it] = (v); }ArrayLen(x)=ArrayCap(x);}while(0)
#define ArrayPush(x,v) ngArrayPush((void*)&x,sizeof(x[0]),&v)
#define ArrayFit(x) ngArrayResize((void*)&x,sizeof(x[0]),ArrayLen(x))
#define ArrayRemove(x,i,r) ngArrayRemove((void*)&x,sizeof(x[0]),i,r)
#define ArrayIRemove(x,i) ngArrayRemove((void*)&x,sizeof(x[0]),i,NULL)
#define ArrayPop(x,r) ngArrayPop((void*)&x,sizeof(x[0]),r)
#define ArrayIPop(x) ngArrayPop((void*)&x,sizeof(x[0]),NULL)
#define ArraySwapAndPop(x,i,r) ngArraySwapAndPop((void*)&x,sizeof(x[0]),i,r)
#define ArrayISwapAndPop(x,i) ngArraySwapAndPop((void*)&x,sizeof(x[0]),i,NULL)

#ifdef NG_CORE_IMPL
#include <string.h>
#include <stdio.h>

void* ngArrayAlloc(i32 item_size,i32 cap,AllocFn alloc,DeallocFn dealloc){
	ArrayMeta* ptr = alloc(NULL,sizeof(ArrayMeta)+item_size*cap);
	assert(ptr!=NULL && "Memory allocation fail!");
	ptr->cap = cap; ptr->len=0; ptr->alloc = alloc; ptr->dealloc = dealloc;
	return (void*)((void*)ptr + sizeof(ArrayMeta));
}

void ngArrayResize(void** array,i32 item_size,i32 cap){
	ArrayMeta* ptr = Metadata(ArrayMeta,*array)->alloc( Metadata(ArrayMeta,*array), sizeof(ArrayMeta) + item_size*cap );
	assert(ptr!=NULL && "Memory allocation fail!");
	ptr->cap = cap; ptr->len = ptr->len <= ptr->cap ? ptr->len : ptr->cap;
	*array = ((void*)ptr + sizeof(ArrayMeta));
	printf("resize\n");
}

void ngArrayFree(void** array){
	if(*array==NULL) return;
	ArrayMeta* meta = Metadata(ArrayMeta,*array);
	meta->dealloc(meta);
	*array = NULL;
}

i32 ngArrayPush(void **array, i32 item_size,const void *item){
	if(ArrayLen(*array)+1 > ArrayCap(*array))
		ngArrayResize(array,item_size,ArrayCap(*array)*2);

	assert(*array!=NULL && "Data rellocation failed!");

	memcpy((*array)+item_size*ArrayLen(*array), item, item_size);
	ArrayLen(*array)++;

	return ArrayLen(*array);
}

i32 ngArrayPop(void **array, i32 item_size, void *item){
	assert(ArrayLen(*array) > 0  && "Trying to remove element out of empty array!");
	if(item!=NULL) memcpy(item, *array+(ArrayLen(*array)-1)*item_size, item_size);
	ArrayLen(*array)--;
	
	if(ArrayLen(*array) <= ArrayCap(*array)/2)
		ngArrayResize(array,item_size,ArrayLen(*array)+1);

	return ArrayLen(*array);
}

i32 ngArrayRemove(void** array, i32 item_size, i32 index, void* item){
	assert(ArrayLen(*array) > index && index>=0  && "Trying to remove element out of bounds!");

	//copy element into item if sad so
	if(item!=NULL) memcpy(item,*array+(index*item_size), item_size);

	//reallocate all elements to the right
	i32 rhsb = ArrayLen(*array)-(index+1);//calculate the amount of element to the right
	ArrayLen(*array)--;//decrease the length
	
	memcpy(
		*array+(index)*item_size,
		*array+(index+1)*item_size,
		item_size * rhsb
	);
	// if for some reason the mothod above fail:
	// for(i32 idx=index;idx<ArrayLen(*array);idx++) {
	// 	memcpy(
	// 		*array+(idx+0)*item_size,
	// 		*array+(idx+1)*item_size,
	// 		item_size
	// 	);
	// }

	//shrink the array if needed
	if(ArrayLen(*array) <= ArrayCap(*array)/2)
		ngArrayResize(array,item_size,ArrayLen(*array)+1);

	return ArrayLen(*array);
}

i32 ngArraySwapAndPop(void **array, i32 item_size, i32 index, void *item){
	assert(ArrayLen(*array) > index && index>=0  && "Trying to remove element out of bounds!");
	if(index+1==ArrayLen(*array)) 
		return ngArrayPop(array, item_size,item);
	//copy element over into item
	if(item!=NULL) memcpy(item, *array+index*item_size,item_size);
	//swap with last element
	memcpy(*array+index*item_size, *array+(--ArrayLen(*array))*item_size,item_size);

	if(ArrayLen(*array) <= ArrayCap(*array)/2)
		ngArrayResize(array,item_size,ArrayLen(*array)+1);

	return ArrayLen(*array);
}

#endif //NG_CORE_IMPL
#endif //NG_METATYPES

#endif

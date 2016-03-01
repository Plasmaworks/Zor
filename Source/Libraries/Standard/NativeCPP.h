#pragma once

//=============================================================================
//  Rogue.h
//
//  ---------------------------------------------------------------------------
//
//  Created 2015.01.19 by Abe Pralle
//
//  This is free and unencumbered software released into the public domain
//  under the terms of the UNLICENSE ( http://unlicense.org ).
//=============================================================================

#if defined(_WIN32)
#  define ROGUE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#  define ROGUE_PLATFORM_MAC 1
#elif defined(__ANDROID__)
#  define ROGUE_PLATFORM_ANDROID 1
#else
#  define ROGUE_PLATFORM_GENERIC 1
#endif

#if defined(ROGUE_PLATFORM_WINDOWS)
#  include <windows.h>
#else
#  include <cstdint>
#endif

#ifndef ROGUE_GC_THRESHOLD_KB
  #define ROGUE_GC_THRESHOLD_KB 512
#endif
#define ROGUE_GC_THRESHOLD_BYTES (ROGUE_GC_THRESHOLD_KB << 10)

#include <stdlib.h>
#include <string.h>

#if defined(ROGUE_PLATFORM_WINDOWS)
  typedef double           RogueReal;
  typedef float            RogueFloat;
  typedef __int64          RogueLong;
  typedef __int32          RogueInt32;
  typedef double           RogueReal64;
  typedef float            RogueReal32;
  typedef __int64          RogueInt64;
  typedef __int32          RogueInt32;
  typedef unsigned __int16 RogueCharacter;
  typedef unsigned char    RogueByte;
  typedef bool             RogueLogical;
#else
  typedef double           RogueReal;
  typedef float            RogueFloat;
  typedef int64_t          RogueLong;
  typedef int32_t          RogueInt32;
  typedef double           RogueReal64;
  typedef float            RogueReal32;
  typedef int64_t          RogueInt64;
  typedef int32_t          RogueInt32;
  typedef uint16_t         RogueCharacter;
  typedef uint8_t          RogueByte;
  typedef bool             RogueLogical;
#endif

struct RogueAllocator;
struct RogueArray;
struct RogueCharacterList;
struct RogueString;

#define ROGUE_CREATE_OBJECT(name) RogueType_create_object(RogueType##name,0)
  //e.g. RogueType_create_object(RogueStringBuilder,0)

#define ROGUE_SINGLETON(name) RogueType_singleton(RogueType##name)
  //e.g. RogueType_singleton( RogueTypeConsole )

#define ROGUE_PROPERTY(name) p_##name


//-----------------------------------------------------------------------------
//  Error Handling
//-----------------------------------------------------------------------------
#include <setjmp.h>
#if defined(ROGUE_PLATFORM_MAC)
  // _setjmp/_longjmp on Mac are equivalent to setjmp/longjmp on other Unix
  // systems.  The non-underscore versions are much slower as they save and
  // restore the signal state as well as registers.
  #define ROGUE_SETJMP  _setjmp
  #define ROGUE_LONGJMP _longjmp
#else
  #define ROGUE_SETJMP  setjmp
  #define ROGUE_LONGJMP longjmp
#endif

#define ROGUE_TRY \
  { \
    RogueErrorHandler local_error_handler; \
    local_error_handler.previous_jump_buffer = Rogue_error_handler; \
    Rogue_error_handler = &local_error_handler; \
    if ( !ROGUE_SETJMP(local_error_handler.info) ) \
    {

#define ROGUE_CATCH(local_error_object,local_error_type) \
      Rogue_error_handler = local_error_handler.previous_jump_buffer; \
    } \
    else \
    { \
      local_error_type local_error_object = (local_error_type) Rogue_error_object; \
      Rogue_error_handler = local_error_handler.previous_jump_buffer;

#define ROGUE_END_TRY \
    } \
  }

#define ROGUE_THROW(_error_object) \
  Rogue_error_object = _error_object; \
  ROGUE_LONGJMP( Rogue_error_handler->info, 1 )

typedef struct RogueErrorHandler
{
  jmp_buf                   info;
  struct RogueErrorHandler* previous_jump_buffer;
} RogueErrorHandler;


//-----------------------------------------------------------------------------
//  Forward References
//-----------------------------------------------------------------------------
struct RogueObject;


//-----------------------------------------------------------------------------
//  Callback Definitions
//-----------------------------------------------------------------------------
typedef void         (*RogueTraceFn)( void* obj );
typedef RogueObject* (*RogueInitFn)( void* obj );
typedef void         (*RogueCleanUpFn)( void* obj );


//-----------------------------------------------------------------------------
//  RogueType
//-----------------------------------------------------------------------------

struct RogueType
{
  int          name_index;

  int          base_type_count;
  RogueType**  base_types;

  int          index;  // used for aspect call dispatch
  int          object_size;

  RogueObject* _singleton;
  void**       methods;

  RogueAllocator*   allocator;

  RogueTraceFn      trace_fn;
  RogueInitFn       init_object_fn;
  RogueInitFn       init_fn;
  RogueCleanUpFn    clean_up_fn;
};

RogueArray*  RogueType_create_array( int count, int element_size, bool is_reference_array=false );
RogueObject* RogueType_create_object( RogueType* THIS, RogueInt32 size );
void         RogueType_print_name( RogueType* THIS );
RogueType*   RogueType_retire( RogueType* THIS );
RogueObject* RogueType_singleton( RogueType* THIS );

//-----------------------------------------------------------------------------
//  RogueObject
//-----------------------------------------------------------------------------
struct RogueObjectType : RogueType
{
};

struct RogueObject
{
  RogueObject* next_object;
  // Used to keep track of this allocation so that it can be freed when no
  // longer referenced.

  RogueType*   type;
  // Type info for this object.

  RogueInt32 object_size;
  // Set to be ~object_size when traced through during a garbage collection,
  // then flipped back again at the end of GC.

  RogueInt32 reference_count;
  // A positive reference_count ensures that this object will never be
  // collected.  A zero reference_count means this object is kept only as 
  // long as it is visible to the memory manager.
};

RogueObject* RogueObject_as( RogueObject* THIS, RogueType* specialized_type );
RogueLogical RogueObject_instance_of( RogueObject* THIS, RogueType* ancestor_type );
void*        RogueObject_retain( RogueObject* THIS );
void*        RogueObject_release( RogueObject* THIS );

void RogueObject_trace( void* obj );
void RogueString_trace( void* obj );
void RogueArray_trace( void* obj );

//-----------------------------------------------------------------------------
//  RogueString
//-----------------------------------------------------------------------------
struct RogueString : RogueObject
{
  RogueInt32   count;
  RogueInt32   hash_code;
  RogueCharacter characters[];
};

RogueString* RogueString_create_with_count( int count );
RogueString* RogueString_create_from_c_string( const char* c_string, int count );
RogueString* RogueString_create_from_characters( RogueCharacterList* characters );
void         RogueString_decode_utf8( const char* utf8_data, RogueInt32 utf8_count, RogueCharacter* dest_buffer );
RogueInt32 RogueString_decoded_utf8_count( const char* utf8_data, RogueInt32 utf8_count );
void         RogueString_print_string( RogueString* st );
void         RogueString_print_characters( RogueCharacter* characters, int count );

bool         RogueString_to_c_string( RogueString* THIS, char* buffer, int buffer_size );
RogueString* RogueString_update_hash_code( RogueString* THIS );


//-----------------------------------------------------------------------------
//  RogueArray
//-----------------------------------------------------------------------------
struct RogueArray : RogueObject
{
  int  count;
  int  element_size;
  bool is_reference_array;

  union
  {
    RogueObject*   objects[];
    RogueByte      logicals[];
    RogueByte      bytes[];
    RogueCharacter characters[];
    RogueInt32   integers[];
    RogueInt64      longs[];
    RogueReal32     floats[];
    RogueReal64      reals[];
  };
};

RogueArray* RogueArray_set( RogueArray* THIS, RogueInt32 i1, RogueArray* other, RogueInt32 other_i1, RogueInt32 copy_count );


//-----------------------------------------------------------------------------
//  RogueAllocator
//-----------------------------------------------------------------------------
#ifndef ROGUEMM_PAGE_SIZE
// 4k; should be a multiple of 256 if redefined
#  define ROGUEMM_PAGE_SIZE (4*1024)
#endif

// 0 = large allocations, 1..4 = block sizes 64, 128, 192, 256
#ifndef ROGUEMM_SLOT_COUNT
#  define ROGUEMM_SLOT_COUNT 5
#endif

// 2^6 = 64
#ifndef ROGUEMM_GRANULARITY_BITS
#  define ROGUEMM_GRANULARITY_BITS 6
#endif

// Block sizes increase by 64 bytes per slot
#ifndef ROGUEMM_GRANULARITY_SIZE
#  define ROGUEMM_GRANULARITY_SIZE (1 << ROGUEMM_GRANULARITY_BITS)
#endif

// 63
#ifndef ROGUEMM_GRANULARITY_MASK
#  define ROGUEMM_GRANULARITY_MASK (ROGUEMM_GRANULARITY_SIZE - 1)
#endif

// Small allocation limit is 256 bytes - afterwards objects are allocated
// from the system.
#ifndef ROGUEMM_SMALL_ALLOCATION_SIZE_LIMIT
#  define ROGUEMM_SMALL_ALLOCATION_SIZE_LIMIT  ((ROGUEMM_SLOT_COUNT-1) << ROGUEMM_GRANULARITY_BITS)
#endif

//-----------------------------------------------------------------------------
//  RogueException
//-----------------------------------------------------------------------------
struct RogueException : RogueObject
{
};


//-----------------------------------------------------------------------------
//  RogueAllocationPage
//-----------------------------------------------------------------------------
struct RogueAllocationPage
{
  // Backs small 0..256-byte allocations.
  RogueAllocationPage* next_page;

  RogueByte* cursor;
  int        remaining;

  RogueByte  data[ ROGUEMM_PAGE_SIZE ];
};

RogueAllocationPage* RogueAllocationPage_create( RogueAllocationPage* next_page );
RogueAllocationPage* RogueAllocationPage_delete( RogueAllocationPage* THIS );
void*                RogueAllocationPage_allocate( RogueAllocationPage* THIS, int size );

//-----------------------------------------------------------------------------
//  RogueAllocator
//-----------------------------------------------------------------------------
struct RogueAllocator
{
  RogueAllocationPage* pages;
  RogueObject*         objects;
  RogueObject*         objects_requiring_cleanup;
  RogueObject*         available_objects[ROGUEMM_SLOT_COUNT];
};

RogueAllocator* RogueAllocator_create();
RogueAllocator* RogueAllocator_delete( RogueAllocator* THIS );

void*        RogueAllocator_allocate( int size );
RogueObject* RogueAllocator_allocate_object( RogueAllocator* THIS, RogueType* of_type, int size );
void*        RogueAllocator_free( RogueAllocator* THIS, void* data, int size );
void         RogueAllocator_free_objects( RogueAllocator* THIS );
void         RogueAllocator_collect_garbage( RogueAllocator* THIS );

extern int                Rogue_allocator_count;
extern RogueAllocator     Rogue_allocators[];
extern int                Rogue_type_count;
extern RogueType          Rogue_types[];
extern int                Rogue_type_info_table[];
extern int                Rogue_object_size_table[];
extern void*              Rogue_dynamic_method_table[];
extern int                Rogue_type_name_index_table[];
extern RogueInitFn        Rogue_init_object_fn_table[];
extern RogueInitFn        Rogue_init_fn_table[];
extern RogueTraceFn       Rogue_trace_fn_table[];
extern RogueCleanUpFn     Rogue_clean_up_fn_table[];
extern int                Rogue_literal_string_count;
extern RogueString*       Rogue_literal_strings[];
extern RogueErrorHandler* Rogue_error_handler;
extern RogueObject*       Rogue_error_object;
extern RogueLogical       Rogue_configured;
extern int                Rogue_argc;
extern const char**       Rogue_argv;
extern int                Rogue_bytes_allocated_since_gc;

void Rogue_configure( int argc=0, const char* argv[]=0 );
bool Rogue_collect_garbage( bool forced=false );
void Rogue_launch();
void Rogue_quit();


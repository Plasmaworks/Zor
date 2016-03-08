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
typedef void         (*RogueCallback)();
typedef void         (*RogueTraceFn)( void* obj );
typedef RogueObject* (*RogueInitFn)( void* obj );
typedef void         (*RogueCleanUpFn)( void* obj );

//-----------------------------------------------------------------------------
//  RogueCallbackInfo
//-----------------------------------------------------------------------------
struct RogueCallbackInfo
{
  RogueCallback      callback;
  RogueCallbackInfo* next_callback_info;

  RogueCallbackInfo() : callback(0), next_callback_info(0) {}
  RogueCallbackInfo( RogueCallback callback ) : callback(callback), next_callback_info(0) {}

  ~RogueCallbackInfo() { if (next_callback_info) delete next_callback_info; }

  void add( RogueCallback callback )
  {
    if (this->callback)
    {
      if (next_callback_info) next_callback_info->add( callback );
      else                    next_callback_info = new RogueCallbackInfo( callback );
    }
    else
    {
      this->callback = callback;
    }
  }

  void call()
  {
    if (callback) callback();
    if (next_callback_info) next_callback_info->call();
  }
};

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
RogueString* RogueString_create_from_c_string( const char* c_string, int count=-1 );
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
extern RogueCallbackInfo  Rogue_on_begin_gc;
extern RogueCallbackInfo  Rogue_on_end_gc;

void Rogue_configure( int argc=0, const char* argv[]=0 );
bool Rogue_collect_garbage( bool forced=false );
void Rogue_launch();
void Rogue_quit();
bool Rogue_update_tasks();  // returns true if tasks are still active

//-----------------------------------------------------------------------------
//  Stack Trace
//-----------------------------------------------------------------------------
struct RogueCallStack
{
  const char** locations;
  int          count;
  int          capacity;

  RogueCallStack() : count(0)
  {
    capacity = 256;
    locations = new const char*[ capacity ];
  }

  ~RogueCallStack()
  {
    delete locations;
  }

  void push( const char* location )
  {
    //printf( "+%s\n", location );
    if (count == capacity)
    {
      capacity *= 2;
      const char** new_locations = new const char*[ capacity ];
      memcpy( new_locations, locations, count * sizeof(const char*) );
      delete locations;
      locations = new_locations;
    }
    locations[ count++ ] = location;
  }

  void pop()
  {
    --count;
    //printf( "-%s\n", locations[count] );
  }
};

extern RogueCallStack Rogue_call_stack;


struct RogueCallTrace
{
   RogueCallTrace( const char* location )
   {
     Rogue_call_stack.push( location );
   }

   ~RogueCallTrace()
   {
     Rogue_call_stack.pop();
   }
};


#include <cmath>

struct RogueStringBuilder;
struct RogueCharacterList;
struct RogueClassGenericList;
struct RogueClassStringReader;
struct RogueClassCharacterReader;
struct RogueStringList;
struct RogueClassGlobal;
struct RogueClassPrintWriterAspect;
struct RogueClassConsole;
struct RogueClassCharacterWriter;
struct RogueClassRogueC;
struct RogueClassException;
struct RogueClassStackTrace;
struct RogueClassReader_of_String;
struct RogueClassMath;
struct RogueClassProgram;
struct RogueTemplateList;
struct RogueClassTemplate;
struct RogueClassString_TemplateTable;
struct RogueClassString_AugmentListTable;
struct RogueRequisiteItemList;
struct RogueClassRequisiteItem;
struct RogueClassMethod;
struct RoguePropertyList;
struct RogueClassProperty;
struct RogueClassString_MethodListTable;
struct RogueTypeList;
struct RogueClassType;
struct RogueClassString_TypeTable;
struct RogueClassString_Int32Table;
struct RogueClassString_StringListTable;
struct RogueClassString_Int32TableEntry;
struct RogueClassInt32_TemplateTable;
struct RogueClassToken;
struct RogueClassError;
struct RogueClassRogueError;
struct RogueClassAttributes;
struct RogueClassCmd;
struct RogueClassCmdReturn;
struct RogueClassCmdStatement;
struct RogueClassCmdStatementList;
struct RogueCmdList;
struct RogueClassTokenType;
struct RogueClassCmdLabel;
struct RogueClassScope;
struct RogueMethodList;
struct RogueClassFunction_Type_RETURNSLogical;
struct RogueClassFunction_197;
struct RogueClassFunction_198;
struct RogueClassCPPWriter;
struct RogueClassString_MethodTable;
struct RogueLocalList;
struct RogueClassLocal;
struct RogueInt32List;
struct RogueByteList;
struct RogueClassSystem;
struct RogueClassString_LogicalTable;
struct RogueParserList;
struct RogueClassParser;
struct RogueClassString_ParseReaderTable;
struct RogueClassFile;
struct RogueTokenList;
struct RogueClassLineReader;
struct RogueTypeParameterList;
struct RogueClassTypeParameter;
struct RogueAugmentList;
struct RogueClassAugment;
struct RogueClassString_TokenTypeTable;
struct RogueClassLiteralCharacterToken;
struct RogueClassLiteralInt64Token;
struct RogueClassLiteralInt32Token;
struct RogueClassLiteralReal64Token;
struct RogueClassStringDataToken;
struct RogueClassString_TypeSpecializerTable;
struct RogueClassTypeSpecializer;
struct RogueTypeSpecializerList;
struct RogueTableEntry_of_String_TemplateList;
struct RogueClassString_TemplateTableEntry;
struct RogueTableEntry_of_String_AugmentListList;
struct RogueClassString_AugmentListTableEntry;
struct RogueCmdLabelList;
struct RogueClassString_CmdLabelTable;
struct RogueClassCloneArgs;
struct RogueClassCloneMethodArgs;
struct RogueClassCmdAccess;
struct RogueClassCmdArgs;
struct RogueCmdFlagArgList;
struct RogueClassCmdFlagArg;
struct RogueClassCmdAssign;
struct RogueCmdControlStructureList;
struct RogueClassCmdControlStructure;
struct RogueClassCmdLiteralThis;
struct RogueClassCmdThisContext;
struct RogueClassCmdGenericLoop;
struct RogueClassCmdLiteralInt32;
struct RogueClassCmdLiteral;
struct RogueClassCmdCompareNE;
struct RogueClassCmdComparison;
struct RogueClassCmdBinary;
struct RogueClassTaskArgs;
struct RogueClassCmdTaskControl;
struct RogueClassCmdTaskControlSection;
struct RogueTableEntry_of_String_MethodListList;
struct RogueClassString_MethodListTableEntry;
struct RogueDefinitionList;
struct RogueClassDefinition;
struct RogueClassString_DefinitionTable;
struct RogueNativePropertyList;
struct RogueClassNativeProperty;
struct RogueClassString_PropertyTable;
struct RogueClassCmdLiteralNull;
struct RogueClassCmdCreateCompound;
struct RogueClassCmdLiteralLogical;
struct RogueClassCmdLiteralString;
struct RogueClassCmdWriteGlobal;
struct RogueClassCmdWriteProperty;
struct RogueTableEntry_of_String_TypeList;
struct RogueClassString_TypeTableEntry;
struct RogueTableEntry_of_String_Int32List;
struct RogueTableEntry_of_String_StringListList;
struct RogueClassString_StringListTableEntry;
struct RogueTableEntry_of_Int32_TemplateList;
struct RogueClassInt32_TemplateTableEntry;
struct RogueClassCmdCastToType;
struct RogueClassCmdTypeOperator;
struct RogueClassCmdLogicalize;
struct RogueClassCmdUnary;
struct RogueClassCmdCreateOptionalValue;
struct RogueClassCmdReadSingleton;
struct RogueClassCmdCallInlineNativeRoutine;
struct RogueClassCmdCallInlineNative;
struct RogueClassCmdCall;
struct RogueClassCmdCallNativeRoutine;
struct RogueClassMacroArgs;
struct RogueClassCmdInlineNative;
struct RogueClassCmdCallRoutine;
struct RogueClassCmdReadArrayCount;
struct RogueClassCmdCallInlineNativeMethod;
struct RogueClassCmdCallNativeMethod;
struct RogueClassCmdCallAspectMethod;
struct RogueClassCmdCallDynamicMethod;
struct RogueClassCmdCallMethod;
struct RogueClassCandidateMethods;
struct RogueClassCmdCreateFunction;
struct RogueFnParamList;
struct RogueClassFnParam;
struct RogueTableEntry_of_String_MethodList;
struct RogueClassString_MethodTableEntry;
struct RogueTableEntry_of_String_LogicalList;
struct RogueClassString_LogicalTableEntry;
struct RogueClassTokenReader;
struct RogueClassString_StringTable;
struct RogueClassTokenizer;
struct RogueClassParseReader;
struct RogueClassPreprocessor;
struct RogueClassEOLToken;
struct RogueClassCmdAdd;
struct RogueClassCmdNativeHeader;
struct RogueClassCmdNativeCode;
struct RogueClassCmdIf;
struct RogueClassCmdWhich;
struct RogueClassCmdContingent;
struct RogueClassCmdTry;
struct RogueClassCmdAwait;
struct RogueClassCmdYield;
struct RogueClassCmdThrow;
struct RogueClassCmdFormattedString;
struct RogueClassCmdTrace;
struct RogueClassCmdEscape;
struct RogueClassCmdNextIteration;
struct RogueClassCmdNecessary;
struct RogueClassCmdSufficient;
struct RogueClassCmdAdjust;
struct RogueClassCmdOpWithAssign;
struct RogueCmdWhichCaseList;
struct RogueClassCmdWhichCase;
struct RogueCmdCatchList;
struct RogueClassCmdCatch;
struct RogueClassCmdLocalDeclaration;
struct RogueClassCmdAdjustLocal;
struct RogueClassCmdReadLocal;
struct RogueClassCmdCompareLE;
struct RogueClassCmdRange;
struct RogueClassCmdLocalOpWithAssign;
struct RogueClassCmdResolvedOpWithAssign;
struct RogueClassCmdRangeUpTo;
struct RogueClassCmdCompareGE;
struct RogueClassCmdForEach;
struct RogueClassCmdRangeDownTo;
struct RogueClassCmdLogicalXor;
struct RogueClassCmdBinaryLogical;
struct RogueClassCmdLogicalOr;
struct RogueClassCmdLogicalAnd;
struct RogueClassCmdCompareEQ;
struct RogueClassCmdCompareIs;
struct RogueClassCmdCompareIsNot;
struct RogueClassCmdCompareLT;
struct RogueClassCmdCompareGT;
struct RogueClassCmdInstanceOf;
struct RogueClassCmdLogicalNot;
struct RogueClassCmdBitwiseXor;
struct RogueClassCmdBitwiseOp;
struct RogueClassCmdBitwiseOr;
struct RogueClassCmdBitwiseAnd;
struct RogueClassCmdBitwiseShiftLeft;
struct RogueClassCmdBitwiseShiftRight;
struct RogueClassCmdBitwiseShiftRightX;
struct RogueClassCmdSubtract;
struct RogueClassCmdMultiply;
struct RogueClassCmdDivide;
struct RogueClassCmdMod;
struct RogueClassCmdPower;
struct RogueClassCmdNegate;
struct RogueClassCmdBitwiseNot;
struct RogueClassCmdGetOptionalValue;
struct RogueClassCmdElementAccess;
struct RogueClassCmdListConvert;
struct RogueClassCmdConvertToType;
struct RogueClassCmdCreateCallback;
struct RogueClassCmdAs;
struct RogueClassCmdDefaultValue;
struct RogueClassCmdLiteralReal64;
struct RogueClassCmdLiteralInt64;
struct RogueClassCmdLiteralCharacter;
struct RogueClassCmdCreateList;
struct RogueClassCmdCallPriorMethod;
struct RogueFnArgList;
struct RogueClassFnArg;
struct RogueClassCmdSelect;
struct RogueCmdSelectCaseList;
struct RogueClassCmdSelectCase;
struct RogueClassCmdTypedLiteralList;
struct RogueTableEntry_of_String_ParseReaderList;
struct RogueClassString_ParseReaderTableEntry;
struct RogueClassFileReader;
struct RogueClassFileWriter;
struct RogueClassTokenListRebuilder;
struct RogueTableEntry_of_String_TokenTypeList;
struct RogueClassString_TokenTypeTableEntry;
struct RogueTableEntry_of_String_TypeSpecializerList;
struct RogueClassString_TypeSpecializerTableEntry;
struct RogueTableEntry_of_String_CmdLabelList;
struct RogueClassString_CmdLabelTableEntry;
struct RogueClassCmdCreateArray;
struct RogueClassCmdCreateObject;
struct RogueClassCmdReadGlobal;
struct RogueClassCmdReadProperty;
struct RogueClassCmdLogicalizeOptionalValue;
struct RogueClassCmdWriteSingleton;
struct RogueClassCmdWriteLocal;
struct RogueClassCmdOpAssignGlobal;
struct RogueClassCmdOpAssignProperty;
struct RogueCmdTaskControlSectionList;
struct RogueClassCmdBlock;
struct RogueTableEntry_of_String_DefinitionList;
struct RogueClassString_DefinitionTableEntry;
struct RogueTableEntry_of_String_PropertyList;
struct RogueClassString_PropertyTableEntry;
struct RogueClassString_CmdTable;
struct RogueClassCmdCallStaticMethod;
struct RogueTableEntry_of_String_StringList;
struct RogueClassString_StringTableEntry;
struct RogueClassDirectiveTokenType;
struct RogueClassStructuralDirectiveTokenType;
struct RogueClassEOLTokenType;
struct RogueClassStructureTokenType;
struct RogueClassNativeCodeTokenType;
struct RogueClassOpWithAssignTokenType;
struct RogueClassString_TokenListTable;
struct RogueClassPreprocessorTokenReader;
struct RogueClassCmdSwitch;
struct RogueClassCmdReadArrayElement;
struct RogueClassCmdWriteArrayElement;
struct RogueClassCmdConvertToPrimitiveType;
struct RogueClassCmdSelectCaseListReader;
struct RogueClassCmdSelectCaseReader;
struct RogueClassCmdAdjustGlobal;
struct RogueClassCmdAdjustProperty;
struct RogueTableEntry_of_String_CmdList;
struct RogueClassString_CmdTableEntry;
struct RogueClassNativeCodeToken;
struct RogueTableEntry_of_String_TokenListList;
struct RogueClassString_TokenListTableEntry;
struct RogueOptionalInt32;
struct RogueOptionalCharacter;

struct RogueOptionalInt32
{
  // PROPERTIES
  RogueInt32 value;
  RogueLogical exists;

  RogueOptionalInt32() : value(0), exists(0) {}

  RogueOptionalInt32( RogueInt32 value ) : value(value), exists(true) {}
  bool operator==( const RogueOptionalInt32 &other ) const
  {
    if (exists)
    {
      if (other.exists) return value == other.value;
      else              return false;
    }
    else
    {
      if (other.exists) return false;
      else              return true;
    }
  }

  bool operator!=( const RogueOptionalInt32 &other ) const
  {
    return !(*this == other);
  }
  
};

struct RogueOptionalCharacter
{
  // PROPERTIES
  RogueCharacter value;
  RogueLogical exists;

  RogueOptionalCharacter() : value(0), exists(0) {}

  RogueOptionalCharacter( RogueCharacter value ) : value(value), exists(true) {}
  bool operator==( const RogueOptionalCharacter &other ) const
  {
    if (exists)
    {
      if (other.exists) return value == other.value;
      else              return false;
    }
    else
    {
      if (other.exists) return false;
      else              return true;
    }
  }

  bool operator!=( const RogueOptionalCharacter &other ) const
  {
    return !(*this == other);
  }
  
};


// GLOBAL PROPERTIES
extern RogueByteList* RogueStringBuilder_work_bytes;
extern RogueClassCmdStatementList* RogueCmdStatementList_current;
extern RogueClassString_TokenTypeTable* RogueTokenType_lookup;
extern RogueClassTokenType* RogueTokenType_directive_define;
extern RogueClassTokenType* RogueTokenType_directive_include;
extern RogueClassTokenType* RogueTokenType_directive_includeNativeCode;
extern RogueClassTokenType* RogueTokenType_directive_includeNativeHeader;
extern RogueClassTokenType* RogueTokenType_directive_if;
extern RogueClassTokenType* RogueTokenType_directive_elseIf;
extern RogueClassTokenType* RogueTokenType_directive_else;
extern RogueClassTokenType* RogueTokenType_directive_endIf;
extern RogueClassTokenType* RogueTokenType_directive_module;
extern RogueClassTokenType* RogueTokenType_directive_requisite;
extern RogueClassTokenType* RogueTokenType_directive_using;
extern RogueClassTokenType* RogueTokenType_placeholder_id;
extern RogueClassTokenType* RogueTokenType_begin_augment_tokens;
extern RogueClassTokenType* RogueTokenType_keyword_augment;
extern RogueClassTokenType* RogueTokenType_keyword_case;
extern RogueClassTokenType* RogueTokenType_keyword_catch;
extern RogueClassTokenType* RogueTokenType_keyword_class;
extern RogueClassTokenType* RogueTokenType_keyword_DEFINITIONS;
extern RogueClassTokenType* RogueTokenType_keyword_else;
extern RogueClassTokenType* RogueTokenType_keyword_elseIf;
extern RogueClassTokenType* RogueTokenType_keyword_endAugment;
extern RogueClassTokenType* RogueTokenType_keyword_endClass;
extern RogueClassTokenType* RogueTokenType_keyword_endContingent;
extern RogueClassTokenType* RogueTokenType_keyword_endForEach;
extern RogueClassTokenType* RogueTokenType_keyword_endFunction;
extern RogueClassTokenType* RogueTokenType_keyword_endIf;
extern RogueClassTokenType* RogueTokenType_keyword_endLoop;
extern RogueClassTokenType* RogueTokenType_keyword_endRoutine;
extern RogueClassTokenType* RogueTokenType_keyword_endTry;
extern RogueClassTokenType* RogueTokenType_keyword_endWhich;
extern RogueClassTokenType* RogueTokenType_keyword_endWhile;
extern RogueClassTokenType* RogueTokenType_keyword_ENUMERATE;
extern RogueClassTokenType* RogueTokenType_keyword_GLOBAL;
extern RogueClassTokenType* RogueTokenType_keyword_GLOBAL_METHODS;
extern RogueClassTokenType* RogueTokenType_keyword_GLOBAL_PROPERTIES;
extern RogueClassTokenType* RogueTokenType_keyword_macro;
extern RogueClassTokenType* RogueTokenType_keyword_method;
extern RogueClassTokenType* RogueTokenType_keyword_METHODS;
extern RogueClassTokenType* RogueTokenType_keyword_nativeCode;
extern RogueClassTokenType* RogueTokenType_keyword_nativeHeader;
extern RogueClassTokenType* RogueTokenType_keyword_others;
extern RogueClassTokenType* RogueTokenType_keyword_PROPERTIES;
extern RogueClassTokenType* RogueTokenType_keyword_routine;
extern RogueClassTokenType* RogueTokenType_keyword_satisfied;
extern RogueClassTokenType* RogueTokenType_keyword_unsatisfied;
extern RogueClassTokenType* RogueTokenType_keyword_with;
extern RogueClassTokenType* RogueTokenType_symbol_close_brace;
extern RogueClassTokenType* RogueTokenType_symbol_close_bracket;
extern RogueClassTokenType* RogueTokenType_symbol_close_comment;
extern RogueClassTokenType* RogueTokenType_symbol_close_paren;
extern RogueClassTokenType* RogueTokenType_symbol_close_specialize;
extern RogueClassTokenType* RogueTokenType_eol;
extern RogueClassTokenType* RogueTokenType_eoi;
extern RogueClassTokenType* RogueTokenType_keyword_await;
extern RogueClassTokenType* RogueTokenType_keyword_contingent;
extern RogueClassTokenType* RogueTokenType_keyword_escapeContingent;
extern RogueClassTokenType* RogueTokenType_keyword_escapeForEach;
extern RogueClassTokenType* RogueTokenType_keyword_escapeIf;
extern RogueClassTokenType* RogueTokenType_keyword_escapeLoop;
extern RogueClassTokenType* RogueTokenType_keyword_escapeTry;
extern RogueClassTokenType* RogueTokenType_keyword_escapeWhich;
extern RogueClassTokenType* RogueTokenType_keyword_escapeWhile;
extern RogueClassTokenType* RogueTokenType_keyword_forEach;
extern RogueClassTokenType* RogueTokenType_keyword_function;
extern RogueClassTokenType* RogueTokenType_keyword_global;
extern RogueClassTokenType* RogueTokenType_keyword_if;
extern RogueClassTokenType* RogueTokenType_keyword_in;
extern RogueClassTokenType* RogueTokenType_keyword_infinity;
extern RogueClassTokenType* RogueTokenType_keyword_is;
extern RogueClassTokenType* RogueTokenType_keyword_isNot;
extern RogueClassTokenType* RogueTokenType_keyword_local;
extern RogueClassTokenType* RogueTokenType_keyword_loop;
extern RogueClassTokenType* RogueTokenType_keyword_NaN;
extern RogueClassTokenType* RogueTokenType_keyword_native;
extern RogueClassTokenType* RogueTokenType_keyword_necessary;
extern RogueClassTokenType* RogueTokenType_keyword_nextIteration;
extern RogueClassTokenType* RogueTokenType_keyword_noAction;
extern RogueClassTokenType* RogueTokenType_keyword_null;
extern RogueClassTokenType* RogueTokenType_keyword_of;
extern RogueClassTokenType* RogueTokenType_keyword_return;
extern RogueClassTokenType* RogueTokenType_keyword_select;
extern RogueClassTokenType* RogueTokenType_keyword_step;
extern RogueClassTokenType* RogueTokenType_keyword_sufficient;
extern RogueClassTokenType* RogueTokenType_keyword_throw;
extern RogueClassTokenType* RogueTokenType_keyword_trace;
extern RogueClassTokenType* RogueTokenType_keyword_trace_position;
extern RogueClassTokenType* RogueTokenType_keyword_try;
extern RogueClassTokenType* RogueTokenType_keyword_which;
extern RogueClassTokenType* RogueTokenType_keyword_while;
extern RogueClassTokenType* RogueTokenType_keyword_yield;
extern RogueClassTokenType* RogueTokenType_identifier;
extern RogueClassTokenType* RogueTokenType_type_identifier;
extern RogueClassTokenType* RogueTokenType_literal_character;
extern RogueClassTokenType* RogueTokenType_literal_integer;
extern RogueClassTokenType* RogueTokenType_literal_long;
extern RogueClassTokenType* RogueTokenType_literal_real;
extern RogueClassTokenType* RogueTokenType_literal_string;
extern RogueClassTokenType* RogueTokenType_keyword_and;
extern RogueClassTokenType* RogueTokenType_keyword_as;
extern RogueClassTokenType* RogueTokenType_keyword_downTo;
extern RogueClassTokenType* RogueTokenType_keyword_false;
extern RogueClassTokenType* RogueTokenType_keyword_instanceOf;
extern RogueClassTokenType* RogueTokenType_keyword_meta;
extern RogueClassTokenType* RogueTokenType_keyword_not;
extern RogueClassTokenType* RogueTokenType_keyword_notInstanceOf;
extern RogueClassTokenType* RogueTokenType_keyword_or;
extern RogueClassTokenType* RogueTokenType_keyword_pi;
extern RogueClassTokenType* RogueTokenType_keyword_prior;
extern RogueClassTokenType* RogueTokenType_keyword_this;
extern RogueClassTokenType* RogueTokenType_keyword_true;
extern RogueClassTokenType* RogueTokenType_keyword_xor;
extern RogueClassTokenType* RogueTokenType_symbol_ampersand;
extern RogueClassTokenType* RogueTokenType_symbol_ampersand_equals;
extern RogueClassTokenType* RogueTokenType_symbol_arrow;
extern RogueClassTokenType* RogueTokenType_symbol_at;
extern RogueClassTokenType* RogueTokenType_symbol_backslash;
extern RogueClassTokenType* RogueTokenType_symbol_caret;
extern RogueClassTokenType* RogueTokenType_symbol_caret_equals;
extern RogueClassTokenType* RogueTokenType_symbol_colon;
extern RogueClassTokenType* RogueTokenType_symbol_comma;
extern RogueClassTokenType* RogueTokenType_symbol_compare;
extern RogueClassTokenType* RogueTokenType_symbol_dot;
extern RogueClassTokenType* RogueTokenType_symbol_dot_equals;
extern RogueClassTokenType* RogueTokenType_symbol_downToGreaterThan;
extern RogueClassTokenType* RogueTokenType_symbol_empty_braces;
extern RogueClassTokenType* RogueTokenType_symbol_empty_brackets;
extern RogueClassTokenType* RogueTokenType_symbol_eq;
extern RogueClassTokenType* RogueTokenType_symbol_equals;
extern RogueClassTokenType* RogueTokenType_symbol_exclamation_point;
extern RogueClassTokenType* RogueTokenType_symbol_fat_arrow;
extern RogueClassTokenType* RogueTokenType_symbol_ge;
extern RogueClassTokenType* RogueTokenType_symbol_gt;
extern RogueClassTokenType* RogueTokenType_symbol_le;
extern RogueClassTokenType* RogueTokenType_symbol_lt;
extern RogueClassTokenType* RogueTokenType_symbol_minus;
extern RogueClassTokenType* RogueTokenType_symbol_minus_equals;
extern RogueClassTokenType* RogueTokenType_symbol_minus_minus;
extern RogueClassTokenType* RogueTokenType_symbol_ne;
extern RogueClassTokenType* RogueTokenType_symbol_open_brace;
extern RogueClassTokenType* RogueTokenType_symbol_open_bracket;
extern RogueClassTokenType* RogueTokenType_symbol_open_paren;
extern RogueClassTokenType* RogueTokenType_symbol_open_specialize;
extern RogueClassTokenType* RogueTokenType_symbol_percent;
extern RogueClassTokenType* RogueTokenType_symbol_percent_equals;
extern RogueClassTokenType* RogueTokenType_symbol_plus;
extern RogueClassTokenType* RogueTokenType_symbol_plus_equals;
extern RogueClassTokenType* RogueTokenType_symbol_plus_plus;
extern RogueClassTokenType* RogueTokenType_symbol_question_mark;
extern RogueClassTokenType* RogueTokenType_symbol_semicolon;
extern RogueClassTokenType* RogueTokenType_symbol_shift_left;
extern RogueClassTokenType* RogueTokenType_symbol_shift_right;
extern RogueClassTokenType* RogueTokenType_symbol_shift_right_x;
extern RogueClassTokenType* RogueTokenType_symbol_slash;
extern RogueClassTokenType* RogueTokenType_symbol_slash_equals;
extern RogueClassTokenType* RogueTokenType_symbol_tilde;
extern RogueClassTokenType* RogueTokenType_symbol_tilde_equals;
extern RogueClassTokenType* RogueTokenType_symbol_times;
extern RogueClassTokenType* RogueTokenType_symbol_times_equals;
extern RogueClassTokenType* RogueTokenType_symbol_upTo;
extern RogueClassTokenType* RogueTokenType_symbol_upToLessThan;
extern RogueClassTokenType* RogueTokenType_symbol_vertical_bar;
extern RogueClassTokenType* RogueTokenType_symbol_vertical_bar_equals;
extern RogueClassTokenType* RogueTokenType_symbol_double_vertical_bar;
extern RogueStringList* RogueSystem_command_line_arguments;
extern RogueString* RogueSystem_executable_filepath;
extern RogueClassString_TokenListTable* RoguePreprocessor_definitions;




struct RogueStringBuilder : RogueObject
{
  // GLOBAL PROPERTIES
  static RogueByteList* work_bytes;

  // PROPERTIES
  RogueCharacterList* characters;
  RogueInt32 indent;
  RogueLogical at_newline;

};




struct RogueCharacterList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassGenericList : RogueObject
{
  // PROPERTIES

};

struct RogueClassStringReader : RogueObject
{
  // PROPERTIES
  RogueInt32 position;
  RogueInt32 count;
  RogueString* string;

};

struct RogueClassCharacterReader : RogueObject
{
};

struct RogueStringList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassGlobal : RogueObject
{
  // PROPERTIES
  RogueStringBuilder* printwriter_output_buffer;
  RogueClassConsole* standard_output;

};

struct RogueClassPrintWriterAspect : RogueObject
{
};

struct RogueClassConsole : RogueObject
{
  // PROPERTIES
  RogueInt32 position;

};

struct RogueClassCharacterWriter : RogueObject
{
};

struct RogueClassRogueC : RogueObject
{
  // PROPERTIES
  RogueStringList* included_files;
  RogueStringList* prefix_path_list;
  RogueClassString_LogicalTable* prefix_path_lookup;
  RogueClassString_LogicalTable* compile_targets;
  RogueStringList* target_list;
  RogueString* libraries_folder;
  RogueStringList* source_files;
  RogueLogical generate_main;
  RogueString* first_filepath;
  RogueString* output_filepath;
  RogueStringList* supported_targets;
  RogueStringList* requisite_declarations;
  RogueString* execute_args;
  RogueString* package_name;
  RogueLogical debug_mode;
  RogueParserList* parsers;
  RogueClassString_ParseReaderTable* parsereaders_by_filepath;

};

struct RogueClassException : RogueObject
{
  // PROPERTIES
  RogueString* message;
  RogueClassStackTrace* stack_trace;

};

struct RogueClassStackTrace : RogueObject
{
  // PROPERTIES
  RogueStringList* entries;
  RogueInt32 count;
  RogueLogical is_formatted;

};

struct RogueClassReader_of_String : RogueObject
{
};

struct RogueClassMath : RogueObject
{
  // PROPERTIES

};

struct RogueClassProgram : RogueObject
{
  // PROPERTIES
  RogueString* code_prefix;
  RogueString* program_name;
  RogueInt32 unique_integer;
  RogueTemplateList* template_list;
  RogueClassString_TemplateTable* template_lookup;
  RogueClassString_AugmentListTable* augment_lookup;
  RogueRequisiteItemList* requisite_list;
  RogueString* first_filepath;
  RogueClassMethod* m_on_launch;
  RoguePropertyList* global_properties;
  RogueStringList* native_header;
  RogueStringList* native_code;
  RogueClassString_MethodListTable* methods_by_signature;
  RogueTypeList* type_list;
  RogueClassString_TypeTable* type_lookup;
  RogueClassType* type_null;
  RogueClassType* type_Real64;
  RogueClassType* type_Real32;
  RogueClassType* type_Int64;
  RogueClassType* type_Int32;
  RogueClassType* type_Character;
  RogueClassType* type_Byte;
  RogueClassType* type_Logical;
  RogueClassType* type_Object;
  RogueClassType* type_String;
  RogueClassType* type_NativeArray;
  RogueClassType* type_GenericList;
  RogueClassType* type_Global;
  RogueClassType* type_Exception;
  RogueClassType* type_StringBuilder;
  RogueClassString_Int32Table* literal_string_lookup;
  RogueStringList* literal_string_list;
  RogueStringBuilder* string_buffer;
  RogueClassString_StringListTable* ids_by_module;

};

struct RogueTemplateList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassTemplate : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueTokenList* tokens;
  RogueClassAttributes* attributes;
  RogueTypeParameterList* type_parameters;
  RogueClassInt32_TemplateTable* alternates_by_param_count;

};

struct RogueClassString_TemplateTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_TemplateList* bins;
  RogueStringList* keys;

};

struct RogueClassString_AugmentListTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_AugmentListList* bins;
  RogueStringList* keys;

};

struct RogueRequisiteItemList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassRequisiteItem : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _type;
  RogueString* signature;

};

struct RogueClassMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* type_context;
  RogueString* name;
  RogueString* signature;
  RogueClassAttributes* attributes;
  RogueClassType* _return_type;
  RogueClassType* _task_result_type;
  RogueLocalList* parameters;
  RogueLocalList* locals;
  RogueInt32 min_args;
  RogueClassCmdStatementList* statements;
  RogueClassCmdStatementList* aspect_statements;
  RogueTypeList* incorporating_classes;
  RogueClassMethod* overridden_method;
  RogueMethodList* overriding_methods;
  RogueString* native_code;
  RogueLogical organized;
  RogueLogical resolved;
  RogueInt32 index;
  RogueLogical is_used;
  RogueLogical called_dynamically;
  RogueCmdLabelList* label_list;
  RogueClassString_CmdLabelTable* label_lookup;
  RogueClassCmdLabel* cur_label;
  RogueString* cpp_name;
  RogueString* cpp_typedef;

};

struct RoguePropertyList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* type_context;
  RogueString* name;
  RogueClassType* _type;
  RogueInt32 attributes;
  RogueClassCmd* initial_value;
  RogueString* cpp_name;

};

struct RogueClassString_MethodListTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_MethodListList* bins;
  RogueStringList* keys;

};

struct RogueTypeList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassType : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueClassAttributes* attributes;
  RogueInt32 index;
  RogueClassTemplate* source_template;
  RogueTypeSpecializerList* specializers;
  RogueLogical defined;
  RogueLogical organized;
  RogueLogical resolved;
  RogueLogical culled;
  RogueClassType* base_class;
  RogueTypeList* base_types;
  RogueTypeList* flat_base_types;
  RogueLogical has_extended_class;
  RogueLogical is_array;
  RogueLogical is_list;
  RogueLogical is_optional;
  RogueClassType* _element_type;
  RogueLogical is_used;
  RogueLogical simplify_name;
  RogueDefinitionList* definition_list;
  RogueClassString_DefinitionTable* definition_lookup;
  RogueClassCmd* prev_enum_cmd;
  RogueInt32 next_enum_offset;
  RogueNativePropertyList* native_properties;
  RoguePropertyList* global_list;
  RogueClassString_PropertyTable* global_lookup;
  RoguePropertyList* property_list;
  RogueClassString_PropertyTable* property_lookup;
  RogueMethodList* global_method_list;
  RogueClassString_MethodListTable* routine_lookup_by_name;
  RogueClassString_MethodTable* routine_lookup_by_signature;
  RogueMethodList* method_list;
  RogueClassString_MethodListTable* method_lookup_by_name;
  RogueClassString_MethodTable* method_lookup_by_signature;
  RogueInt32 dynamic_method_table_index;
  RogueTypeList* callback_parameter_types;
  RogueClassType* _callback_return_type;
  RogueString* cpp_name;
  RogueString* cpp_class_name;
  RogueString* cpp_type_name;

};

struct RogueClassString_TypeTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_TypeList* bins;
  RogueStringList* keys;

};

struct RogueClassString_Int32Table : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_Int32List* bins;
  RogueStringList* keys;

};

struct RogueClassString_StringListTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_StringListList* bins;
  RogueStringList* keys;

};

struct RogueClassString_Int32TableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueInt32 value;
  RogueClassString_Int32TableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassInt32_TemplateTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_Int32_TemplateList* bins;
  RogueInt32List* keys;

};

struct RogueClassToken : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;

};

struct RogueClassError : RogueClassException
{
  // PROPERTIES

};

struct RogueClassRogueError : RogueClassError
{
  // PROPERTIES
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;

};

struct RogueClassAttributes : RogueObject
{
  // PROPERTIES
  RogueInt32 flags;
  RogueStringList* tags;

};

struct RogueClassCmd : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;

};

struct RogueClassCmdReturn : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* value;

};

struct RogueClassCmdStatement : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;

};

struct RogueClassCmdStatementList : RogueObject
{
  // GLOBAL PROPERTIES
  static RogueClassCmdStatementList* current;

  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueCmdList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassTokenType : RogueObject
{
  // GLOBAL PROPERTIES
  static RogueClassString_TokenTypeTable* lookup;
  static RogueClassTokenType* directive_define;
  static RogueClassTokenType* directive_include;
  static RogueClassTokenType* directive_includeNativeCode;
  static RogueClassTokenType* directive_includeNativeHeader;
  static RogueClassTokenType* directive_if;
  static RogueClassTokenType* directive_elseIf;
  static RogueClassTokenType* directive_else;
  static RogueClassTokenType* directive_endIf;
  static RogueClassTokenType* directive_module;
  static RogueClassTokenType* directive_requisite;
  static RogueClassTokenType* directive_using;
  static RogueClassTokenType* placeholder_id;
  static RogueClassTokenType* begin_augment_tokens;
  static RogueClassTokenType* keyword_augment;
  static RogueClassTokenType* keyword_case;
  static RogueClassTokenType* keyword_catch;
  static RogueClassTokenType* keyword_class;
  static RogueClassTokenType* keyword_DEFINITIONS;
  static RogueClassTokenType* keyword_else;
  static RogueClassTokenType* keyword_elseIf;
  static RogueClassTokenType* keyword_endAugment;
  static RogueClassTokenType* keyword_endClass;
  static RogueClassTokenType* keyword_endContingent;
  static RogueClassTokenType* keyword_endForEach;
  static RogueClassTokenType* keyword_endFunction;
  static RogueClassTokenType* keyword_endIf;
  static RogueClassTokenType* keyword_endLoop;
  static RogueClassTokenType* keyword_endRoutine;
  static RogueClassTokenType* keyword_endTry;
  static RogueClassTokenType* keyword_endWhich;
  static RogueClassTokenType* keyword_endWhile;
  static RogueClassTokenType* keyword_ENUMERATE;
  static RogueClassTokenType* keyword_GLOBAL;
  static RogueClassTokenType* keyword_GLOBAL_METHODS;
  static RogueClassTokenType* keyword_GLOBAL_PROPERTIES;
  static RogueClassTokenType* keyword_macro;
  static RogueClassTokenType* keyword_method;
  static RogueClassTokenType* keyword_METHODS;
  static RogueClassTokenType* keyword_nativeCode;
  static RogueClassTokenType* keyword_nativeHeader;
  static RogueClassTokenType* keyword_others;
  static RogueClassTokenType* keyword_PROPERTIES;
  static RogueClassTokenType* keyword_routine;
  static RogueClassTokenType* keyword_satisfied;
  static RogueClassTokenType* keyword_unsatisfied;
  static RogueClassTokenType* keyword_with;
  static RogueClassTokenType* symbol_close_brace;
  static RogueClassTokenType* symbol_close_bracket;
  static RogueClassTokenType* symbol_close_comment;
  static RogueClassTokenType* symbol_close_paren;
  static RogueClassTokenType* symbol_close_specialize;
  static RogueClassTokenType* eol;
  static RogueClassTokenType* eoi;
  static RogueClassTokenType* keyword_await;
  static RogueClassTokenType* keyword_contingent;
  static RogueClassTokenType* keyword_escapeContingent;
  static RogueClassTokenType* keyword_escapeForEach;
  static RogueClassTokenType* keyword_escapeIf;
  static RogueClassTokenType* keyword_escapeLoop;
  static RogueClassTokenType* keyword_escapeTry;
  static RogueClassTokenType* keyword_escapeWhich;
  static RogueClassTokenType* keyword_escapeWhile;
  static RogueClassTokenType* keyword_forEach;
  static RogueClassTokenType* keyword_function;
  static RogueClassTokenType* keyword_global;
  static RogueClassTokenType* keyword_if;
  static RogueClassTokenType* keyword_in;
  static RogueClassTokenType* keyword_infinity;
  static RogueClassTokenType* keyword_is;
  static RogueClassTokenType* keyword_isNot;
  static RogueClassTokenType* keyword_local;
  static RogueClassTokenType* keyword_loop;
  static RogueClassTokenType* keyword_NaN;
  static RogueClassTokenType* keyword_native;
  static RogueClassTokenType* keyword_necessary;
  static RogueClassTokenType* keyword_nextIteration;
  static RogueClassTokenType* keyword_noAction;
  static RogueClassTokenType* keyword_null;
  static RogueClassTokenType* keyword_of;
  static RogueClassTokenType* keyword_return;
  static RogueClassTokenType* keyword_select;
  static RogueClassTokenType* keyword_step;
  static RogueClassTokenType* keyword_sufficient;
  static RogueClassTokenType* keyword_throw;
  static RogueClassTokenType* keyword_trace;
  static RogueClassTokenType* keyword_trace_position;
  static RogueClassTokenType* keyword_try;
  static RogueClassTokenType* keyword_which;
  static RogueClassTokenType* keyword_while;
  static RogueClassTokenType* keyword_yield;
  static RogueClassTokenType* identifier;
  static RogueClassTokenType* type_identifier;
  static RogueClassTokenType* literal_character;
  static RogueClassTokenType* literal_integer;
  static RogueClassTokenType* literal_long;
  static RogueClassTokenType* literal_real;
  static RogueClassTokenType* literal_string;
  static RogueClassTokenType* keyword_and;
  static RogueClassTokenType* keyword_as;
  static RogueClassTokenType* keyword_downTo;
  static RogueClassTokenType* keyword_false;
  static RogueClassTokenType* keyword_instanceOf;
  static RogueClassTokenType* keyword_meta;
  static RogueClassTokenType* keyword_not;
  static RogueClassTokenType* keyword_notInstanceOf;
  static RogueClassTokenType* keyword_or;
  static RogueClassTokenType* keyword_pi;
  static RogueClassTokenType* keyword_prior;
  static RogueClassTokenType* keyword_this;
  static RogueClassTokenType* keyword_true;
  static RogueClassTokenType* keyword_xor;
  static RogueClassTokenType* symbol_ampersand;
  static RogueClassTokenType* symbol_ampersand_equals;
  static RogueClassTokenType* symbol_arrow;
  static RogueClassTokenType* symbol_at;
  static RogueClassTokenType* symbol_backslash;
  static RogueClassTokenType* symbol_caret;
  static RogueClassTokenType* symbol_caret_equals;
  static RogueClassTokenType* symbol_colon;
  static RogueClassTokenType* symbol_comma;
  static RogueClassTokenType* symbol_compare;
  static RogueClassTokenType* symbol_dot;
  static RogueClassTokenType* symbol_dot_equals;
  static RogueClassTokenType* symbol_downToGreaterThan;
  static RogueClassTokenType* symbol_empty_braces;
  static RogueClassTokenType* symbol_empty_brackets;
  static RogueClassTokenType* symbol_eq;
  static RogueClassTokenType* symbol_equals;
  static RogueClassTokenType* symbol_exclamation_point;
  static RogueClassTokenType* symbol_fat_arrow;
  static RogueClassTokenType* symbol_ge;
  static RogueClassTokenType* symbol_gt;
  static RogueClassTokenType* symbol_le;
  static RogueClassTokenType* symbol_lt;
  static RogueClassTokenType* symbol_minus;
  static RogueClassTokenType* symbol_minus_equals;
  static RogueClassTokenType* symbol_minus_minus;
  static RogueClassTokenType* symbol_ne;
  static RogueClassTokenType* symbol_open_brace;
  static RogueClassTokenType* symbol_open_bracket;
  static RogueClassTokenType* symbol_open_paren;
  static RogueClassTokenType* symbol_open_specialize;
  static RogueClassTokenType* symbol_percent;
  static RogueClassTokenType* symbol_percent_equals;
  static RogueClassTokenType* symbol_plus;
  static RogueClassTokenType* symbol_plus_equals;
  static RogueClassTokenType* symbol_plus_plus;
  static RogueClassTokenType* symbol_question_mark;
  static RogueClassTokenType* symbol_semicolon;
  static RogueClassTokenType* symbol_shift_left;
  static RogueClassTokenType* symbol_shift_right;
  static RogueClassTokenType* symbol_shift_right_x;
  static RogueClassTokenType* symbol_slash;
  static RogueClassTokenType* symbol_slash_equals;
  static RogueClassTokenType* symbol_tilde;
  static RogueClassTokenType* symbol_tilde_equals;
  static RogueClassTokenType* symbol_times;
  static RogueClassTokenType* symbol_times_equals;
  static RogueClassTokenType* symbol_upTo;
  static RogueClassTokenType* symbol_upToLessThan;
  static RogueClassTokenType* symbol_vertical_bar;
  static RogueClassTokenType* symbol_vertical_bar_equals;
  static RogueClassTokenType* symbol_double_vertical_bar;

  // PROPERTIES
  RogueString* name;

};

struct RogueClassCmdLabel : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueClassCmdStatementList* statements;
  RogueLogical is_referenced;

};

struct RogueClassScope : RogueObject
{
  // PROPERTIES
  RogueClassType* _this_type;
  RogueClassMethod* this_method;
  RogueClassCmdStatementList* this_body;
  RogueClassType* _implicit_type;
  RogueLocalList* local_list;
  RogueCmdControlStructureList* control_stack;

};

struct RogueMethodList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassFunction_Type_RETURNSLogical : RogueObject
{
  // PROPERTIES

};

struct RogueClassFunction_197 : RogueObject
{
  // PROPERTIES

};

struct RogueClassFunction_198 : RogueObject
{
  // PROPERTIES

};

struct RogueClassCPPWriter : RogueObject
{
  // PROPERTIES
  RogueString* filepath;
  RogueStringBuilder* buffer;
  RogueInt32 indent;
  RogueLogical needs_indent;
  RogueInt32 line_number;
  RogueStringBuilder* temp_buffer;

};

struct RogueClassString_MethodTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_MethodList* bins;
  RogueStringList* keys;

};

struct RogueLocalList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassLocal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueString* original_name;
  RogueClassType* _type;
  RogueInt32 index;
  RogueClassCmd* initial_value;
  RogueString* _cpp_name;

};

struct RogueInt32List : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueByteList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassSystem : RogueObject
{
  // GLOBAL PROPERTIES
  static RogueStringList* command_line_arguments;
  static RogueString* executable_filepath;

  // PROPERTIES

};

struct RogueClassString_LogicalTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_LogicalList* bins;
  RogueStringList* keys;

};

struct RogueParserList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassParser : RogueObject
{
  // PROPERTIES
  RogueClassTokenReader* reader;
  RogueString* filepath;
  RogueClassType* _this_type;
  RogueClassMethod* this_method;
  RogueLocalList* local_declarations;
  RoguePropertyList* property_list;
  RogueStringBuilder* string_buffer;
  RogueClassCmdStatementList* cur_statement_list;
  RogueLogical parsing_augment;
  RogueStringList* used_modules;
  RogueClassString_StringTable* module_id_map;

};

struct RogueClassString_ParseReaderTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_ParseReaderList* bins;
  RogueStringList* keys;

};

struct RogueClassFile : RogueObject
{
  // PROPERTIES
  RogueString* filepath;

};

struct RogueTokenList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassLineReader : RogueObject
{
  // PROPERTIES
  RogueInt32 position;
  RogueClassCharacterReader* source;
  RogueString* next;
  RogueStringBuilder* buffer;
  RogueCharacter prev;

};

struct RogueTypeParameterList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassTypeParameter : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;

};

struct RogueAugmentList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassAugment : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueTypeList* base_types;
  RogueTokenList* tokens;

};

struct RogueClassString_TokenTypeTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_TokenTypeList* bins;
  RogueStringList* keys;

};

struct RogueClassLiteralCharacterToken : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueCharacter value;

};

struct RogueClassLiteralInt64Token : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueInt64 value;

};

struct RogueClassLiteralInt32Token : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueInt32 value;

};

struct RogueClassLiteralReal64Token : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueReal64 value;

};

struct RogueClassStringDataToken : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueString* value;

};

struct RogueClassString_TypeSpecializerTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_TypeSpecializerList* bins;
  RogueStringList* keys;

};

struct RogueClassTypeSpecializer : RogueObject
{
  // PROPERTIES
  RogueString* name;
  RogueInt32 index;
  RogueTokenList* tokens;

};

struct RogueTypeSpecializerList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueTableEntry_of_String_TemplateList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_TemplateTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassTemplate* value;
  RogueClassString_TemplateTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_AugmentListList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_AugmentListTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueAugmentList* value;
  RogueClassString_AugmentListTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueCmdLabelList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_CmdLabelTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_CmdLabelList* bins;
  RogueStringList* keys;

};

struct RogueClassCloneArgs : RogueObject
{
  // PROPERTIES

};

struct RogueClassCloneMethodArgs : RogueObject
{
  // PROPERTIES
  RogueClassMethod* cloned_method;

};

struct RogueClassCmdAccess : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueString* name;
  RogueClassCmdArgs* args;
  RogueCmdFlagArgList* flag_args;

};

struct RogueClassCmdArgs : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueCmdFlagArgList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdFlagArg : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueClassCmd* value;
  RogueLogical is_negative;

};

struct RogueClassCmdAssign : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* target;
  RogueClassCmd* new_value;

};

struct RogueCmdControlStructureList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdControlStructure : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;

};

struct RogueClassCmdLiteralThis : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _this_type;

};

struct RogueClassCmdThisContext : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _this_type;

};

struct RogueClassCmdGenericLoop : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueClassCmdStatementList* control_statements;
  RogueClassCmd* condition;
  RogueClassCmdStatementList* upkeep;

};

struct RogueClassCmdLiteralInt32 : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueInt32 value;

};

struct RogueClassCmdLiteral : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;

};

struct RogueClassCmdCompareNE : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdComparison : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdBinary : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassTaskArgs : RogueObject
{
  // PROPERTIES
  RogueClassType* _task_type;
  RogueClassMethod* task_method;
  RogueClassType* _original_type;
  RogueClassMethod* original_method;
  RogueClassCmdTaskControl* cmd_task_control;
  RogueClassProperty* context_property;
  RogueClassProperty* ip_property;

};

struct RogueClassCmdTaskControl : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueCmdTaskControlSectionList* sections;
  RogueClassCmdTaskControlSection* current_section;

};

struct RogueClassCmdTaskControlSection : RogueObject
{
  // PROPERTIES
  RogueInt32 ip;
  RogueClassCmdStatementList* statements;

};

struct RogueTableEntry_of_String_MethodListList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_MethodListTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueMethodList* value;
  RogueClassString_MethodListTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueDefinitionList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassDefinition : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueClassCmd* expression;
  RogueLogical is_enumeration;

};

struct RogueClassString_DefinitionTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_DefinitionList* bins;
  RogueStringList* keys;

};

struct RogueNativePropertyList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassNativeProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* code;

};

struct RogueClassString_PropertyTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_PropertyList* bins;
  RogueStringList* keys;

};

struct RogueClassCmdLiteralNull : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;

};

struct RogueClassCmdCreateCompound : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdLiteralLogical : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueLogical value;

};

struct RogueClassCmdLiteralString : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* value;
  RogueInt32 index;

};

struct RogueClassCmdWriteGlobal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassProperty* global_info;
  RogueClassCmd* new_value;

};

struct RogueClassCmdWriteProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassProperty* property_info;
  RogueClassCmd* new_value;

};

struct RogueTableEntry_of_String_TypeList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_TypeTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassType* value;
  RogueClassString_TypeTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_Int32List : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueTableEntry_of_String_StringListList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_StringListTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueStringList* value;
  RogueClassString_StringListTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_Int32_TemplateList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassInt32_TemplateTableEntry : RogueObject
{
  // PROPERTIES
  RogueInt32 key;
  RogueClassTemplate* value;
  RogueClassInt32_TemplateTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassCmdCastToType : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdTypeOperator : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdLogicalize : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;

};

struct RogueClassCmdUnary : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;

};

struct RogueClassCmdCreateOptionalValue : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;
  RogueClassCmd* value;

};

struct RogueClassCmdReadSingleton : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;

};

struct RogueClassCmdCallInlineNativeRoutine : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;
  RogueInt32 adjust_delta;

};

struct RogueClassCmdCallInlineNative : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;
  RogueInt32 adjust_delta;

};

struct RogueClassCmdCall : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdCallNativeRoutine : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassMacroArgs : RogueObject
{
  // PROPERTIES
  RogueClassCmd* this_context;
  RogueClassMethod* method_info;
  RogueClassString_CmdTable* arg_lookup;

};

struct RogueClassCmdInlineNative : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* code;
  RogueClassMethod* this_method;
  RogueClassType* _result_type;
  RogueClassTokenType* modify_op;
  RogueInt32 adjust_delta;
  RogueClassCmd* new_value;

};

struct RogueClassCmdCallRoutine : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdReadArrayCount : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassType* _array_type;

};

struct RogueClassCmdCallInlineNativeMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;
  RogueInt32 adjust_delta;

};

struct RogueClassCmdCallNativeMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdCallAspectMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdCallDynamicMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdCallMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueClassCandidateMethods : RogueObject
{
  // PROPERTIES
  RogueClassType* type_context;
  RogueClassCmdAccess* access;
  RogueMethodList* available;
  RogueMethodList* compatible;
  RogueLogical error_on_fail;

};

struct RogueClassCmdCreateFunction : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueFnParamList* parameters;
  RogueClassType* _return_type;
  RogueFnArgList* with_args;
  RogueClassCmdStatementList* statements;
  RogueLogical is_generic;

};

struct RogueFnParamList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassFnParam : RogueObject
{
  // PROPERTIES
  RogueString* name;
  RogueClassType* _type;

};

struct RogueTableEntry_of_String_MethodList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_MethodTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassMethod* value;
  RogueClassString_MethodTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_LogicalList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_LogicalTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueLogical value;
  RogueClassString_LogicalTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassTokenReader : RogueObject
{
  // PROPERTIES
  RogueTokenList* tokens;
  RogueInt32 position;
  RogueInt32 count;

};

struct RogueClassString_StringTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_StringList* bins;
  RogueStringList* keys;

};

struct RogueClassTokenizer : RogueObject
{
  // PROPERTIES
  RogueString* filepath;
  RogueClassParseReader* reader;
  RogueTokenList* tokens;
  RogueStringBuilder* buffer;
  RogueString* next_filepath;
  RogueInt32 next_line;
  RogueInt32 next_column;

};

struct RogueClassParseReader : RogueObject
{
  // PROPERTIES
  RogueInt32 position;
  RogueCharacterList* data;
  RogueInt32 count;
  RogueInt32 line;
  RogueInt32 column;
  RogueInt32 spaces_per_tab;

};

struct RogueClassPreprocessor : RogueObject
{
  // GLOBAL PROPERTIES
  static RogueClassString_TokenListTable* definitions;

  // PROPERTIES
  RogueClassParser* parser;
  RogueClassPreprocessorTokenReader* reader;
  RogueTokenList* tokens;
  RogueString* cur_module;

};

struct RogueClassEOLToken : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueString* comment;

};

struct RogueClassCmdAdd : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdNativeHeader : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* code;

};

struct RogueClassCmdNativeCode : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* code;

};

struct RogueClassCmdIf : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueClassCmd* condition;
  RogueClassCmdStatementList* else_statements;

};

struct RogueClassCmdWhich : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueClassCmd* expression;
  RogueCmdWhichCaseList* cases;
  RogueClassCmdWhichCase* case_others;

};

struct RogueClassCmdContingent : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueClassCmdStatementList* satisfied_statements;
  RogueClassCmdStatementList* unsatisfied_statements;
  RogueString* satisfied_label;
  RogueString* unsatisfied_label;
  RogueClassCmdTaskControlSection* satisfied_section;
  RogueClassCmdTaskControlSection* unsatisfied_section;

};

struct RogueClassCmdTry : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueCmdCatchList* catches;

};

struct RogueClassCmdAwait : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* expression;
  RogueClassCmdStatementList* statement_list;
  RogueClassLocal* result_var;

};

struct RogueClassCmdYield : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;

};

struct RogueClassCmdThrow : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* expression;

};

struct RogueClassCmdFormattedString : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* format;
  RogueClassCmdArgs* args;

};

struct RogueClassCmdTrace : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* info;

};

struct RogueClassCmdEscape : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueInt32 _control_type;
  RogueClassCmdControlStructure* target_cmd;

};

struct RogueClassCmdNextIteration : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdControlStructure* target_cmd;

};

struct RogueClassCmdNecessary : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdContingent* target_cmd;
  RogueClassCmd* condition;

};

struct RogueClassCmdSufficient : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdContingent* target_cmd;
  RogueClassCmd* condition;

};

struct RogueClassCmdAdjust : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueInt32 delta;

};

struct RogueClassCmdOpWithAssign : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* target;
  RogueClassTokenType* op;
  RogueClassCmd* new_value;

};

struct RogueCmdWhichCaseList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdWhichCase : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdArgs* conditions;
  RogueClassCmdStatementList* statements;

};

struct RogueCmdCatchList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdCatch : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* error_var;
  RogueClassCmdStatementList* statements;

};

struct RogueClassCmdLocalDeclaration : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* local_info;
  RogueLogical skip_initialization;

};

struct RogueClassCmdAdjustLocal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* local_info;
  RogueInt32 delta;

};

struct RogueClassCmdReadLocal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* local_info;

};

struct RogueClassCmdCompareLE : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdRange : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* first;
  RogueClassCmd* last;
  RogueClassCmd* step_size;

};

struct RogueClassCmdLocalOpWithAssign : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassTokenType* op;
  RogueClassCmd* new_value;
  RogueClassLocal* local_info;

};

struct RogueClassCmdResolvedOpWithAssign : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassTokenType* op;
  RogueClassCmd* new_value;

};

struct RogueClassCmdRangeUpTo : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* first;
  RogueClassCmd* last;
  RogueClassCmd* step_size;

};

struct RogueClassCmdCompareGE : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdForEach : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueString* control_var_name;
  RogueString* index_var_name;
  RogueClassCmd* collection;
  RogueClassCmd* step_cmd;

};

struct RogueClassCmdRangeDownTo : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* first;
  RogueClassCmd* last;
  RogueClassCmd* step_size;

};

struct RogueClassCmdLogicalXor : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBinaryLogical : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdLogicalOr : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdLogicalAnd : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdCompareEQ : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdCompareIs : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdCompareIsNot : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdCompareLT : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdCompareGT : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;
  RogueLogical resolved;

};

struct RogueClassCmdInstanceOf : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdLogicalNot : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;

};

struct RogueClassCmdBitwiseXor : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseOp : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseOr : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseAnd : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseShiftLeft : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseShiftRight : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdBitwiseShiftRightX : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdSubtract : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdMultiply : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdDivide : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdMod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdPower : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* left;
  RogueClassCmd* right;

};

struct RogueClassCmdNegate : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;

};

struct RogueClassCmdBitwiseNot : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;

};

struct RogueClassCmdGetOptionalValue : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* value;

};

struct RogueClassCmdElementAccess : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassCmd* index;

};

struct RogueClassCmdListConvert : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* list;
  RogueClassType* _to_type;
  RogueClassCmd* convert_fn;

};

struct RogueClassCmdConvertToType : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdCreateCallback : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueString* name;
  RogueString* signature;
  RogueClassType* _return_type;

};

struct RogueClassCmdAs : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdDefaultValue : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;

};

struct RogueClassCmdLiteralReal64 : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueReal64 value;

};

struct RogueClassCmdLiteralInt64 : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueInt64 value;

};

struct RogueClassCmdLiteralCharacter : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueCharacter value;

};

struct RogueClassCmdCreateList : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdArgs* args;
  RogueClassType* _list_type;

};

struct RogueClassCmdCallPriorMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* name;
  RogueClassCmdArgs* args;
  RogueCmdFlagArgList* flag_args;

};

struct RogueFnArgList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassFnArg : RogueObject
{
  // PROPERTIES
  RogueString* name;
  RogueClassCmd* value;
  RogueClassType* _type;

};

struct RogueClassCmdSelect : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* local_info;
  RogueClassCmd* expression;
  RogueCmdSelectCaseList* cases;
  RogueClassType* _value_type;

};

struct RogueCmdSelectCaseList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdSelectCase : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueCmdList* conditions;
  RogueClassCmd* value;

};

struct RogueClassCmdTypedLiteralList : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueString* list_type_name;
  RogueClassCmdArgs* elements;

};

struct RogueTableEntry_of_String_ParseReaderList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_ParseReaderTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassParseReader* value;
  RogueClassString_ParseReaderTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassFileReader : RogueObject
{
  // PROPERTIES
  FILE* fp;
  RogueInt32 position;
  RogueString* filepath;
  RogueInt32 count;
  RogueInt32 buffer_position;
  RogueByteList* buffer;

};

struct RogueClassFileWriter : RogueObject
{
  // PROPERTIES
  FILE* fp;
  RogueInt32 position;
  RogueString* filepath;
  RogueLogical error;
  RogueByteList* buffer;

};

struct RogueClassTokenListRebuilder : RogueObject
{
  // PROPERTIES
  RogueTokenList* list;
  RogueInt32 read_index;
  RogueInt32 write_index;

};

struct RogueTableEntry_of_String_TokenTypeList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_TokenTypeTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassTokenType* value;
  RogueClassString_TokenTypeTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_TypeSpecializerList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_TypeSpecializerTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassTypeSpecializer* value;
  RogueClassString_TypeSpecializerTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_CmdLabelList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_CmdLabelTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassCmdLabel* value;
  RogueClassString_CmdLabelTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassCmdCreateArray : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _array_type;
  RogueClassCmd* count_cmd;

};

struct RogueClassCmdCreateObject : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;

};

struct RogueClassCmdReadGlobal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassProperty* global_info;

};

struct RogueClassCmdReadProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassProperty* property_info;

};

struct RogueClassCmdLogicalizeOptionalValue : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* value;
  RogueLogical positive;

};

struct RogueClassCmdWriteSingleton : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassType* _of_type;
  RogueClassCmd* new_value;

};

struct RogueClassCmdWriteLocal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassLocal* local_info;
  RogueClassCmd* new_value;

};

struct RogueClassCmdOpAssignGlobal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassTokenType* op;
  RogueClassCmd* new_value;
  RogueClassProperty* global_info;

};

struct RogueClassCmdOpAssignProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassTokenType* op;
  RogueClassCmd* new_value;
  RogueClassCmd* context;
  RogueClassProperty* property_info;

};

struct RogueCmdTaskControlSectionList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassCmdBlock : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;

};

struct RogueTableEntry_of_String_DefinitionList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_DefinitionTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassDefinition* value;
  RogueClassString_DefinitionTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueTableEntry_of_String_PropertyList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_PropertyTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassProperty* value;
  RogueClassString_PropertyTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassString_CmdTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_CmdList* bins;
  RogueStringList* keys;

};

struct RogueClassCmdCallStaticMethod : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassMethod* method_info;
  RogueClassCmdArgs* args;

};

struct RogueTableEntry_of_String_StringList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_StringTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueString* value;
  RogueClassString_StringTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassDirectiveTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassStructuralDirectiveTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassEOLTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassStructureTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassNativeCodeTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassOpWithAssignTokenType : RogueObject
{
  // PROPERTIES
  RogueString* name;

};

struct RogueClassString_TokenListTable : RogueObject
{
  // PROPERTIES
  RogueInt32 bin_mask;
  RogueTableEntry_of_String_TokenListList* bins;
  RogueStringList* keys;

};

struct RogueClassPreprocessorTokenReader : RogueObject
{
  // PROPERTIES
  RogueTokenList* tokens;
  RogueTokenList* queue;
  RogueInt32 position;
  RogueInt32 count;

};

struct RogueClassCmdSwitch : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmdStatementList* statements;
  RogueInt32 _control_type;
  RogueLogical contains_yield;
  RogueString* escape_label;
  RogueString* upkeep_label;
  RogueClassCmdTaskControlSection* task_escape_section;
  RogueClassCmdTaskControlSection* task_upkeep_section;
  RogueClassCmdControlStructure* cloned_command;
  RogueClassCmd* expression;
  RogueCmdWhichCaseList* cases;
  RogueClassCmdWhichCase* case_others;

};

struct RogueClassCmdReadArrayElement : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassType* _array_type;
  RogueClassCmd* index;

};

struct RogueClassCmdWriteArrayElement : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassType* _array_type;
  RogueClassCmd* index;
  RogueClassCmd* new_value;

};

struct RogueClassCmdConvertToPrimitiveType : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* operand;
  RogueClassType* _target_type;

};

struct RogueClassCmdSelectCaseListReader : RogueObject
{
  // PROPERTIES
  RogueInt32 position;
  RogueCmdSelectCaseList* list;
  RogueInt32 limit;
  RogueLogical is_limited;

};

struct RogueClassCmdSelectCaseReader : RogueObject
{
};

struct RogueClassCmdAdjustGlobal : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassProperty* global_info;
  RogueInt32 delta;

};

struct RogueClassCmdAdjustProperty : RogueObject
{
  // PROPERTIES
  RogueClassToken* t;
  RogueClassCmd* context;
  RogueClassProperty* property_info;
  RogueInt32 delta;

};

struct RogueTableEntry_of_String_CmdList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_CmdTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueClassCmd* value;
  RogueClassString_CmdTableEntry* next_entry;
  RogueInt32 hash;

};

struct RogueClassNativeCodeToken : RogueObject
{
  // PROPERTIES
  RogueClassTokenType* _type;
  RogueString* filepath;
  RogueInt32 line;
  RogueInt32 column;
  RogueString* value;

};

struct RogueTableEntry_of_String_TokenListList : RogueObject
{
  // PROPERTIES
  RogueArray* data;
  RogueInt32 count;

};

struct RogueClassString_TokenListTableEntry : RogueObject
{
  // PROPERTIES
  RogueString* key;
  RogueTokenList* value;
  RogueClassString_TokenListTableEntry* next_entry;
  RogueInt32 hash;

};


extern RogueType* RogueTypeReal64;
extern RogueType* RogueTypeInt64;
extern RogueType* RogueTypeInt32;
extern RogueType* RogueTypeStringBuilder;
extern RogueType* RogueTypeObject;
extern RogueType* RogueTypeByte;
extern RogueType* RogueTypeCharacter;
extern RogueType* RogueTypeLogical;
extern RogueType* RogueTypeString;
extern RogueType* RogueTypeCharacterList;
extern RogueType* RogueTypeGenericList;
extern RogueType* RogueTypeArray;
extern RogueType* RogueTypeStringReader;
extern RogueType* RogueTypeCharacterReader;
extern RogueType* RogueTypeStringList;
extern RogueType* RogueTypeGlobal;
extern RogueType* RogueTypePrintWriterAspect;
extern RogueType* RogueTypeConsole;
extern RogueType* RogueTypeCharacterWriter;
extern RogueType* RogueTypeRogueC;
extern RogueType* RogueTypeException;
extern RogueType* RogueTypeStackTrace;
extern RogueType* RogueTypeReader_of_String;
extern RogueType* RogueTypeMath;
extern RogueType* RogueTypeProgram;
extern RogueType* RogueTypeTemplateList;
extern RogueType* RogueTypeTemplate;
extern RogueType* RogueTypeString_TemplateTable;
extern RogueType* RogueTypeString_AugmentListTable;
extern RogueType* RogueTypeRequisiteItemList;
extern RogueType* RogueTypeRequisiteItem;
extern RogueType* RogueTypeMethod;
extern RogueType* RogueTypePropertyList;
extern RogueType* RogueTypeProperty;
extern RogueType* RogueTypeString_MethodListTable;
extern RogueType* RogueTypeTypeList;
extern RogueType* RogueTypeType;
extern RogueType* RogueTypeString_TypeTable;
extern RogueType* RogueTypeString_Int32Table;
extern RogueType* RogueTypeString_StringListTable;
extern RogueType* RogueTypeString_Int32TableEntry;
extern RogueType* RogueTypeInt32_TemplateTable;
extern RogueType* RogueTypeToken;
extern RogueType* RogueTypeError;
extern RogueType* RogueTypeRogueError;
extern RogueType* RogueTypeAttributes;
extern RogueType* RogueTypeCmd;
extern RogueType* RogueTypeCmdReturn;
extern RogueType* RogueTypeCmdStatement;
extern RogueType* RogueTypeCmdStatementList;
extern RogueType* RogueTypeCmdList;
extern RogueType* RogueTypeTokenType;
extern RogueType* RogueTypeCmdLabel;
extern RogueType* RogueTypeScope;
extern RogueType* RogueTypeMethodList;
extern RogueType* RogueTypeFunction_Type_RETURNSLogical;
extern RogueType* RogueTypeFunction_197;
extern RogueType* RogueTypeFunction_198;
extern RogueType* RogueTypeCPPWriter;
extern RogueType* RogueTypeString_MethodTable;
extern RogueType* RogueTypeLocalList;
extern RogueType* RogueTypeLocal;
extern RogueType* RogueTypeInt32List;
extern RogueType* RogueTypeByteList;
extern RogueType* RogueTypeSystem;
extern RogueType* RogueTypeString_LogicalTable;
extern RogueType* RogueTypeParserList;
extern RogueType* RogueTypeParser;
extern RogueType* RogueTypeString_ParseReaderTable;
extern RogueType* RogueTypeFile;
extern RogueType* RogueTypeTokenList;
extern RogueType* RogueTypeLineReader;
extern RogueType* RogueTypeTypeParameterList;
extern RogueType* RogueTypeTypeParameter;
extern RogueType* RogueTypeAugmentList;
extern RogueType* RogueTypeAugment;
extern RogueType* RogueTypeString_TokenTypeTable;
extern RogueType* RogueTypeLiteralCharacterToken;
extern RogueType* RogueTypeLiteralInt64Token;
extern RogueType* RogueTypeLiteralInt32Token;
extern RogueType* RogueTypeLiteralReal64Token;
extern RogueType* RogueTypeStringDataToken;
extern RogueType* RogueTypeString_TypeSpecializerTable;
extern RogueType* RogueTypeTypeSpecializer;
extern RogueType* RogueTypeTypeSpecializerList;
extern RogueType* RogueTypeTableEntry_of_String_TemplateList;
extern RogueType* RogueTypeString_TemplateTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_AugmentListList;
extern RogueType* RogueTypeString_AugmentListTableEntry;
extern RogueType* RogueTypeCmdLabelList;
extern RogueType* RogueTypeString_CmdLabelTable;
extern RogueType* RogueTypeCloneArgs;
extern RogueType* RogueTypeCloneMethodArgs;
extern RogueType* RogueTypeCmdAccess;
extern RogueType* RogueTypeCmdArgs;
extern RogueType* RogueTypeCmdFlagArgList;
extern RogueType* RogueTypeCmdFlagArg;
extern RogueType* RogueTypeCmdAssign;
extern RogueType* RogueTypeCmdControlStructureList;
extern RogueType* RogueTypeCmdControlStructure;
extern RogueType* RogueTypeCmdLiteralThis;
extern RogueType* RogueTypeCmdThisContext;
extern RogueType* RogueTypeCmdGenericLoop;
extern RogueType* RogueTypeCmdLiteralInt32;
extern RogueType* RogueTypeCmdLiteral;
extern RogueType* RogueTypeCmdCompareNE;
extern RogueType* RogueTypeCmdComparison;
extern RogueType* RogueTypeCmdBinary;
extern RogueType* RogueTypeTaskArgs;
extern RogueType* RogueTypeCmdTaskControl;
extern RogueType* RogueTypeCmdTaskControlSection;
extern RogueType* RogueTypeTableEntry_of_String_MethodListList;
extern RogueType* RogueTypeString_MethodListTableEntry;
extern RogueType* RogueTypeDefinitionList;
extern RogueType* RogueTypeDefinition;
extern RogueType* RogueTypeString_DefinitionTable;
extern RogueType* RogueTypeNativePropertyList;
extern RogueType* RogueTypeNativeProperty;
extern RogueType* RogueTypeString_PropertyTable;
extern RogueType* RogueTypeCmdLiteralNull;
extern RogueType* RogueTypeCmdCreateCompound;
extern RogueType* RogueTypeCmdLiteralLogical;
extern RogueType* RogueTypeCmdLiteralString;
extern RogueType* RogueTypeCmdWriteGlobal;
extern RogueType* RogueTypeCmdWriteProperty;
extern RogueType* RogueTypeTableEntry_of_String_TypeList;
extern RogueType* RogueTypeString_TypeTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_Int32List;
extern RogueType* RogueTypeTableEntry_of_String_StringListList;
extern RogueType* RogueTypeString_StringListTableEntry;
extern RogueType* RogueTypeTableEntry_of_Int32_TemplateList;
extern RogueType* RogueTypeInt32_TemplateTableEntry;
extern RogueType* RogueTypeCmdCastToType;
extern RogueType* RogueTypeCmdTypeOperator;
extern RogueType* RogueTypeCmdLogicalize;
extern RogueType* RogueTypeCmdUnary;
extern RogueType* RogueTypeCmdCreateOptionalValue;
extern RogueType* RogueTypeCmdReadSingleton;
extern RogueType* RogueTypeCmdCallInlineNativeRoutine;
extern RogueType* RogueTypeCmdCallInlineNative;
extern RogueType* RogueTypeCmdCall;
extern RogueType* RogueTypeCmdCallNativeRoutine;
extern RogueType* RogueTypeMacroArgs;
extern RogueType* RogueTypeCmdInlineNative;
extern RogueType* RogueTypeCmdCallRoutine;
extern RogueType* RogueTypeCmdReadArrayCount;
extern RogueType* RogueTypeCmdCallInlineNativeMethod;
extern RogueType* RogueTypeCmdCallNativeMethod;
extern RogueType* RogueTypeCmdCallAspectMethod;
extern RogueType* RogueTypeCmdCallDynamicMethod;
extern RogueType* RogueTypeCmdCallMethod;
extern RogueType* RogueTypeCandidateMethods;
extern RogueType* RogueTypeCmdCreateFunction;
extern RogueType* RogueTypeFnParamList;
extern RogueType* RogueTypeFnParam;
extern RogueType* RogueTypeTableEntry_of_String_MethodList;
extern RogueType* RogueTypeString_MethodTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_LogicalList;
extern RogueType* RogueTypeString_LogicalTableEntry;
extern RogueType* RogueTypeTokenReader;
extern RogueType* RogueTypeString_StringTable;
extern RogueType* RogueTypeTokenizer;
extern RogueType* RogueTypeParseReader;
extern RogueType* RogueTypePreprocessor;
extern RogueType* RogueTypeEOLToken;
extern RogueType* RogueTypeCmdAdd;
extern RogueType* RogueTypeCmdNativeHeader;
extern RogueType* RogueTypeCmdNativeCode;
extern RogueType* RogueTypeCmdIf;
extern RogueType* RogueTypeCmdWhich;
extern RogueType* RogueTypeCmdContingent;
extern RogueType* RogueTypeCmdTry;
extern RogueType* RogueTypeCmdAwait;
extern RogueType* RogueTypeCmdYield;
extern RogueType* RogueTypeCmdThrow;
extern RogueType* RogueTypeCmdFormattedString;
extern RogueType* RogueTypeCmdTrace;
extern RogueType* RogueTypeCmdEscape;
extern RogueType* RogueTypeCmdNextIteration;
extern RogueType* RogueTypeCmdNecessary;
extern RogueType* RogueTypeCmdSufficient;
extern RogueType* RogueTypeCmdAdjust;
extern RogueType* RogueTypeCmdOpWithAssign;
extern RogueType* RogueTypeCmdWhichCaseList;
extern RogueType* RogueTypeCmdWhichCase;
extern RogueType* RogueTypeCmdCatchList;
extern RogueType* RogueTypeCmdCatch;
extern RogueType* RogueTypeCmdLocalDeclaration;
extern RogueType* RogueTypeCmdAdjustLocal;
extern RogueType* RogueTypeCmdReadLocal;
extern RogueType* RogueTypeCmdCompareLE;
extern RogueType* RogueTypeCmdRange;
extern RogueType* RogueTypeCmdLocalOpWithAssign;
extern RogueType* RogueTypeCmdResolvedOpWithAssign;
extern RogueType* RogueTypeCmdRangeUpTo;
extern RogueType* RogueTypeCmdCompareGE;
extern RogueType* RogueTypeCmdForEach;
extern RogueType* RogueTypeCmdRangeDownTo;
extern RogueType* RogueTypeCmdLogicalXor;
extern RogueType* RogueTypeCmdBinaryLogical;
extern RogueType* RogueTypeCmdLogicalOr;
extern RogueType* RogueTypeCmdLogicalAnd;
extern RogueType* RogueTypeCmdCompareEQ;
extern RogueType* RogueTypeCmdCompareIs;
extern RogueType* RogueTypeCmdCompareIsNot;
extern RogueType* RogueTypeCmdCompareLT;
extern RogueType* RogueTypeCmdCompareGT;
extern RogueType* RogueTypeCmdInstanceOf;
extern RogueType* RogueTypeCmdLogicalNot;
extern RogueType* RogueTypeCmdBitwiseXor;
extern RogueType* RogueTypeCmdBitwiseOp;
extern RogueType* RogueTypeCmdBitwiseOr;
extern RogueType* RogueTypeCmdBitwiseAnd;
extern RogueType* RogueTypeCmdBitwiseShiftLeft;
extern RogueType* RogueTypeCmdBitwiseShiftRight;
extern RogueType* RogueTypeCmdBitwiseShiftRightX;
extern RogueType* RogueTypeCmdSubtract;
extern RogueType* RogueTypeCmdMultiply;
extern RogueType* RogueTypeCmdDivide;
extern RogueType* RogueTypeCmdMod;
extern RogueType* RogueTypeCmdPower;
extern RogueType* RogueTypeCmdNegate;
extern RogueType* RogueTypeCmdBitwiseNot;
extern RogueType* RogueTypeCmdGetOptionalValue;
extern RogueType* RogueTypeCmdElementAccess;
extern RogueType* RogueTypeCmdListConvert;
extern RogueType* RogueTypeCmdConvertToType;
extern RogueType* RogueTypeCmdCreateCallback;
extern RogueType* RogueTypeCmdAs;
extern RogueType* RogueTypeCmdDefaultValue;
extern RogueType* RogueTypeCmdLiteralReal64;
extern RogueType* RogueTypeCmdLiteralInt64;
extern RogueType* RogueTypeCmdLiteralCharacter;
extern RogueType* RogueTypeCmdCreateList;
extern RogueType* RogueTypeCmdCallPriorMethod;
extern RogueType* RogueTypeFnArgList;
extern RogueType* RogueTypeFnArg;
extern RogueType* RogueTypeCmdSelect;
extern RogueType* RogueTypeCmdSelectCaseList;
extern RogueType* RogueTypeCmdSelectCase;
extern RogueType* RogueTypeCmdTypedLiteralList;
extern RogueType* RogueTypeTableEntry_of_String_ParseReaderList;
extern RogueType* RogueTypeString_ParseReaderTableEntry;
extern RogueType* RogueTypeFileReader;
extern RogueType* RogueTypeFileWriter;
extern RogueType* RogueTypeTokenListRebuilder;
extern RogueType* RogueTypeTableEntry_of_String_TokenTypeList;
extern RogueType* RogueTypeString_TokenTypeTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_TypeSpecializerList;
extern RogueType* RogueTypeString_TypeSpecializerTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_CmdLabelList;
extern RogueType* RogueTypeString_CmdLabelTableEntry;
extern RogueType* RogueTypeCmdCreateArray;
extern RogueType* RogueTypeCmdCreateObject;
extern RogueType* RogueTypeCmdReadGlobal;
extern RogueType* RogueTypeCmdReadProperty;
extern RogueType* RogueTypeCmdLogicalizeOptionalValue;
extern RogueType* RogueTypeCmdWriteSingleton;
extern RogueType* RogueTypeCmdWriteLocal;
extern RogueType* RogueTypeCmdOpAssignGlobal;
extern RogueType* RogueTypeCmdOpAssignProperty;
extern RogueType* RogueTypeCmdTaskControlSectionList;
extern RogueType* RogueTypeCmdBlock;
extern RogueType* RogueTypeTableEntry_of_String_DefinitionList;
extern RogueType* RogueTypeString_DefinitionTableEntry;
extern RogueType* RogueTypeTableEntry_of_String_PropertyList;
extern RogueType* RogueTypeString_PropertyTableEntry;
extern RogueType* RogueTypeString_CmdTable;
extern RogueType* RogueTypeCmdCallStaticMethod;
extern RogueType* RogueTypeTableEntry_of_String_StringList;
extern RogueType* RogueTypeString_StringTableEntry;
extern RogueType* RogueTypeDirectiveTokenType;
extern RogueType* RogueTypeStructuralDirectiveTokenType;
extern RogueType* RogueTypeEOLTokenType;
extern RogueType* RogueTypeStructureTokenType;
extern RogueType* RogueTypeNativeCodeTokenType;
extern RogueType* RogueTypeOpWithAssignTokenType;
extern RogueType* RogueTypeString_TokenListTable;
extern RogueType* RogueTypePreprocessorTokenReader;
extern RogueType* RogueTypeCmdSwitch;
extern RogueType* RogueTypeCmdReadArrayElement;
extern RogueType* RogueTypeCmdWriteArrayElement;
extern RogueType* RogueTypeCmdConvertToPrimitiveType;
extern RogueType* RogueTypeCmdSelectCaseListReader;
extern RogueType* RogueTypeCmdSelectCaseReader;
extern RogueType* RogueTypeCmdAdjustGlobal;
extern RogueType* RogueTypeCmdAdjustProperty;
extern RogueType* RogueTypeTableEntry_of_String_CmdList;
extern RogueType* RogueTypeString_CmdTableEntry;
extern RogueType* RogueTypeNativeCodeToken;
extern RogueType* RogueTypeTableEntry_of_String_TokenListList;
extern RogueType* RogueTypeString_TokenListTableEntry;
extern RogueType* RogueTypeOptionalInt32;
extern RogueType* RogueTypeOptionalCharacter;

void RogueStringBuilder__init_class();
RogueInt32 RogueMath__max__Int32_Int32( RogueInt32 a_0, RogueInt32 b_1 );
RogueInt32 RogueMath__mod__Int32_Int32( RogueInt32 a_0, RogueInt32 b_1 );
RogueInt64 RogueMath__mod__Int64_Int64( RogueInt64 a_0, RogueInt64 b_1 );
RogueReal64 RogueMath__mod__Real64_Real64( RogueReal64 a_0, RogueReal64 b_1 );
RogueInt32 RogueMath__shift_right__Int32_Int32( RogueInt32 value_0, RogueInt32 bits_1 );
void RogueCmdStatementList__init_class();
void RogueTokenType__init_class();
void RogueSystem__exit__Int32( RogueInt32 result_code_0 );
void RogueSystem__init_class();
RogueString* RogueFile__absolute_filepath__String( RogueString* filepath_0 );
RogueLogical RogueFile__exists__String( RogueString* filepath_0 );
RogueString* RogueFile__filename__String( RogueString* filepath_0 );
RogueLogical RogueFile__is_folder__String( RogueString* filepath_0 );
RogueString* RogueFile__load_as_string__String( RogueString* filepath_0 );
RogueString* RogueFile__path__String( RogueString* filepath_0 );
RogueClassFileReader* RogueFile__reader__String( RogueString* filepath_0 );
RogueLogical RogueFile__save__String_String( RogueString* filepath_0, RogueString* data_1 );
RogueInt32 RogueFile__size__String( RogueString* filepath_0 );
RogueClassFileWriter* RogueFile__writer__String( RogueString* filepath_0 );
void RoguePreprocessor__init_class();

RogueLogical RogueReal64__is_infinite( RogueReal64 THIS );
RogueLogical RogueReal64__is_not_a_number( RogueReal64 THIS );
RogueInt32 RogueInt32__hash_code( RogueInt32 THIS );
RogueString* RogueInt32__to_String( RogueInt32 THIS );
RogueString* RogueStringBuilder__to_String( RogueStringBuilder* THIS );
RogueString* RogueStringBuilder__type_name( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__init( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__init__Int32( RogueStringBuilder* THIS, RogueInt32 initial_capacity_0 );
RogueStringBuilder* RogueStringBuilder__clear( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__insert_spaces__Int32_Int32_Character( RogueStringBuilder* THIS, RogueInt32 index_0, RogueInt32 spaces_1, RogueCharacter character_2 );
RogueLogical RogueStringBuilder__needs_indent( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__print__Character( RogueStringBuilder* THIS, RogueCharacter value_0 );
RogueStringBuilder* RogueStringBuilder__print__Int32( RogueStringBuilder* THIS, RogueInt32 value_0 );
RogueStringBuilder* RogueStringBuilder__print__Logical( RogueStringBuilder* THIS, RogueLogical value_0 );
RogueStringBuilder* RogueStringBuilder__print__Int64( RogueStringBuilder* THIS, RogueInt64 value_0 );
RogueStringBuilder* RogueStringBuilder__print__Object( RogueStringBuilder* THIS, RogueObject* value_0 );
RogueStringBuilder* RogueStringBuilder__print__Real64( RogueStringBuilder* THIS, RogueReal64 value_0 );
RogueStringBuilder* RogueStringBuilder__print__String( RogueStringBuilder* THIS, RogueString* value_0 );
void RogueStringBuilder__print_indent( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__print_to_work_bytes__Real64_Int32( RogueStringBuilder* THIS, RogueReal64 value_0, RogueInt32 decimal_places_1 );
void RogueStringBuilder__print_work_bytes( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__println( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__println__Int32( RogueStringBuilder* THIS, RogueInt32 value_0 );
RogueStringBuilder* RogueStringBuilder__println__String( RogueStringBuilder* THIS, RogueString* value_0 );
RogueStringBuilder* RogueStringBuilder__reserve__Int32( RogueStringBuilder* THIS, RogueInt32 additional_count_0 );
void RogueStringBuilder__round_off_work_bytes( RogueStringBuilder* THIS );
RogueReal64 RogueStringBuilder__scan_work_bytes( RogueStringBuilder* THIS );
RogueStringBuilder* RogueStringBuilder__init_object( RogueStringBuilder* THIS );
RogueLogical RogueObject__operatorEQUALSEQUALS__Object( RogueObject* THIS, RogueObject* other_0 );
RogueString* RogueObject__to_String( RogueObject* THIS );
RogueString* RogueObject__type_name( RogueObject* THIS );
RogueString* RogueByte__to_String( RogueByte THIS );
RogueLogical RogueCharacter__is_alphanumeric( RogueCharacter THIS );
RogueLogical RogueCharacter__is_identifier( RogueCharacter THIS );
RogueLogical RogueCharacter__is_letter( RogueCharacter THIS );
RogueLogical RogueCharacter__is_number__Int32( RogueCharacter THIS, RogueInt32 base_0 );
RogueString* RogueCharacter__to_String( RogueCharacter THIS );
RogueInt32 RogueCharacter__to_number__Int32( RogueCharacter THIS, RogueInt32 base_0 );
RogueString* RogueString__after__Int32( RogueString* THIS, RogueInt32 index_0 );
RogueString* RogueString__before__Int32( RogueString* THIS, RogueInt32 index_0 );
RogueString* RogueString__after_any__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueString* RogueString__after_first__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueString* RogueString__after_first__String( RogueString* THIS, RogueString* st_0 );
RogueString* RogueString__after_last__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueString* RogueString__before_first__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueString* RogueString__before_first__String( RogueString* THIS, RogueString* st_0 );
RogueString* RogueString__before_last__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueString* RogueString__before_last__String( RogueString* THIS, RogueString* st_0 );
RogueLogical RogueString__begins_with__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueLogical RogueString__begins_with__String( RogueString* THIS, RogueString* other_0 );
RogueLogical RogueString__contains__String( RogueString* THIS, RogueString* substring_0 );
RogueLogical RogueString__contains_at__String_Int32( RogueString* THIS, RogueString* substring_0, RogueInt32 at_index_1 );
RogueString* RogueString__decode_utf8( RogueString* THIS );
RogueLogical RogueString__ends_with__String( RogueString* THIS, RogueString* other_0 );
RogueString* RogueString__from__Int32( RogueString* THIS, RogueInt32 i1_0 );
RogueString* RogueString__from__Int32_Int32( RogueString* THIS, RogueInt32 i1_0, RogueInt32 i2_1 );
RogueString* RogueString__from_first__Character( RogueString* THIS, RogueCharacter ch_0 );
RogueCharacter RogueString__last( RogueString* THIS );
RogueString* RogueString__left_justified__Int32( RogueString* THIS, RogueInt32 spaces_0 );
RogueString* RogueString__leftmost__Int32( RogueString* THIS, RogueInt32 n_0 );
RogueOptionalInt32 RogueString__locate__Character_OptionalInt32( RogueString* THIS, RogueCharacter ch_0, RogueOptionalInt32 optional_i1_1 );
RogueOptionalInt32 RogueString__locate__String_OptionalInt32( RogueString* THIS, RogueString* other_0, RogueOptionalInt32 optional_i1_1 );
RogueOptionalInt32 RogueString__locate_last__Character_OptionalInt32( RogueString* THIS, RogueCharacter ch_0, RogueOptionalInt32 starting_index_1 );
RogueOptionalInt32 RogueString__locate_last__String_OptionalInt32( RogueString* THIS, RogueString* other_0, RogueOptionalInt32 starting_index_1 );
RogueString* RogueString__operatorPLUS__Character( RogueString* THIS, RogueCharacter value_0 );
RogueString* RogueString__operatorPLUS__Int32( RogueString* THIS, RogueInt32 value_0 );
RogueLogical RogueString__operatorEQUALSEQUALS__String( RogueString* THIS, RogueString* value_0 );
RogueInt32 RogueString__operatorLTGT__String( RogueString* THIS, RogueString* other_0 );
RogueString* RogueString__operatorPLUS__Int64( RogueString* THIS, RogueInt64 value_0 );
RogueString* RogueString__operatorPLUS__Object( RogueString* THIS, RogueObject* value_0 );
RogueString* RogueString__operatorPLUS__Real64( RogueString* THIS, RogueReal64 value_0 );
RogueString* RogueString__operatorPLUS__String( RogueString* THIS, RogueString* value_0 );
RogueString* RogueString__pluralize__Int32( RogueString* THIS, RogueInt32 quantity_0 );
RogueClassStringReader* RogueString__reader( RogueString* THIS );
RogueString* RogueString__replacing__Character_Character( RogueString* THIS, RogueCharacter look_for_0, RogueCharacter replace_with_1 );
RogueString* RogueString__replacing__String_String( RogueString* THIS, RogueString* look_for_0, RogueString* replace_with_1 );
RogueString* RogueString__rightmost__Int32( RogueString* THIS, RogueInt32 n_0 );
RogueStringList* RogueString__split__Character( RogueString* THIS, RogueCharacter separator_0 );
RogueString* RogueString__to_lowercase( RogueString* THIS );
RogueString* RogueString__to_utf8( RogueString* THIS );
RogueStringList* RogueString__word_wrapped__Int32( RogueString* THIS, RogueInt32 width_0 );
RogueStringBuilder* RogueString__word_wrapped__Int32_StringBuilder( RogueString* THIS, RogueInt32 width_0, RogueStringBuilder* buffer_1 );
RogueString* RogueCharacterList__to_String( RogueCharacterList* THIS );
RogueString* RogueCharacterList__type_name( RogueCharacterList* THIS );
RogueCharacterList* RogueCharacterList__init_object( RogueCharacterList* THIS );
RogueCharacterList* RogueCharacterList__init__Int32( RogueCharacterList* THIS, RogueInt32 initial_capacity_0 );
RogueCharacterList* RogueCharacterList__add__Character( RogueCharacterList* THIS, RogueCharacter value_0 );
RogueInt32 RogueCharacterList__capacity( RogueCharacterList* THIS );
RogueCharacterList* RogueCharacterList__clear( RogueCharacterList* THIS );
RogueCharacterList* RogueCharacterList__ensure_capacity__Int32( RogueCharacterList* THIS, RogueInt32 desired_capacity_0 );
RogueCharacterList* RogueCharacterList__expand_to_count__Int32( RogueCharacterList* THIS, RogueInt32 minimum_count_0 );
RogueCharacterList* RogueCharacterList__expand_to_include__Int32( RogueCharacterList* THIS, RogueInt32 index_0 );
RogueCharacterList* RogueCharacterList__reserve__Int32( RogueCharacterList* THIS, RogueInt32 additional_count_0 );
RogueCharacterList* RogueCharacterList__shift__Int32_OptionalInt32_Int32_OptionalCharacter( RogueCharacterList* THIS, RogueInt32 i1_0, RogueOptionalInt32 element_count_1, RogueInt32 delta_2, RogueOptionalCharacter fill_3 );
RogueString* RogueGenericList__type_name( RogueClassGenericList* THIS );
RogueClassGenericList* RogueGenericList__init_object( RogueClassGenericList* THIS );
RogueString* RogueCharacterArray__type_name( RogueArray* THIS );
RogueString* RogueNativeArray__type_name( RogueArray* THIS );
RogueString* RogueStringReader__type_name( RogueClassStringReader* THIS );
RogueLogical RogueStringReader__has_another( RogueClassStringReader* THIS );
RogueCharacter RogueStringReader__read( RogueClassStringReader* THIS );
RogueClassStringReader* RogueStringReader__init__String( RogueClassStringReader* THIS, RogueString* _auto_24_0 );
RogueLogical RogueStringReader__has_another__Int32( RogueClassStringReader* THIS, RogueInt32 n_0 );
RogueClassStringReader* RogueStringReader__init_object( RogueClassStringReader* THIS );
RogueLogical RogueCharacterReader__has_another( RogueObject* THIS );
RogueCharacter RogueCharacterReader__read( RogueObject* THIS );
RogueString* RogueStringList__to_String( RogueStringList* THIS );
RogueString* RogueStringList__type_name( RogueStringList* THIS );
RogueStringList* RogueStringList__init_object( RogueStringList* THIS );
RogueStringList* RogueStringList__init( RogueStringList* THIS );
RogueStringList* RogueStringList__init__Int32( RogueStringList* THIS, RogueInt32 initial_capacity_0 );
RogueStringList* RogueStringList__add__String( RogueStringList* THIS, RogueString* value_0 );
RogueStringList* RogueStringList__add__StringList( RogueStringList* THIS, RogueStringList* other_0 );
RogueInt32 RogueStringList__capacity( RogueStringList* THIS );
RogueStringList* RogueStringList__clear( RogueStringList* THIS );
RogueOptionalInt32 RogueStringList__locate__String( RogueStringList* THIS, RogueString* value_0 );
RogueStringList* RogueStringList__reserve__Int32( RogueStringList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueStringList__joined__String( RogueStringList* THIS, RogueString* separator_0 );
RogueString* RogueStringArray__type_name( RogueArray* THIS );
RogueString* RogueGlobal__type_name( RogueClassGlobal* THIS );
RogueClassPrintWriterAspect* RogueGlobal__flush( RogueClassGlobal* THIS );
RogueClassPrintWriterAspect* RogueGlobal__print__Object( RogueClassGlobal* THIS, RogueObject* value_0 );
RogueClassPrintWriterAspect* RogueGlobal__print__String( RogueClassGlobal* THIS, RogueString* value_0 );
RogueClassPrintWriterAspect* RogueGlobal__println( RogueClassGlobal* THIS );
RogueClassPrintWriterAspect* RogueGlobal__println__Object( RogueClassGlobal* THIS, RogueObject* value_0 );
RogueClassPrintWriterAspect* RogueGlobal__println__String( RogueClassGlobal* THIS, RogueString* value_0 );
RogueClassGlobal* RogueGlobal__write__CharacterList( RogueClassGlobal* THIS, RogueCharacterList* characters_0 );
void RogueGlobal__on_launch( RogueClassGlobal* THIS );
RogueClassGlobal* RogueGlobal__init_object( RogueClassGlobal* THIS );
RogueClassPrintWriterAspect* RoguePrintWriterAspect__println( RogueObject* THIS );
RogueString* RogueConsole__type_name( RogueClassConsole* THIS );
void RogueConsole__print__StringBuilder( RogueClassConsole* THIS, RogueStringBuilder* value_0 );
RogueClassConsole* RogueConsole__init_object( RogueClassConsole* THIS );
RogueString* RogueRogueC__type_name( RogueClassRogueC* THIS );
void RogueRogueC__launch( RogueClassRogueC* THIS );
void RogueRogueC__write_output( RogueClassRogueC* THIS );
void RogueRogueC__include_source__String( RogueClassRogueC* THIS, RogueString* filepath_0 );
void RogueRogueC__include_source__Token_String( RogueClassRogueC* THIS, RogueClassToken* t_0, RogueString* filepath_1 );
void RogueRogueC__include_native__Token_String_String( RogueClassRogueC* THIS, RogueClassToken* t_0, RogueString* filepath_1, RogueString* native_type_2 );
void RogueRogueC__process_command_line_arguments( RogueClassRogueC* THIS );
void RogueRogueC__write_cpp( RogueClassRogueC* THIS );
RogueClassRogueC* RogueRogueC__init_object( RogueClassRogueC* THIS );
RogueString* RogueException__to_String( RogueClassException* THIS );
RogueString* RogueException__type_name( RogueClassException* THIS );
RogueClassException* RogueException__init_object( RogueClassException* THIS );
RogueString* RogueStackTrace__to_String( RogueClassStackTrace* THIS );
RogueString* RogueStackTrace__type_name( RogueClassStackTrace* THIS );
RogueClassStackTrace* RogueStackTrace__init__Int32( RogueClassStackTrace* THIS, RogueInt32 omit_count_0 );
void RogueStackTrace__format( RogueClassStackTrace* THIS );
void RogueStackTrace__print( RogueClassStackTrace* THIS );
RogueStringBuilder* RogueStackTrace__print__StringBuilder( RogueClassStackTrace* THIS, RogueStringBuilder* buffer_0 );
RogueClassStackTrace* RogueStackTrace__init_object( RogueClassStackTrace* THIS );
RogueString* RogueMath__type_name( RogueClassMath* THIS );
RogueClassMath* RogueMath__init_object( RogueClassMath* THIS );
RogueString* RogueProgram__type_name( RogueClassProgram* THIS );
RogueInt32 RogueProgram__add_literal_string__String( RogueClassProgram* THIS, RogueString* value_0 );
void RogueProgram__add_module_id__String_String( RogueClassProgram* THIS, RogueString* module_name_0, RogueString* id_name_1 );
void RogueProgram__add_template__Template( RogueClassProgram* THIS, RogueClassTemplate* template_0 );
void RogueProgram__configure( RogueClassProgram* THIS );
RogueString* RogueProgram__create_unique_id( RogueClassProgram* THIS );
RogueInt32 RogueProgram__next_unique_integer( RogueClassProgram* THIS );
RogueClassTemplate* RogueProgram__find_template__String( RogueClassProgram* THIS, RogueString* name_0 );
RogueClassType* RogueProgram__find_type__String( RogueClassProgram* THIS, RogueString* name_0 );
RogueClassType* RogueProgram__get_type_reference__Token_String( RogueClassProgram* THIS, RogueClassToken* t_0, RogueString* name_1 );
RogueString* RogueProgram__get_callback_type_signature__TypeList( RogueClassProgram* THIS, RogueTypeList* parameter_types_0 );
RogueClassType* RogueProgram__get_callback_type_reference__Token_TypeList_Type( RogueClassProgram* THIS, RogueClassToken* t_0, RogueTypeList* parameter_types_1, RogueClassType* return_type_2 );
RogueLogical RogueProgram__is_type_defined__String( RogueClassProgram* THIS, RogueString* name_0 );
RogueClassType* RogueProgram__create_built_in_type__String_Int32( RogueClassProgram* THIS, RogueString* name_0, RogueInt32 attributes_1 );
void RogueProgram__resolve( RogueClassProgram* THIS );
void RogueProgram__reorder_compounds( RogueClassProgram* THIS );
void RogueProgram__collect_compound_dependencies__Type_TypeList( RogueClassProgram* THIS, RogueClassType* type_0, RogueTypeList* compounds_1 );
void RogueProgram__cull_unused_code( RogueClassProgram* THIS );
void RogueProgram__trace_overridden_methods( RogueClassProgram* THIS );
RogueString* RogueProgram__validate_cpp_name__String( RogueClassProgram* THIS, RogueString* name_0 );
void RogueProgram__write_cpp__String( RogueClassProgram* THIS, RogueString* filepath_0 );
void RogueProgram__print_property_trace_code__Type_CPPWriter( RogueClassProgram* THIS, RogueClassType* type_0, RogueClassCPPWriter* writer_1 );
RogueClassProgram* RogueProgram__init_object( RogueClassProgram* THIS );
RogueString* RogueTemplateList__to_String( RogueTemplateList* THIS );
RogueString* RogueTemplateList__type_name( RogueTemplateList* THIS );
RogueTemplateList* RogueTemplateList__init_object( RogueTemplateList* THIS );
RogueTemplateList* RogueTemplateList__init( RogueTemplateList* THIS );
RogueTemplateList* RogueTemplateList__init__Int32( RogueTemplateList* THIS, RogueInt32 initial_capacity_0 );
RogueTemplateList* RogueTemplateList__add__Template( RogueTemplateList* THIS, RogueClassTemplate* value_0 );
RogueInt32 RogueTemplateList__capacity( RogueTemplateList* THIS );
RogueTemplateList* RogueTemplateList__reserve__Int32( RogueTemplateList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueTemplate__type_name( RogueClassTemplate* THIS );
RogueClassTemplate* RogueTemplate__init__Token_String_Int32( RogueClassTemplate* THIS, RogueClassToken* _auto_99_0, RogueString* _auto_100_1, RogueInt32 attribute_flags_2 );
RogueClassTypeParameter* RogueTemplate__add_type_parameter__Token_String( RogueClassTemplate* THIS, RogueClassToken* p_t_0, RogueString* p_name_1 );
RogueInt32 Rogue_Template__element_type( RogueClassTemplate* THIS );
void RogueTemplate__instantiate__Type_Scope( RogueClassTemplate* THIS, RogueClassType* type_0, RogueClassScope* scope_1 );
void RogueTemplate__instantiate_list__Type_TokenList( RogueClassTemplate* THIS, RogueClassType* type_0, RogueTokenList* augmented_tokens_1 );
void RogueTemplate__instantiate_optional__Type_TokenList( RogueClassTemplate* THIS, RogueClassType* type_0, RogueTokenList* augmented_tokens_1 );
void RogueTemplate__instantiate_parameterized_type__Type_TokenList_Scope( RogueClassTemplate* THIS, RogueClassType* type_0, RogueTokenList* augmented_tokens_1, RogueClassScope* scope_2 );
void RogueTemplate__instantiate_standard_type__Type_TokenList( RogueClassTemplate* THIS, RogueClassType* type_0, RogueTokenList* augmented_tokens_1 );
RogueInt32 RogueTemplate__type_parameter_count( RogueClassTemplate* THIS );
RogueClassTemplate* RogueTemplate__init_object( RogueClassTemplate* THIS );
RogueString* RogueString_TemplateTable__to_String( RogueClassString_TemplateTable* THIS );
RogueString* RogueString_TemplateTable__type_name( RogueClassString_TemplateTable* THIS );
RogueClassString_TemplateTable* RogueString_TemplateTable__init( RogueClassString_TemplateTable* THIS );
RogueClassString_TemplateTable* RogueString_TemplateTable__init__Int32( RogueClassString_TemplateTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_TemplateTableEntry* RogueString_TemplateTable__find__String( RogueClassString_TemplateTable* THIS, RogueString* key_0 );
RogueClassTemplate* RogueString_TemplateTable__get__String( RogueClassString_TemplateTable* THIS, RogueString* key_0 );
RogueClassString_TemplateTable* RogueString_TemplateTable__set__String_Template( RogueClassString_TemplateTable* THIS, RogueString* key_0, RogueClassTemplate* value_1 );
RogueStringBuilder* RogueString_TemplateTable__print_to__StringBuilder( RogueClassString_TemplateTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_TemplateTable* RogueString_TemplateTable__init_object( RogueClassString_TemplateTable* THIS );
RogueString* RogueString_AugmentListTable__to_String( RogueClassString_AugmentListTable* THIS );
RogueString* RogueString_AugmentListTable__type_name( RogueClassString_AugmentListTable* THIS );
RogueClassString_AugmentListTable* RogueString_AugmentListTable__init( RogueClassString_AugmentListTable* THIS );
RogueClassString_AugmentListTable* RogueString_AugmentListTable__init__Int32( RogueClassString_AugmentListTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_AugmentListTableEntry* RogueString_AugmentListTable__find__String( RogueClassString_AugmentListTable* THIS, RogueString* key_0 );
RogueAugmentList* RogueString_AugmentListTable__get__String( RogueClassString_AugmentListTable* THIS, RogueString* key_0 );
RogueClassString_AugmentListTable* RogueString_AugmentListTable__set__String_AugmentList( RogueClassString_AugmentListTable* THIS, RogueString* key_0, RogueAugmentList* value_1 );
RogueStringBuilder* RogueString_AugmentListTable__print_to__StringBuilder( RogueClassString_AugmentListTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_AugmentListTable* RogueString_AugmentListTable__init_object( RogueClassString_AugmentListTable* THIS );
RogueString* RogueRequisiteItemList__to_String( RogueRequisiteItemList* THIS );
RogueString* RogueRequisiteItemList__type_name( RogueRequisiteItemList* THIS );
RogueRequisiteItemList* RogueRequisiteItemList__init_object( RogueRequisiteItemList* THIS );
RogueRequisiteItemList* RogueRequisiteItemList__init( RogueRequisiteItemList* THIS );
RogueRequisiteItemList* RogueRequisiteItemList__init__Int32( RogueRequisiteItemList* THIS, RogueInt32 initial_capacity_0 );
RogueRequisiteItemList* RogueRequisiteItemList__add__RequisiteItem( RogueRequisiteItemList* THIS, RogueClassRequisiteItem* value_0 );
RogueInt32 RogueRequisiteItemList__capacity( RogueRequisiteItemList* THIS );
RogueRequisiteItemList* RogueRequisiteItemList__reserve__Int32( RogueRequisiteItemList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueRequisiteItem__type_name( RogueClassRequisiteItem* THIS );
RogueClassRequisiteItem* RogueRequisiteItem__init__Token_Type_String( RogueClassRequisiteItem* THIS, RogueClassToken* _auto_105_0, RogueClassType* _auto_106_1, RogueString* _auto_107_2 );
RogueClassRequisiteItem* RogueRequisiteItem__init_object( RogueClassRequisiteItem* THIS );
RogueString* RogueMethod__to_String( RogueClassMethod* THIS );
RogueString* RogueMethod__type_name( RogueClassMethod* THIS );
RogueClassMethod* RogueMethod__init__Token_Type_String( RogueClassMethod* THIS, RogueClassToken* _auto_112_0, RogueClassType* _auto_113_1, RogueString* _auto_114_2 );
RogueClassMethod* RogueMethod__clone( RogueClassMethod* THIS );
RogueClassMethod* RogueMethod__incorporate__Type( RogueClassMethod* THIS, RogueClassType* into_type_0 );
RogueLogical RogueMethod__accepts_arg_count__Int32( RogueClassMethod* THIS, RogueInt32 n_0 );
RogueClassLocal* RogueMethod__add_local__Token_String_Type_Cmd( RogueClassMethod* THIS, RogueClassToken* v_t_0, RogueString* v_name_1, RogueClassType* v_type_2, RogueClassCmd* v_initial_value_3 );
RogueClassLocal* RogueMethod__add_parameter__Token_String_Type( RogueClassMethod* THIS, RogueClassToken* p_t_0, RogueString* p_name_1, RogueClassType* p_type_2 );
void RogueMethod__assign_signature( RogueClassMethod* THIS );
RogueClassCmdLabel* RogueMethod__begin_label__Token_String_Logical( RogueClassMethod* THIS, RogueClassToken* label_t_0, RogueString* label_name_1, RogueLogical consolidate_duplicates_2 );
RogueLogical RogueMethod__is_abstract( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_augment( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_dynamic( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_fallback( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_generated( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_incorporated( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_initializer( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_macro( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_native( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_overridden( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_requisite( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_global( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_task( RogueClassMethod* THIS );
RogueLogical RogueMethod__is_task_conversion( RogueClassMethod* THIS );
RogueLogical RogueMethod__omit_output( RogueClassMethod* THIS );
RogueClassMethod* RogueMethod__organize__Scope_Logical( RogueClassMethod* THIS, RogueClassScope* scope_0, RogueLogical add_to_lookup_1 );
void RogueMethod__resolve( RogueClassMethod* THIS );
void RogueMethod__convert_augment_to_standalone( RogueClassMethod* THIS );
void RogueMethod__convert_to_task( RogueClassMethod* THIS );
RogueClassMethod* RogueMethod__set_incorporated( RogueClassMethod* THIS );
RogueClassMethod* RogueMethod__set_type_context__Type( RogueClassMethod* THIS, RogueClassType* _auto_115_0 );
void RogueMethod__trace_used_code( RogueClassMethod* THIS );
void RogueMethod__assign_cpp_name( RogueClassMethod* THIS );
void RogueMethod__print_prototype__CPPWriter( RogueClassMethod* THIS, RogueClassCPPWriter* writer_0 );
void RogueMethod__print_signature__CPPWriter( RogueClassMethod* THIS, RogueClassCPPWriter* writer_0 );
void RogueMethod__print_definition__CPPWriter( RogueClassMethod* THIS, RogueClassCPPWriter* writer_0 );
RogueClassMethod* RogueMethod__init_object( RogueClassMethod* THIS );
RogueString* RoguePropertyList__to_String( RoguePropertyList* THIS );
RogueString* RoguePropertyList__type_name( RoguePropertyList* THIS );
RoguePropertyList* RoguePropertyList__init_object( RoguePropertyList* THIS );
RoguePropertyList* RoguePropertyList__init( RoguePropertyList* THIS );
RoguePropertyList* RoguePropertyList__init__Int32( RoguePropertyList* THIS, RogueInt32 initial_capacity_0 );
RoguePropertyList* RoguePropertyList__add__Property( RoguePropertyList* THIS, RogueClassProperty* value_0 );
RogueInt32 RoguePropertyList__capacity( RoguePropertyList* THIS );
RoguePropertyList* RoguePropertyList__clear( RoguePropertyList* THIS );
RogueOptionalInt32 RoguePropertyList__locate__Property( RoguePropertyList* THIS, RogueClassProperty* value_0 );
RoguePropertyList* RoguePropertyList__reserve__Int32( RoguePropertyList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueProperty__to_String( RogueClassProperty* THIS );
RogueString* RogueProperty__type_name( RogueClassProperty* THIS );
RogueClassProperty* RogueProperty__init__Token_Type_String_Type_Cmd( RogueClassProperty* THIS, RogueClassToken* _auto_116_0, RogueClassType* _auto_117_1, RogueString* _auto_118_2, RogueClassType* _auto_119_3, RogueClassCmd* _auto_120_4 );
RogueClassProperty* RogueProperty__clone( RogueClassProperty* THIS );
RogueLogical RogueProperty__is_incorporated( RogueClassProperty* THIS );
void RogueProperty__mark_incorporated( RogueClassProperty* THIS );
RogueClassProperty* RogueProperty__set_type_context__Type( RogueClassProperty* THIS, RogueClassType* _auto_121_0 );
RogueClassProperty* RogueProperty__init_object( RogueClassProperty* THIS );
RogueString* RogueString_MethodListTable__to_String( RogueClassString_MethodListTable* THIS );
RogueString* RogueString_MethodListTable__type_name( RogueClassString_MethodListTable* THIS );
RogueClassString_MethodListTable* RogueString_MethodListTable__init( RogueClassString_MethodListTable* THIS );
RogueClassString_MethodListTable* RogueString_MethodListTable__init__Int32( RogueClassString_MethodListTable* THIS, RogueInt32 bin_count_0 );
void RogueString_MethodListTable__clear( RogueClassString_MethodListTable* THIS );
RogueClassString_MethodListTableEntry* RogueString_MethodListTable__find__String( RogueClassString_MethodListTable* THIS, RogueString* key_0 );
RogueMethodList* RogueString_MethodListTable__get__String( RogueClassString_MethodListTable* THIS, RogueString* key_0 );
RogueClassString_MethodListTable* RogueString_MethodListTable__set__String_MethodList( RogueClassString_MethodListTable* THIS, RogueString* key_0, RogueMethodList* value_1 );
RogueStringBuilder* RogueString_MethodListTable__print_to__StringBuilder( RogueClassString_MethodListTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_MethodListTable* RogueString_MethodListTable__init_object( RogueClassString_MethodListTable* THIS );
RogueString* RogueTypeList__to_String( RogueTypeList* THIS );
RogueString* RogueTypeList__type_name( RogueTypeList* THIS );
RogueTypeList* RogueTypeList__init_object( RogueTypeList* THIS );
RogueTypeList* RogueTypeList__init( RogueTypeList* THIS );
RogueTypeList* RogueTypeList__init__Int32( RogueTypeList* THIS, RogueInt32 initial_capacity_0 );
RogueTypeList* RogueTypeList__add__Type( RogueTypeList* THIS, RogueClassType* value_0 );
RogueInt32 RogueTypeList__capacity( RogueTypeList* THIS );
RogueTypeList* RogueTypeList__choose__Function_Type_RETURNSLogical( RogueTypeList* THIS, RogueClassFunction_Type_RETURNSLogical* fn_0 );
RogueTypeList* RogueTypeList__clear( RogueTypeList* THIS );
void RogueTypeList__discard_from__Int32( RogueTypeList* THIS, RogueInt32 index_0 );
RogueTypeList* RogueTypeList__insert__Type_Int32( RogueTypeList* THIS, RogueClassType* value_0, RogueInt32 before_index_1 );
RogueOptionalInt32 RogueTypeList__locate__Type( RogueTypeList* THIS, RogueClassType* value_0 );
RogueTypeList* RogueTypeList__reserve__Int32( RogueTypeList* THIS, RogueInt32 additional_count_0 );
RogueClassType* RogueTypeList__remove__Type( RogueTypeList* THIS, RogueClassType* value_0 );
RogueClassType* RogueTypeList__remove_at__Int32( RogueTypeList* THIS, RogueInt32 index_0 );
RogueTypeList* RogueTypeList__swap__Int32_Int32( RogueTypeList* THIS, RogueInt32 i1_0, RogueInt32 i2_1 );
RogueString* RogueType__to_String( RogueClassType* THIS );
RogueString* RogueType__type_name( RogueClassType* THIS );
RogueClassType* RogueType__init__Token_String( RogueClassType* THIS, RogueClassToken* _auto_127_0, RogueString* _auto_128_1 );
RogueClassMethod* RogueType__add_method__Token_String( RogueClassType* THIS, RogueClassToken* m_t_0, RogueString* m_name_1 );
RogueClassMethod* RogueType__add_method__Method( RogueClassType* THIS, RogueClassMethod* m_0 );
RogueClassMethod* RogueType__replace_method__Method_Token_String( RogueClassType* THIS, RogueClassMethod* existing_0, RogueClassToken* m_t_1, RogueString* m_name_2 );
RogueClassMethod* RogueType__add_routine__Token_String( RogueClassType* THIS, RogueClassToken* r_t_0, RogueString* r_name_1 );
RogueClassProperty* RogueType__add_global__Token_String( RogueClassType* THIS, RogueClassToken* s_t_0, RogueString* s_name_1 );
RogueClassProperty* RogueType__add_property__Token_String_Type_Cmd( RogueClassType* THIS, RogueClassToken* p_t_0, RogueString* p_name_1, RogueClassType* p_type_2, RogueClassCmd* initial_value_3 );
RogueClassProperty* RogueType__add_property__Property( RogueClassType* THIS, RogueClassProperty* p_0 );
void RogueType__collect_type_info__Int32List( RogueClassType* THIS, RogueInt32List* info_0 );
RogueClassCmd* RogueType__create_default_value__Token( RogueClassType* THIS, RogueClassToken* _t_0 );
RogueClassMethod* RogueType__find_method__String( RogueClassType* THIS, RogueString* signature_0 );
RogueClassMethod* RogueType__find_routine__String( RogueClassType* THIS, RogueString* signature_0 );
RogueClassProperty* RogueType__find_property__String( RogueClassType* THIS, RogueString* p_name_0 );
RogueClassProperty* RogueType__find_global__String( RogueClassType* THIS, RogueString* s_name_0 );
RogueLogical RogueType__has_method_named__String( RogueClassType* THIS, RogueString* m_name_0 );
RogueLogical RogueType__has_routine_named__String( RogueClassType* THIS, RogueString* r_name_0 );
RogueLogical RogueType__instance_of__Type( RogueClassType* THIS, RogueClassType* ancestor_type_0 );
RogueLogical RogueType__is_compatible_with__Type( RogueClassType* THIS, RogueClassType* other_0 );
RogueLogical RogueType__is_equivalent_or_more_specific_than__Type( RogueClassType* THIS, RogueClassType* other_0 );
RogueLogical RogueType__is_aspect( RogueClassType* THIS );
RogueLogical RogueType__is_class( RogueClassType* THIS );
RogueLogical RogueType__is_compound( RogueClassType* THIS );
RogueLogical RogueType__is_functional( RogueClassType* THIS );
RogueLogical RogueType__is_native( RogueClassType* THIS );
RogueLogical RogueType__is_primitive( RogueClassType* THIS );
RogueLogical RogueType__is_reference( RogueClassType* THIS );
RogueLogical RogueType__is_requisite( RogueClassType* THIS );
RogueLogical RogueType__is_routine( RogueClassType* THIS );
RogueLogical RogueType__is_singleton( RogueClassType* THIS );
RogueClassType* RogueType__organize__Scope( RogueClassType* THIS, RogueClassScope* scope_0 );
RogueInt32 RogueType__primitive_rank( RogueClassType* THIS );
void RogueType__collect_base_types__TypeList( RogueClassType* THIS, RogueTypeList* list_0 );
void RogueType__cull_unused_methods( RogueClassType* THIS );
RogueLogical RogueType__has_global_references( RogueClassType* THIS );
RogueLogical RogueType__has_object_references( RogueClassType* THIS );
RogueLogical RogueType__is_reference_array( RogueClassType* THIS );
void RogueType__inherit_definitions__Type( RogueClassType* THIS, RogueClassType* from_type_0 );
void RogueType__inherit_properties__PropertyList_String_PropertyTable( RogueClassType* THIS, RoguePropertyList* list_0, RogueClassString_PropertyTable* lookup_1 );
void RogueType__inherit_property__Property_PropertyList_String_PropertyTable( RogueClassType* THIS, RogueClassProperty* p_0, RoguePropertyList* list_1, RogueClassString_PropertyTable* lookup_2 );
void RogueType__inherit_methods__MethodList_String_MethodTable( RogueClassType* THIS, RogueMethodList* list_0, RogueClassString_MethodTable* lookup_1 );
void RogueType__inherit_method__Method_MethodList_String_MethodTable( RogueClassType* THIS, RogueClassMethod* m_0, RogueMethodList* list_1, RogueClassString_MethodTable* lookup_2 );
void RogueType__inherit_routines__MethodList_String_MethodTable( RogueClassType* THIS, RogueMethodList* list_0, RogueClassString_MethodTable* lookup_1 );
void RogueType__inherit_routine__Method_MethodList_String_MethodTable( RogueClassType* THIS, RogueClassMethod* m_0, RogueMethodList* list_1, RogueClassString_MethodTable* lookup_2 );
void RogueType__apply_augment_labels__Method_Method( RogueClassType* THIS, RogueClassMethod* aug_m_0, RogueClassMethod* existing_m_1 );
void RogueType__index_and_move_inline_to_end__MethodList( RogueClassType* THIS, RogueMethodList* list_0 );
RogueLogical RogueType__omit_output( RogueClassType* THIS );
RogueClassType* RogueType__resolve( RogueClassType* THIS );
RogueLogical RogueType__should_cull( RogueClassType* THIS );
void RogueType__trace_used_code( RogueClassType* THIS );
void RogueType__assign_cpp_name( RogueClassType* THIS );
void RogueType__print_data_definition__CPPWriter( RogueClassType* THIS, RogueClassCPPWriter* writer_0 );
void RogueType__print_global_method_prototypes__CPPWriter( RogueClassType* THIS, RogueClassCPPWriter* writer_0 );
void RogueType__print_global_method_definitions__CPPWriter( RogueClassType* THIS, RogueClassCPPWriter* writer_0 );
void RogueType__print_method_prototypes__CPPWriter( RogueClassType* THIS, RogueClassCPPWriter* writer_0 );
void RogueType__determine_cpp_method_typedefs__StringList_String_MethodTable( RogueClassType* THIS, RogueStringList* list_0, RogueClassString_MethodTable* lookup_1 );
RogueInt32 RogueType__print_dynamic_method_table_entries__Int32_CPPWriter( RogueClassType* THIS, RogueInt32 at_index_0, RogueClassCPPWriter* writer_1 );
void RogueType__print_method_definitions__CPPWriter( RogueClassType* THIS, RogueClassCPPWriter* writer_0 );
RogueClassType* RogueType__init_object( RogueClassType* THIS );
RogueString* RogueString_TypeTable__to_String( RogueClassString_TypeTable* THIS );
RogueString* RogueString_TypeTable__type_name( RogueClassString_TypeTable* THIS );
RogueClassString_TypeTable* RogueString_TypeTable__init( RogueClassString_TypeTable* THIS );
RogueClassString_TypeTable* RogueString_TypeTable__init__Int32( RogueClassString_TypeTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_TypeTableEntry* RogueString_TypeTable__find__String( RogueClassString_TypeTable* THIS, RogueString* key_0 );
RogueClassType* RogueString_TypeTable__get__String( RogueClassString_TypeTable* THIS, RogueString* key_0 );
RogueClassString_TypeTable* RogueString_TypeTable__set__String_Type( RogueClassString_TypeTable* THIS, RogueString* key_0, RogueClassType* value_1 );
RogueStringBuilder* RogueString_TypeTable__print_to__StringBuilder( RogueClassString_TypeTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_TypeTable* RogueString_TypeTable__init_object( RogueClassString_TypeTable* THIS );
RogueString* RogueString_Int32Table__to_String( RogueClassString_Int32Table* THIS );
RogueString* RogueString_Int32Table__type_name( RogueClassString_Int32Table* THIS );
RogueClassString_Int32Table* RogueString_Int32Table__init( RogueClassString_Int32Table* THIS );
RogueClassString_Int32Table* RogueString_Int32Table__init__Int32( RogueClassString_Int32Table* THIS, RogueInt32 bin_count_0 );
RogueClassString_Int32TableEntry* RogueString_Int32Table__find__String( RogueClassString_Int32Table* THIS, RogueString* key_0 );
RogueInt32 RogueString_Int32Table__get__String( RogueClassString_Int32Table* THIS, RogueString* key_0 );
RogueClassString_Int32Table* RogueString_Int32Table__set__String_Int32( RogueClassString_Int32Table* THIS, RogueString* key_0, RogueInt32 value_1 );
RogueStringBuilder* RogueString_Int32Table__print_to__StringBuilder( RogueClassString_Int32Table* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_Int32Table* RogueString_Int32Table__init_object( RogueClassString_Int32Table* THIS );
RogueString* RogueString_StringListTable__to_String( RogueClassString_StringListTable* THIS );
RogueString* RogueString_StringListTable__type_name( RogueClassString_StringListTable* THIS );
RogueClassString_StringListTable* RogueString_StringListTable__init( RogueClassString_StringListTable* THIS );
RogueClassString_StringListTable* RogueString_StringListTable__init__Int32( RogueClassString_StringListTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_StringListTableEntry* RogueString_StringListTable__find__String( RogueClassString_StringListTable* THIS, RogueString* key_0 );
RogueStringList* RogueString_StringListTable__get__String( RogueClassString_StringListTable* THIS, RogueString* key_0 );
RogueClassString_StringListTable* RogueString_StringListTable__set__String_StringList( RogueClassString_StringListTable* THIS, RogueString* key_0, RogueStringList* value_1 );
RogueStringBuilder* RogueString_StringListTable__print_to__StringBuilder( RogueClassString_StringListTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_StringListTable* RogueString_StringListTable__init_object( RogueClassString_StringListTable* THIS );
RogueString* RogueString_Int32TableEntry__type_name( RogueClassString_Int32TableEntry* THIS );
RogueClassString_Int32TableEntry* RogueString_Int32TableEntry__init__String_Int32_Int32( RogueClassString_Int32TableEntry* THIS, RogueString* _key_0, RogueInt32 _value_1, RogueInt32 _hash_2 );
RogueClassString_Int32TableEntry* RogueString_Int32TableEntry__init_object( RogueClassString_Int32TableEntry* THIS );
RogueString* RogueInt32_TemplateTable__to_String( RogueClassInt32_TemplateTable* THIS );
RogueString* RogueInt32_TemplateTable__type_name( RogueClassInt32_TemplateTable* THIS );
RogueClassInt32_TemplateTable* RogueInt32_TemplateTable__init( RogueClassInt32_TemplateTable* THIS );
RogueClassInt32_TemplateTable* RogueInt32_TemplateTable__init__Int32( RogueClassInt32_TemplateTable* THIS, RogueInt32 bin_count_0 );
RogueLogical RogueInt32_TemplateTable__contains__Int32( RogueClassInt32_TemplateTable* THIS, RogueInt32 key_0 );
RogueClassInt32_TemplateTableEntry* RogueInt32_TemplateTable__find__Int32( RogueClassInt32_TemplateTable* THIS, RogueInt32 key_0 );
RogueClassTemplate* RogueInt32_TemplateTable__get__Int32( RogueClassInt32_TemplateTable* THIS, RogueInt32 key_0 );
RogueClassInt32_TemplateTable* RogueInt32_TemplateTable__set__Int32_Template( RogueClassInt32_TemplateTable* THIS, RogueInt32 key_0, RogueClassTemplate* value_1 );
RogueStringBuilder* RogueInt32_TemplateTable__print_to__StringBuilder( RogueClassInt32_TemplateTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassInt32_TemplateTable* RogueInt32_TemplateTable__init_object( RogueClassInt32_TemplateTable* THIS );
RogueString* RogueToken__to_String( RogueClassToken* THIS );
RogueString* RogueToken__type_name( RogueClassToken* THIS );
RogueClassToken* RogueToken__init__TokenType( RogueClassToken* THIS, RogueClassTokenType* _auto_136_0 );
RogueClassRogueError* RogueToken__error__String( RogueClassToken* THIS, RogueString* message_0 );
RogueLogical RogueToken__is_directive( RogueClassToken* THIS );
RogueLogical RogueToken__is_structure( RogueClassToken* THIS );
RogueString* RogueToken__quoted_name( RogueClassToken* THIS );
RogueClassToken* RogueToken__set_location__String_Int32_Int32( RogueClassToken* THIS, RogueString* _auto_137_0, RogueInt32 _auto_138_1, RogueInt32 _auto_139_2 );
RogueClassToken* RogueToken__set_location__Token( RogueClassToken* THIS, RogueClassToken* existing_0 );
RogueCharacter RogueToken__to_Character( RogueClassToken* THIS );
RogueInt32 RogueToken__to_Int32( RogueClassToken* THIS );
RogueInt64 RogueToken__to_Int64( RogueClassToken* THIS );
RogueReal64 RogueToken__to_Real64( RogueClassToken* THIS );
RogueClassType* RogueToken__to_Type( RogueClassToken* THIS );
RogueClassToken* RogueToken__init_object( RogueClassToken* THIS );
RogueString* RogueError__type_name( RogueClassError* THIS );
RogueClassError* RogueError__init_object( RogueClassError* THIS );
RogueString* RogueRogueError__to_String( RogueClassRogueError* THIS );
RogueString* RogueRogueError__type_name( RogueClassRogueError* THIS );
RogueClassRogueError* RogueRogueError__init_object( RogueClassRogueError* THIS );
RogueClassRogueError* RogueRogueError__init__String_String_Int32_Int32( RogueClassRogueError* THIS, RogueString* _auto_140_0, RogueString* _auto_141_1, RogueInt32 _auto_142_2, RogueInt32 _auto_143_3 );
RogueString* RogueTypeArray__type_name( RogueArray* THIS );
RogueString* RogueAttributes__type_name( RogueClassAttributes* THIS );
RogueClassAttributes* RogueAttributes__init__Int32( RogueClassAttributes* THIS, RogueInt32 _auto_146_0 );
RogueClassAttributes* RogueAttributes__clone( RogueClassAttributes* THIS );
RogueClassAttributes* RogueAttributes__add__Int32( RogueClassAttributes* THIS, RogueInt32 flag_0 );
RogueClassAttributes* RogueAttributes__add__String( RogueClassAttributes* THIS, RogueString* tag_0 );
RogueClassAttributes* RogueAttributes__add__Attributes( RogueClassAttributes* THIS, RogueClassAttributes* other_0 );
RogueString* RogueAttributes__element_type_name( RogueClassAttributes* THIS );
RogueClassAttributes* RogueAttributes__init_object( RogueClassAttributes* THIS );
RogueString* RogueCmd__type_name( RogueClassCmd* THIS );
void RogueCmd__add_to__CmdStatementList( RogueClassCmd* THIS, RogueClassCmdStatementList* statements_0 );
RogueClassCmd* RogueCmd__call_prior__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmd__cast_to__Type_Scope( RogueClassCmd* THIS, RogueClassType* target_type_0, RogueClassScope* scope_1 );
RogueClassCmd* RogueCmd__clone__CloneArgs( RogueClassCmd* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmd__clone__Cmd_CloneArgs( RogueClassCmd* THIS, RogueClassCmd* other_0, RogueClassCloneArgs* clone_args_1 );
RogueClassCmdArgs* RogueCmd__clone__CmdArgs_CloneArgs( RogueClassCmd* THIS, RogueClassCmdArgs* args_0, RogueClassCloneArgs* clone_args_1 );
RogueClassCmdStatementList* RogueCmd__clone__CmdStatementList_CloneArgs( RogueClassCmd* THIS, RogueClassCmdStatementList* statements_0, RogueClassCloneArgs* clone_args_1 );
RogueClassCmd* RogueCmd__combine_literal_operands__Type( RogueClassCmd* THIS, RogueClassType* common_type_0 );
void RogueCmd__exit_scope__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueClassType* RogueCmd__find_operation_result_type__Type_Type( RogueClassCmd* THIS, RogueClassType* left_type_0, RogueClassType* right_type_1 );
RogueClassType* RogueCmd__find_common_type__Type_Type( RogueClassCmd* THIS, RogueClassType* left_type_0, RogueClassType* right_type_1 );
RogueLogical RogueCmd__is_generic_function( RogueClassCmd* THIS );
RogueClassType* RogueCmd__must_find_common_type__Type_Type( RogueClassCmd* THIS, RogueClassType* left_type_0, RogueClassType* right_type_1 );
RogueClassType* RogueCmd__implicit_type__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueLogical RogueCmd__is_literal( RogueClassCmd* THIS );
void RogueCmd__require_type_context( RogueClassCmd* THIS );
RogueClassCmd* RogueCmd__require_integer( RogueClassCmd* THIS );
RogueClassCmd* RogueCmd__require_logical__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_Cmd__require_type( RogueClassCmd* THIS );
RogueClassCmd* RogueCmd__require_value( RogueClassCmd* THIS );
RogueLogical RogueCmd__requires_semicolon( RogueClassCmd* THIS );
RogueClassCmd* RogueCmd__resolve__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmd__resolve_assignment__Scope_Cmd( RogueClassCmd* THIS, RogueClassScope* scope_0, RogueClassCmd* new_value_1 );
RogueClassCmd* RogueCmd__resolve_adjust__Scope_Int32( RogueClassCmd* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
RogueClassCmd* RogueCmd__resolve_modify_and_assign__Scope_TokenType_Cmd( RogueClassCmd* THIS, RogueClassScope* scope_0, RogueClassTokenType* op_1, RogueClassCmd* new_value_2 );
RogueClassType* Rogue_Cmd__type( RogueClassCmd* THIS );
void RogueCmd__write_cpp__CPPWriter_Logical( RogueClassCmd* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmd__trace_used_code( RogueClassCmd* THIS );
void RogueCmd__update_this_type__Scope( RogueClassCmd* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmd__init_object( RogueClassCmd* THIS );
RogueString* RogueCmdReturn__type_name( RogueClassCmdReturn* THIS );
RogueClassCmd* RogueCmdReturn__clone__CloneArgs( RogueClassCmdReturn* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReturn__resolve__Scope( RogueClassCmdReturn* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdReturn__type( RogueClassCmdReturn* THIS );
void RogueCmdReturn__write_cpp__CPPWriter_Logical( RogueClassCmdReturn* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReturn__trace_used_code( RogueClassCmdReturn* THIS );
void RogueCmdReturn__update_this_type__Scope( RogueClassCmdReturn* THIS, RogueClassScope* scope_0 );
RogueClassCmdReturn* RogueCmdReturn__init_object( RogueClassCmdReturn* THIS );
RogueClassCmdReturn* RogueCmdReturn__init__Token_Cmd( RogueClassCmdReturn* THIS, RogueClassToken* _auto_148_0, RogueClassCmd* _auto_149_1 );
RogueString* RogueCmdStatement__type_name( RogueClassCmdStatement* THIS );
void RogueCmdStatement__trace_used_code( RogueClassCmdStatement* THIS );
void RogueCmdStatement__update_this_type__Scope( RogueClassCmdStatement* THIS, RogueClassScope* scope_0 );
RogueClassCmdStatement* RogueCmdStatement__init_object( RogueClassCmdStatement* THIS );
RogueString* RogueCmdStatementList__type_name( RogueClassCmdStatementList* THIS );
RogueClassCmdStatementList* RogueCmdStatementList__init_object( RogueClassCmdStatementList* THIS );
RogueClassCmdStatementList* RogueCmdStatementList__init( RogueClassCmdStatementList* THIS );
RogueClassCmdStatementList* RogueCmdStatementList__init__Int32( RogueClassCmdStatementList* THIS, RogueInt32 initial_capacity_0 );
RogueClassCmdStatementList* RogueCmdStatementList__init__Cmd( RogueClassCmdStatementList* THIS, RogueClassCmd* statement_0 );
RogueClassCmdStatementList* RogueCmdStatementList__clone__CloneArgs( RogueClassCmdStatementList* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdStatementList__resolve__Scope( RogueClassCmdStatementList* THIS, RogueClassScope* scope_0 );
void RogueCmdStatementList__write_cpp__CPPWriter_Logical( RogueClassCmdStatementList* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdStatementList__trace_used_code( RogueClassCmdStatementList* THIS );
void RogueCmdStatementList__update_this_type__Scope( RogueClassCmdStatementList* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdList__to_String( RogueCmdList* THIS );
RogueString* RogueCmdList__type_name( RogueCmdList* THIS );
RogueCmdList* RogueCmdList__init_object( RogueCmdList* THIS );
RogueCmdList* RogueCmdList__init( RogueCmdList* THIS );
RogueCmdList* RogueCmdList__init__Int32( RogueCmdList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdList* RogueCmdList__add__Cmd( RogueCmdList* THIS, RogueClassCmd* value_0 );
RogueCmdList* RogueCmdList__add__CmdList( RogueCmdList* THIS, RogueCmdList* other_0 );
RogueInt32 RogueCmdList__capacity( RogueCmdList* THIS );
void RogueCmdList__discard__Int32_Int32( RogueCmdList* THIS, RogueInt32 i1_0, RogueInt32 n_1 );
void RogueCmdList__discard_from__Int32( RogueCmdList* THIS, RogueInt32 index_0 );
RogueCmdList* RogueCmdList__insert__Cmd_Int32( RogueCmdList* THIS, RogueClassCmd* value_0, RogueInt32 before_index_1 );
RogueClassCmd* RogueCmdList__last( RogueCmdList* THIS );
RogueCmdList* RogueCmdList__reserve__Int32( RogueCmdList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueTokenType__to_String( RogueClassTokenType* THIS );
RogueString* RogueTokenType__type_name( RogueClassTokenType* THIS );
RogueClassTokenType* RogueTokenType__init__String( RogueClassTokenType* THIS, RogueString* _auto_155_0 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32_Character( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueCharacter value_3 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32_Int64( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueInt64 value_3 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32_Int32( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueInt32 value_3 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32_Real64( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueReal64 value_3 );
RogueClassToken* RogueTokenType__create_token__String_Int32_Int32_String( RogueClassTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueString* value_3 );
RogueClassToken* RogueTokenType__create_token__Token( RogueClassTokenType* THIS, RogueClassToken* existing_0 );
RogueClassToken* RogueTokenType__create_token__Token_String( RogueClassTokenType* THIS, RogueClassToken* existing_0, RogueString* value_1 );
RogueLogical RogueTokenType__is_directive( RogueClassTokenType* THIS );
RogueLogical RogueTokenType__is_op_with_assign( RogueClassTokenType* THIS );
RogueLogical RogueTokenType__is_structure( RogueClassTokenType* THIS );
RogueString* RogueTokenType__quoted_name( RogueClassTokenType* THIS );
RogueString* RogueTokenType__to_String__Token( RogueClassTokenType* THIS, RogueClassToken* t_0 );
RogueClassTokenType* RogueTokenType__init_object( RogueClassTokenType* THIS );
RogueString* RogueCmdLabel__type_name( RogueClassCmdLabel* THIS );
RogueClassCmdLabel* RogueCmdLabel__clone__CloneArgs( RogueClassCmdLabel* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdLabel__requires_semicolon( RogueClassCmdLabel* THIS );
RogueClassCmd* RogueCmdLabel__resolve__Scope( RogueClassCmdLabel* THIS, RogueClassScope* scope_0 );
void RogueCmdLabel__write_cpp__CPPWriter_Logical( RogueClassCmdLabel* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdLabel__trace_used_code( RogueClassCmdLabel* THIS );
void RogueCmdLabel__update_this_type__Scope( RogueClassCmdLabel* THIS, RogueClassScope* scope_0 );
RogueClassCmdLabel* RogueCmdLabel__init_object( RogueClassCmdLabel* THIS );
RogueClassCmdLabel* RogueCmdLabel__init__Token_String_CmdStatementList( RogueClassCmdLabel* THIS, RogueClassToken* _auto_156_0, RogueString* _auto_157_1, RogueClassCmdStatementList* _auto_158_2 );
RogueString* RoguePropertyArray__type_name( RogueArray* THIS );
RogueString* RogueScope__type_name( RogueClassScope* THIS );
RogueClassScope* RogueScope__init__Type_Method( RogueClassScope* THIS, RogueClassType* _auto_161_0, RogueClassMethod* _auto_162_1 );
RogueClassLocal* RogueScope__find_local__String( RogueClassScope* THIS, RogueString* name_0 );
RogueClassType* RogueScope__find_type__String( RogueClassScope* THIS, RogueString* name_0 );
void RogueScope__push_local__Local_Logical( RogueClassScope* THIS, RogueClassLocal* v_0, RogueLogical validate_name_1 );
void RogueScope__pop_local( RogueClassScope* THIS );
RogueClassType* RogueScope__set_implicit_type__Type( RogueClassScope* THIS, RogueClassType* new_implicit_type_0 );
RogueClassCmd* RogueScope__resolve_call__Type_CmdAccess_Logical_Logical( RogueClassScope* THIS, RogueClassType* type_context_0, RogueClassCmdAccess* access_1, RogueLogical error_on_fail_2, RogueLogical suppress_inherited_3 );
RogueClassMethod* RogueScope__find_method__Type_CmdAccess_Logical_Logical( RogueClassScope* THIS, RogueClassType* type_context_0, RogueClassCmdAccess* access_1, RogueLogical error_on_fail_2, RogueLogical suppress_inherited_3 );
RogueClassScope* RogueScope__init_object( RogueClassScope* THIS );
RogueString* RogueRequisiteItemArray__type_name( RogueArray* THIS );
RogueString* RogueMethodList__to_String( RogueMethodList* THIS );
RogueString* RogueMethodList__type_name( RogueMethodList* THIS );
RogueMethodList* RogueMethodList__init_object( RogueMethodList* THIS );
RogueMethodList* RogueMethodList__init( RogueMethodList* THIS );
RogueMethodList* RogueMethodList__init__Int32( RogueMethodList* THIS, RogueInt32 initial_capacity_0 );
RogueMethodList* RogueMethodList__add__Method( RogueMethodList* THIS, RogueClassMethod* value_0 );
RogueMethodList* RogueMethodList__add__MethodList( RogueMethodList* THIS, RogueMethodList* other_0 );
RogueInt32 RogueMethodList__capacity( RogueMethodList* THIS );
RogueMethodList* RogueMethodList__clear( RogueMethodList* THIS );
RogueOptionalInt32 RogueMethodList__locate__Method( RogueMethodList* THIS, RogueClassMethod* value_0 );
RogueMethodList* RogueMethodList__reserve__Int32( RogueMethodList* THIS, RogueInt32 additional_count_0 );
RogueClassMethod* RogueMethodList__remove__Method( RogueMethodList* THIS, RogueClassMethod* value_0 );
RogueClassMethod* RogueMethodList__remove_at__Int32( RogueMethodList* THIS, RogueInt32 index_0 );
RogueString* RogueMethodArray__type_name( RogueArray* THIS );
RogueString* RogueTemplateArray__type_name( RogueArray* THIS );
RogueString* RogueFunction_Type_RETURNSLogical__type_name( RogueClassFunction_Type_RETURNSLogical* THIS );
RogueLogical RogueFunction_Type_RETURNSLogical__call__Type( RogueClassFunction_Type_RETURNSLogical* THIS, RogueClassType* param1_0 );
RogueClassFunction_Type_RETURNSLogical* RogueFunction_Type_RETURNSLogical__init_object( RogueClassFunction_Type_RETURNSLogical* THIS );
RogueString* RogueFunction_197__type_name( RogueClassFunction_197* THIS );
RogueLogical RogueFunction_197__call__Type( RogueClassFunction_197* THIS, RogueClassType* type_0 );
RogueClassFunction_197* RogueFunction_197__init_object( RogueClassFunction_197* THIS );
RogueString* RogueFunction_198__type_name( RogueClassFunction_198* THIS );
RogueLogical RogueFunction_198__call__Type( RogueClassFunction_198* THIS, RogueClassType* type_0 );
RogueClassFunction_198* RogueFunction_198__init_object( RogueClassFunction_198* THIS );
RogueString* RogueCPPWriter__type_name( RogueClassCPPWriter* THIS );
RogueClassCPPWriter* RogueCPPWriter__init__String( RogueClassCPPWriter* THIS, RogueString* _auto_222_0 );
void RogueCPPWriter__close( RogueClassCPPWriter* THIS );
void RogueCPPWriter__print_indent( RogueClassCPPWriter* THIS );
RogueClassCPPWriter* RogueCPPWriter__print__Int64( RogueClassCPPWriter* THIS, RogueInt64 value_0 );
RogueClassCPPWriter* RogueCPPWriter__print__Int32( RogueClassCPPWriter* THIS, RogueInt32 value_0 );
RogueClassCPPWriter* RogueCPPWriter__print__Real64( RogueClassCPPWriter* THIS, RogueReal64 value_0 );
RogueClassCPPWriter* RogueCPPWriter__print__Character( RogueClassCPPWriter* THIS, RogueCharacter value_0 );
RogueClassCPPWriter* RogueCPPWriter__print__String( RogueClassCPPWriter* THIS, RogueString* value_0 );
RogueClassCPPWriter* RogueCPPWriter__println( RogueClassCPPWriter* THIS );
RogueClassCPPWriter* RogueCPPWriter__println__String( RogueClassCPPWriter* THIS, RogueString* value_0 );
RogueClassCPPWriter* RogueCPPWriter__print__Type( RogueClassCPPWriter* THIS, RogueClassType* type_0 );
RogueClassCPPWriter* RogueCPPWriter__print_cast__Type_Type( RogueClassCPPWriter* THIS, RogueClassType* from_type_0, RogueClassType* to_type_1 );
RogueClassCPPWriter* RogueCPPWriter__print_open_cast__Type_Type( RogueClassCPPWriter* THIS, RogueClassType* from_type_0, RogueClassType* to_type_1 );
RogueClassCPPWriter* RogueCPPWriter__print_close_cast__Type_Type( RogueClassCPPWriter* THIS, RogueClassType* from_type_0, RogueClassType* to_type_1 );
RogueClassCPPWriter* RogueCPPWriter__print_cast__Type_Type_Cmd( RogueClassCPPWriter* THIS, RogueClassType* from_type_0, RogueClassType* to_type_1, RogueClassCmd* cmd_2 );
RogueClassCPPWriter* RogueCPPWriter__print_access_operator__Type( RogueClassCPPWriter* THIS, RogueClassType* type_0 );
RogueClassCPPWriter* RogueCPPWriter__print_type_name__Type( RogueClassCPPWriter* THIS, RogueClassType* type_0 );
RogueClassCPPWriter* RogueCPPWriter__print_type_info__Type( RogueClassCPPWriter* THIS, RogueClassType* type_0 );
RogueClassCPPWriter* RogueCPPWriter__print_default_value__Type( RogueClassCPPWriter* THIS, RogueClassType* type_0 );
RogueClassCPPWriter* RogueCPPWriter__print_literal_character__Character_Logical( RogueClassCPPWriter* THIS, RogueCharacter ch_0, RogueLogical in_string_1 );
RogueClassCPPWriter* RogueCPPWriter__print_literal_string__String( RogueClassCPPWriter* THIS, RogueString* st_0 );
RogueClassCPPWriter* RogueCPPWriter__print_native_code__Token_Type_Method_String( RogueClassCPPWriter* THIS, RogueClassToken* t_0, RogueClassType* type_context_1, RogueClassMethod* method_context_2, RogueString* code_3 );
RogueClassCPPWriter* RogueCPPWriter__print_native_marker__Token_Type_Method_String( RogueClassCPPWriter* THIS, RogueClassToken* t_0, RogueClassType* type_context_1, RogueClassMethod* method_context_2, RogueString* name_3 );
void RogueCPPWriter__print_native_code_marker_value__Token_String_String_String_Type( RogueClassCPPWriter* THIS, RogueClassToken* t_0, RogueString* name_1, RogueString* operation_2, RogueString* default_3, RogueClassType* type_4 );
void RogueCPPWriter__mark_statement_start__String_Int32_Int32( RogueClassCPPWriter* THIS, RogueString* statement_filepath_0, RogueInt32 start_line_1, RogueInt32 end_line_2 );
void RogueCPPWriter__mark_statement_end( RogueClassCPPWriter* THIS );
void RogueCPPWriter__mark_method_start__Type_Method( RogueClassCPPWriter* THIS, RogueClassType* t_0, RogueClassMethod* m_1 );
void RogueCPPWriter__mark_method_end( RogueClassCPPWriter* THIS );
void RogueCPPWriter__mark_routine_start__Method( RogueClassCPPWriter* THIS, RogueClassMethod* m_0 );
void RogueCPPWriter__mark_routine_end( RogueClassCPPWriter* THIS );
RogueClassCPPWriter* RogueCPPWriter__init_object( RogueClassCPPWriter* THIS );
RogueString* RogueString_MethodTable__to_String( RogueClassString_MethodTable* THIS );
RogueString* RogueString_MethodTable__type_name( RogueClassString_MethodTable* THIS );
RogueClassString_MethodTable* RogueString_MethodTable__init( RogueClassString_MethodTable* THIS );
RogueClassString_MethodTable* RogueString_MethodTable__init__Int32( RogueClassString_MethodTable* THIS, RogueInt32 bin_count_0 );
void RogueString_MethodTable__clear( RogueClassString_MethodTable* THIS );
RogueClassString_MethodTableEntry* RogueString_MethodTable__find__String( RogueClassString_MethodTable* THIS, RogueString* key_0 );
RogueClassMethod* RogueString_MethodTable__get__String( RogueClassString_MethodTable* THIS, RogueString* key_0 );
RogueClassString_MethodTable* RogueString_MethodTable__set__String_Method( RogueClassString_MethodTable* THIS, RogueString* key_0, RogueClassMethod* value_1 );
RogueStringBuilder* RogueString_MethodTable__print_to__StringBuilder( RogueClassString_MethodTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_MethodTable* RogueString_MethodTable__init_object( RogueClassString_MethodTable* THIS );
RogueString* RogueLocalList__to_String( RogueLocalList* THIS );
RogueString* RogueLocalList__type_name( RogueLocalList* THIS );
RogueLocalList* RogueLocalList__init_object( RogueLocalList* THIS );
RogueLocalList* RogueLocalList__init( RogueLocalList* THIS );
RogueLocalList* RogueLocalList__init__Int32( RogueLocalList* THIS, RogueInt32 initial_capacity_0 );
RogueLocalList* RogueLocalList__add__Local( RogueLocalList* THIS, RogueClassLocal* value_0 );
RogueInt32 RogueLocalList__capacity( RogueLocalList* THIS );
RogueLocalList* RogueLocalList__clear( RogueLocalList* THIS );
RogueLocalList* RogueLocalList__reserve__Int32( RogueLocalList* THIS, RogueInt32 additional_count_0 );
RogueClassLocal* RogueLocalList__remove_at__Int32( RogueLocalList* THIS, RogueInt32 index_0 );
RogueClassLocal* RogueLocalList__remove_last( RogueLocalList* THIS );
RogueString* RogueLocal__type_name( RogueClassLocal* THIS );
RogueClassLocal* RogueLocal__init__Token_String( RogueClassLocal* THIS, RogueClassToken* _auto_249_0, RogueString* _auto_250_1 );
RogueClassLocal* RogueLocal__clone__CloneArgs( RogueClassLocal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueString* RogueLocal__cpp_name( RogueClassLocal* THIS );
RogueClassLocal* RogueLocal__init_object( RogueClassLocal* THIS );
RogueString* RogueLocalArray__type_name( RogueArray* THIS );
RogueString* RogueInt32List__to_String( RogueInt32List* THIS );
RogueString* RogueInt32List__type_name( RogueInt32List* THIS );
RogueInt32List* RogueInt32List__init_object( RogueInt32List* THIS );
RogueInt32List* RogueInt32List__init( RogueInt32List* THIS );
RogueInt32List* RogueInt32List__init__Int32( RogueInt32List* THIS, RogueInt32 initial_capacity_0 );
RogueInt32List* RogueInt32List__add__Int32( RogueInt32List* THIS, RogueInt32 value_0 );
RogueInt32 RogueInt32List__capacity( RogueInt32List* THIS );
RogueInt32List* RogueInt32List__reserve__Int32( RogueInt32List* THIS, RogueInt32 additional_count_0 );
RogueString* RogueInt32Array__type_name( RogueArray* THIS );
RogueString* RogueByteList__to_String( RogueByteList* THIS );
RogueString* RogueByteList__type_name( RogueByteList* THIS );
RogueByteList* RogueByteList__init_object( RogueByteList* THIS );
RogueByteList* RogueByteList__init( RogueByteList* THIS );
RogueByteList* RogueByteList__init__Int32( RogueByteList* THIS, RogueInt32 initial_capacity_0 );
RogueByteList* RogueByteList__add__Byte( RogueByteList* THIS, RogueByte value_0 );
RogueInt32 RogueByteList__capacity( RogueByteList* THIS );
RogueByteList* RogueByteList__clear( RogueByteList* THIS );
RogueByteList* RogueByteList__insert__Byte_Int32( RogueByteList* THIS, RogueByte value_0, RogueInt32 before_index_1 );
RogueByteList* RogueByteList__reserve__Int32( RogueByteList* THIS, RogueInt32 additional_count_0 );
RogueByte RogueByteList__remove_at__Int32( RogueByteList* THIS, RogueInt32 index_0 );
RogueByte RogueByteList__remove_last( RogueByteList* THIS );
RogueByteList* RogueByteList__reverse( RogueByteList* THIS );
RogueByteList* RogueByteList__reverse__Int32_Int32( RogueByteList* THIS, RogueInt32 i1_0, RogueInt32 i2_1 );
RogueString* RogueByteArray__type_name( RogueArray* THIS );
RogueString* RogueSystem__type_name( RogueClassSystem* THIS );
RogueClassSystem* RogueSystem__init_object( RogueClassSystem* THIS );
RogueString* RogueString_LogicalTable__to_String( RogueClassString_LogicalTable* THIS );
RogueString* RogueString_LogicalTable__type_name( RogueClassString_LogicalTable* THIS );
RogueClassString_LogicalTable* RogueString_LogicalTable__init( RogueClassString_LogicalTable* THIS );
RogueClassString_LogicalTable* RogueString_LogicalTable__init__Int32( RogueClassString_LogicalTable* THIS, RogueInt32 bin_count_0 );
RogueLogical RogueString_LogicalTable__contains__String( RogueClassString_LogicalTable* THIS, RogueString* key_0 );
RogueClassString_LogicalTableEntry* RogueString_LogicalTable__find__String( RogueClassString_LogicalTable* THIS, RogueString* key_0 );
RogueLogical RogueString_LogicalTable__get__String( RogueClassString_LogicalTable* THIS, RogueString* key_0 );
RogueClassString_LogicalTable* RogueString_LogicalTable__set__String_Logical( RogueClassString_LogicalTable* THIS, RogueString* key_0, RogueLogical value_1 );
RogueStringBuilder* RogueString_LogicalTable__print_to__StringBuilder( RogueClassString_LogicalTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_LogicalTable* RogueString_LogicalTable__init_object( RogueClassString_LogicalTable* THIS );
RogueString* RogueParserList__to_String( RogueParserList* THIS );
RogueString* RogueParserList__type_name( RogueParserList* THIS );
RogueParserList* RogueParserList__init_object( RogueParserList* THIS );
RogueParserList* RogueParserList__init( RogueParserList* THIS );
RogueParserList* RogueParserList__init__Int32( RogueParserList* THIS, RogueInt32 initial_capacity_0 );
RogueParserList* RogueParserList__add__Parser( RogueParserList* THIS, RogueClassParser* value_0 );
RogueInt32 RogueParserList__capacity( RogueParserList* THIS );
RogueParserList* RogueParserList__reserve__Int32( RogueParserList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueParser__type_name( RogueClassParser* THIS );
RogueClassParser* RogueParser__init__String( RogueClassParser* THIS, RogueString* _auto_363_0 );
RogueClassParser* RogueParser__init__Token_String_String( RogueClassParser* THIS, RogueClassToken* t_0, RogueString* _auto_364_1, RogueString* data_2 );
RogueClassParser* RogueParser__init__TokenList_Logical( RogueClassParser* THIS, RogueTokenList* tokens_0, RogueLogical skip_reprocess_1 );
void RogueParser__add_used_module__String( RogueClassParser* THIS, RogueString* module_name_0 );
RogueLogical RogueParser__consume__TokenType( RogueClassParser* THIS, RogueClassTokenType* type_0 );
RogueLogical RogueParser__consume__String( RogueClassParser* THIS, RogueString* identifier_0 );
RogueLogical RogueParser__consume_end_commands( RogueClassParser* THIS );
RogueLogical RogueParser__consume_eols( RogueClassParser* THIS );
RogueClassRogueError* RogueParser__error__String( RogueClassParser* THIS, RogueString* message_0 );
void RogueParser__insert_module_prefixes( RogueClassParser* THIS );
void RogueParser__must_consume__TokenType_String( RogueClassParser* THIS, RogueClassTokenType* type_0, RogueString* error_message_1 );
void RogueParser__must_consume_eols( RogueClassParser* THIS );
RogueClassToken* RogueParser__must_read__TokenType( RogueClassParser* THIS, RogueClassTokenType* type_0 );
RogueLogical RogueParser__next_is__TokenType( RogueClassParser* THIS, RogueClassTokenType* type_0 );
RogueLogical RogueParser__next_is_end_command( RogueClassParser* THIS );
RogueLogical RogueParser__next_is_statement( RogueClassParser* THIS );
void RogueParser__parse_elements( RogueClassParser* THIS );
RogueLogical RogueParser__parse_element( RogueClassParser* THIS );
void RogueParser__parse_class_template( RogueClassParser* THIS );
void RogueParser__parse_routine( RogueClassParser* THIS );
void RogueParser__parse_template_tokens__Template_TokenType( RogueClassParser* THIS, RogueClassTemplate* template_0, RogueClassTokenType* end_type_1 );
void RogueParser__parse_augment( RogueClassParser* THIS );
void RogueParser__parse_attributes__Attributes( RogueClassParser* THIS, RogueClassAttributes* attributes_0 );
void RogueParser__ensure_unspecialized_element_type__Token_Attributes( RogueClassParser* THIS, RogueClassToken* t_0, RogueClassAttributes* attributes_1 );
void RogueParser__parse_type_def__Type( RogueClassParser* THIS, RogueClassType* _auto_365_0 );
RogueLogical RogueParser__parse_section( RogueClassParser* THIS );
RogueLogical RogueParser__parse_definitions__Logical( RogueClassParser* THIS, RogueLogical enumerate_0 );
RogueLogical RogueParser__parse_properties__Logical( RogueClassParser* THIS, RogueLogical as_global_0 );
RogueLogical RogueParser__parse_method__Logical( RogueClassParser* THIS, RogueLogical as_routine_0 );
void RogueParser__parse_single_or_multi_line_statements__CmdStatementList_TokenType( RogueClassParser* THIS, RogueClassCmdStatementList* statements_0, RogueClassTokenType* end_type_1 );
void RogueParser__parse_multi_line_statements__CmdStatementList( RogueClassParser* THIS, RogueClassCmdStatementList* statements_0 );
void RogueParser__parse_augment_statements( RogueClassParser* THIS );
void RogueParser__parse_single_line_statements__CmdStatementList( RogueClassParser* THIS, RogueClassCmdStatementList* statements_0 );
void RogueParser__parse_statement__CmdStatementList_Logical( RogueClassParser* THIS, RogueClassCmdStatementList* statements_0, RogueLogical allow_control_structures_1 );
RogueClassCmdWhich* RogueParser__parse_which( RogueClassParser* THIS );
RogueLogical RogueParser__parse_native_elements( RogueClassParser* THIS );
RogueClassCmdContingent* RogueParser__parse_contingent( RogueClassParser* THIS );
RogueClassCmdTry* RogueParser__parse_try( RogueClassParser* THIS );
void RogueParser__parse_local_or_global__CmdStatementList( RogueClassParser* THIS, RogueClassCmdStatementList* statements_0 );
RogueClassType* Rogue_Parser__parse_type( RogueClassParser* THIS );
RogueString* RogueParser__parse_possible_type__Logical( RogueClassParser* THIS, RogueLogical allow_at_sign_0 );
RogueClassCmdIf* RogueParser__parse_if( RogueClassParser* THIS );
RogueClassCmdGenericLoop* RogueParser__parse_loop( RogueClassParser* THIS );
RogueClassCmdGenericLoop* RogueParser__parse_while( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_for_each( RogueClassParser* THIS );
RogueClassToken* RogueParser__peek( RogueClassParser* THIS );
RogueClassToken* RogueParser__read( RogueClassParser* THIS );
RogueString* RogueParser__read_identifier__Logical( RogueClassParser* THIS, RogueLogical allow_at_sign_0 );
RogueClassCmd* RogueParser__parse_expression( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_range( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_range__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_logical_xor( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_logical_xor__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_logical_or( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_logical_or__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_logical_and( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_logical_and__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_comparison( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_comparison__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_bitwise_xor( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_bitwise_xor__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_bitwise_or( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_bitwise_or__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_bitwise_and( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_bitwise_and__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_bitwise_shift( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_bitwise_shift__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_add_subtract( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_add_subtract__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_multiply_divide_mod( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_multiply_divide_mod__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_power( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_power__Cmd( RogueClassParser* THIS, RogueClassCmd* lhs_0 );
RogueClassCmd* RogueParser__parse_pre_unary( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_post_unary( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_post_unary__Cmd( RogueClassParser* THIS, RogueClassCmd* operand_0 );
RogueClassCmd* RogueParser__parse_member_access( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_member_access__Cmd( RogueClassParser* THIS, RogueClassCmd* context_0 );
RogueClassCmd* RogueParser__parse_access__Token_Cmd( RogueClassParser* THIS, RogueClassToken* t_0, RogueClassCmd* context_1 );
RogueCmdFlagArgList* RogueParser__parse_args__CmdArgs( RogueClassParser* THIS, RogueClassCmdArgs* args_0 );
RogueString* RogueParser__parse_specialization_string( RogueClassParser* THIS );
RogueInt32 RogueParser__parse_specializer_count( RogueClassParser* THIS );
void RogueParser__parse_specializer__StringBuilder_TokenList( RogueClassParser* THIS, RogueStringBuilder* buffer_0, RogueTokenList* tokens_1 );
RogueClassCmd* RogueParser__parse_term( RogueClassParser* THIS );
RogueClassCmd* RogueParser__parse_typed_literal_list__Token_String( RogueClassParser* THIS, RogueClassToken* t_0, RogueString* list_type_name_1 );
RogueClassParser* RogueParser__init_object( RogueClassParser* THIS );
RogueString* RogueString_ParseReaderTable__to_String( RogueClassString_ParseReaderTable* THIS );
RogueString* RogueString_ParseReaderTable__type_name( RogueClassString_ParseReaderTable* THIS );
RogueClassString_ParseReaderTable* RogueString_ParseReaderTable__init( RogueClassString_ParseReaderTable* THIS );
RogueClassString_ParseReaderTable* RogueString_ParseReaderTable__init__Int32( RogueClassString_ParseReaderTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_ParseReaderTableEntry* RogueString_ParseReaderTable__find__String( RogueClassString_ParseReaderTable* THIS, RogueString* key_0 );
RogueClassParseReader* RogueString_ParseReaderTable__get__String( RogueClassString_ParseReaderTable* THIS, RogueString* key_0 );
RogueClassString_ParseReaderTable* RogueString_ParseReaderTable__set__String_ParseReader( RogueClassString_ParseReaderTable* THIS, RogueString* key_0, RogueClassParseReader* value_1 );
RogueStringBuilder* RogueString_ParseReaderTable__print_to__StringBuilder( RogueClassString_ParseReaderTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_ParseReaderTable* RogueString_ParseReaderTable__init_object( RogueClassString_ParseReaderTable* THIS );
RogueString* RogueFile__to_String( RogueClassFile* THIS );
RogueString* RogueFile__type_name( RogueClassFile* THIS );
RogueClassFile* RogueFile__init__String( RogueClassFile* THIS, RogueString* _auto_374_0 );
RogueString* RogueFile__filename( RogueClassFile* THIS );
RogueClassFile* RogueFile__init_object( RogueClassFile* THIS );
RogueString* RogueParserArray__type_name( RogueArray* THIS );
RogueString* RogueTokenList__to_String( RogueTokenList* THIS );
RogueString* RogueTokenList__type_name( RogueTokenList* THIS );
RogueTokenList* RogueTokenList__init_object( RogueTokenList* THIS );
RogueTokenList* RogueTokenList__init( RogueTokenList* THIS );
RogueTokenList* RogueTokenList__init__Int32( RogueTokenList* THIS, RogueInt32 initial_capacity_0 );
RogueTokenList* RogueTokenList__add__Token( RogueTokenList* THIS, RogueClassToken* value_0 );
RogueTokenList* RogueTokenList__add__TokenList( RogueTokenList* THIS, RogueTokenList* other_0 );
RogueInt32 RogueTokenList__capacity( RogueTokenList* THIS );
void RogueTokenList__discard_from__Int32( RogueTokenList* THIS, RogueInt32 index_0 );
RogueClassToken* RogueTokenList__last( RogueTokenList* THIS );
RogueClassTokenListRebuilder* RogueTokenList__rebuilder( RogueTokenList* THIS );
RogueTokenList* RogueTokenList__reserve__Int32( RogueTokenList* THIS, RogueInt32 additional_count_0 );
RogueClassToken* RogueTokenList__remove_at__Int32( RogueTokenList* THIS, RogueInt32 index_0 );
RogueClassToken* RogueTokenList__remove_last( RogueTokenList* THIS );
RogueString* RogueLineReader__type_name( RogueClassLineReader* THIS );
RogueLogical RogueLineReader__has_another( RogueClassLineReader* THIS );
RogueString* RogueLineReader__read( RogueClassLineReader* THIS );
RogueClassLineReader* RogueLineReader__init__CharacterReader( RogueClassLineReader* THIS, RogueClassCharacterReader* _auto_395_0 );
RogueClassLineReader* RogueLineReader__init__File( RogueClassLineReader* THIS, RogueClassFile* file_0 );
RogueClassLineReader* RogueLineReader__init__String( RogueClassLineReader* THIS, RogueString* string_0 );
RogueString* RogueLineReader__prepare_next( RogueClassLineReader* THIS );
RogueClassLineReader* RogueLineReader__init_object( RogueClassLineReader* THIS );
RogueString* RogueTypeParameterList__to_String( RogueTypeParameterList* THIS );
RogueString* RogueTypeParameterList__type_name( RogueTypeParameterList* THIS );
RogueTypeParameterList* RogueTypeParameterList__init_object( RogueTypeParameterList* THIS );
RogueTypeParameterList* RogueTypeParameterList__init( RogueTypeParameterList* THIS );
RogueTypeParameterList* RogueTypeParameterList__init__Int32( RogueTypeParameterList* THIS, RogueInt32 initial_capacity_0 );
RogueTypeParameterList* RogueTypeParameterList__add__TypeParameter( RogueTypeParameterList* THIS, RogueClassTypeParameter* value_0 );
RogueInt32 RogueTypeParameterList__capacity( RogueTypeParameterList* THIS );
RogueTypeParameterList* RogueTypeParameterList__reserve__Int32( RogueTypeParameterList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueTypeParameter__type_name( RogueClassTypeParameter* THIS );
RogueClassTypeParameter* RogueTypeParameter__init__Token_String( RogueClassTypeParameter* THIS, RogueClassToken* _auto_439_0, RogueString* _auto_440_1 );
RogueClassTypeParameter* RogueTypeParameter__init_object( RogueClassTypeParameter* THIS );
RogueString* RogueAugmentList__to_String( RogueAugmentList* THIS );
RogueString* RogueAugmentList__type_name( RogueAugmentList* THIS );
RogueAugmentList* RogueAugmentList__init_object( RogueAugmentList* THIS );
RogueAugmentList* RogueAugmentList__init( RogueAugmentList* THIS );
RogueAugmentList* RogueAugmentList__init__Int32( RogueAugmentList* THIS, RogueInt32 initial_capacity_0 );
RogueAugmentList* RogueAugmentList__add__Augment( RogueAugmentList* THIS, RogueClassAugment* value_0 );
RogueInt32 RogueAugmentList__capacity( RogueAugmentList* THIS );
RogueAugmentList* RogueAugmentList__reserve__Int32( RogueAugmentList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueAugment__type_name( RogueClassAugment* THIS );
RogueClassAugment* RogueAugment__init__Token_String( RogueClassAugment* THIS, RogueClassToken* _auto_445_0, RogueString* _auto_446_1 );
RogueClassAugment* RogueAugment__init_object( RogueClassAugment* THIS );
RogueString* RogueAugmentArray__type_name( RogueArray* THIS );
RogueString* RogueString_TokenTypeTable__to_String( RogueClassString_TokenTypeTable* THIS );
RogueString* RogueString_TokenTypeTable__type_name( RogueClassString_TokenTypeTable* THIS );
RogueClassString_TokenTypeTable* RogueString_TokenTypeTable__init( RogueClassString_TokenTypeTable* THIS );
RogueClassString_TokenTypeTable* RogueString_TokenTypeTable__init__Int32( RogueClassString_TokenTypeTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_TokenTypeTableEntry* RogueString_TokenTypeTable__find__String( RogueClassString_TokenTypeTable* THIS, RogueString* key_0 );
RogueClassTokenType* RogueString_TokenTypeTable__get__String( RogueClassString_TokenTypeTable* THIS, RogueString* key_0 );
RogueClassString_TokenTypeTable* RogueString_TokenTypeTable__set__String_TokenType( RogueClassString_TokenTypeTable* THIS, RogueString* key_0, RogueClassTokenType* value_1 );
RogueStringBuilder* RogueString_TokenTypeTable__print_to__StringBuilder( RogueClassString_TokenTypeTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_TokenTypeTable* RogueString_TokenTypeTable__init_object( RogueClassString_TokenTypeTable* THIS );
RogueString* RogueLiteralCharacterToken__to_String( RogueClassLiteralCharacterToken* THIS );
RogueString* RogueLiteralCharacterToken__type_name( RogueClassLiteralCharacterToken* THIS );
RogueCharacter RogueLiteralCharacterToken__to_Character( RogueClassLiteralCharacterToken* THIS );
RogueClassLiteralCharacterToken* RogueLiteralCharacterToken__init_object( RogueClassLiteralCharacterToken* THIS );
RogueClassLiteralCharacterToken* RogueLiteralCharacterToken__init__TokenType_Character( RogueClassLiteralCharacterToken* THIS, RogueClassTokenType* _auto_453_0, RogueCharacter _auto_454_1 );
RogueString* RogueLiteralInt64Token__to_String( RogueClassLiteralInt64Token* THIS );
RogueString* RogueLiteralInt64Token__type_name( RogueClassLiteralInt64Token* THIS );
RogueInt32 RogueLiteralInt64Token__to_Int32( RogueClassLiteralInt64Token* THIS );
RogueInt64 RogueLiteralInt64Token__to_Int64( RogueClassLiteralInt64Token* THIS );
RogueReal64 RogueLiteralInt64Token__to_Real64( RogueClassLiteralInt64Token* THIS );
RogueClassLiteralInt64Token* RogueLiteralInt64Token__init_object( RogueClassLiteralInt64Token* THIS );
RogueClassLiteralInt64Token* RogueLiteralInt64Token__init__TokenType_Int64( RogueClassLiteralInt64Token* THIS, RogueClassTokenType* _auto_455_0, RogueInt64 _auto_456_1 );
RogueString* RogueLiteralInt32Token__to_String( RogueClassLiteralInt32Token* THIS );
RogueString* RogueLiteralInt32Token__type_name( RogueClassLiteralInt32Token* THIS );
RogueInt32 RogueLiteralInt32Token__to_Int32( RogueClassLiteralInt32Token* THIS );
RogueReal64 RogueLiteralInt32Token__to_Real64( RogueClassLiteralInt32Token* THIS );
RogueClassLiteralInt32Token* RogueLiteralInt32Token__init_object( RogueClassLiteralInt32Token* THIS );
RogueClassLiteralInt32Token* RogueLiteralInt32Token__init__TokenType_Int32( RogueClassLiteralInt32Token* THIS, RogueClassTokenType* _auto_457_0, RogueInt32 _auto_458_1 );
RogueString* RogueLiteralReal64Token__to_String( RogueClassLiteralReal64Token* THIS );
RogueString* RogueLiteralReal64Token__type_name( RogueClassLiteralReal64Token* THIS );
RogueInt32 RogueLiteralReal64Token__to_Int32( RogueClassLiteralReal64Token* THIS );
RogueReal64 RogueLiteralReal64Token__to_Real64( RogueClassLiteralReal64Token* THIS );
RogueClassLiteralReal64Token* RogueLiteralReal64Token__init_object( RogueClassLiteralReal64Token* THIS );
RogueClassLiteralReal64Token* RogueLiteralReal64Token__init__TokenType_Real64( RogueClassLiteralReal64Token* THIS, RogueClassTokenType* _auto_459_0, RogueReal64 _auto_460_1 );
RogueString* RogueStringDataToken__to_String( RogueClassStringDataToken* THIS );
RogueString* RogueStringDataToken__type_name( RogueClassStringDataToken* THIS );
RogueClassStringDataToken* RogueStringDataToken__init_object( RogueClassStringDataToken* THIS );
RogueClassStringDataToken* RogueStringDataToken__init__TokenType_String( RogueClassStringDataToken* THIS, RogueClassTokenType* _auto_461_0, RogueString* _auto_462_1 );
RogueString* RogueTokenArray__type_name( RogueArray* THIS );
RogueString* RogueString_TypeSpecializerTable__to_String( RogueClassString_TypeSpecializerTable* THIS );
RogueString* RogueString_TypeSpecializerTable__type_name( RogueClassString_TypeSpecializerTable* THIS );
RogueClassString_TypeSpecializerTable* RogueString_TypeSpecializerTable__init( RogueClassString_TypeSpecializerTable* THIS );
RogueClassString_TypeSpecializerTable* RogueString_TypeSpecializerTable__init__Int32( RogueClassString_TypeSpecializerTable* THIS, RogueInt32 bin_count_0 );
RogueInt32 RogueString_TypeSpecializerTable__count( RogueClassString_TypeSpecializerTable* THIS );
RogueClassString_TypeSpecializerTableEntry* RogueString_TypeSpecializerTable__find__String( RogueClassString_TypeSpecializerTable* THIS, RogueString* key_0 );
RogueClassTypeSpecializer* RogueString_TypeSpecializerTable__get__String( RogueClassString_TypeSpecializerTable* THIS, RogueString* key_0 );
RogueClassString_TypeSpecializerTable* RogueString_TypeSpecializerTable__set__String_TypeSpecializer( RogueClassString_TypeSpecializerTable* THIS, RogueString* key_0, RogueClassTypeSpecializer* value_1 );
RogueStringBuilder* RogueString_TypeSpecializerTable__print_to__StringBuilder( RogueClassString_TypeSpecializerTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_TypeSpecializerTable* RogueString_TypeSpecializerTable__init_object( RogueClassString_TypeSpecializerTable* THIS );
RogueString* RogueTypeParameterArray__type_name( RogueArray* THIS );
RogueString* RogueTypeSpecializer__type_name( RogueClassTypeSpecializer* THIS );
RogueClassTypeSpecializer* RogueTypeSpecializer__init__String_Int32( RogueClassTypeSpecializer* THIS, RogueString* _auto_474_0, RogueInt32 _auto_475_1 );
RogueClassTypeSpecializer* RogueTypeSpecializer__init_object( RogueClassTypeSpecializer* THIS );
RogueString* RogueTypeSpecializerList__to_String( RogueTypeSpecializerList* THIS );
RogueString* RogueTypeSpecializerList__type_name( RogueTypeSpecializerList* THIS );
RogueTypeSpecializerList* RogueTypeSpecializerList__init_object( RogueTypeSpecializerList* THIS );
RogueTypeSpecializerList* RogueTypeSpecializerList__init( RogueTypeSpecializerList* THIS );
RogueTypeSpecializerList* RogueTypeSpecializerList__init__Int32( RogueTypeSpecializerList* THIS, RogueInt32 initial_capacity_0 );
RogueTypeSpecializerList* RogueTypeSpecializerList__add__TypeSpecializer( RogueTypeSpecializerList* THIS, RogueClassTypeSpecializer* value_0 );
RogueInt32 RogueTypeSpecializerList__capacity( RogueTypeSpecializerList* THIS );
RogueTypeSpecializerList* RogueTypeSpecializerList__reserve__Int32( RogueTypeSpecializerList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TemplateTableEntryList__to_String( RogueTableEntry_of_String_TemplateList* THIS );
RogueString* RogueString_TemplateTableEntryList__type_name( RogueTableEntry_of_String_TemplateList* THIS );
RogueTableEntry_of_String_TemplateList* RogueString_TemplateTableEntryList__init_object( RogueTableEntry_of_String_TemplateList* THIS );
RogueTableEntry_of_String_TemplateList* RogueString_TemplateTableEntryList__init__Int32_String_TemplateTableEntry( RogueTableEntry_of_String_TemplateList* THIS, RogueInt32 initial_capacity_0, RogueClassString_TemplateTableEntry* initial_value_1 );
RogueTableEntry_of_String_TemplateList* RogueString_TemplateTableEntryList__add__String_TemplateTableEntry( RogueTableEntry_of_String_TemplateList* THIS, RogueClassString_TemplateTableEntry* value_0 );
RogueInt32 RogueString_TemplateTableEntryList__capacity( RogueTableEntry_of_String_TemplateList* THIS );
RogueTableEntry_of_String_TemplateList* RogueString_TemplateTableEntryList__reserve__Int32( RogueTableEntry_of_String_TemplateList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TemplateTableEntry__type_name( RogueClassString_TemplateTableEntry* THIS );
RogueClassString_TemplateTableEntry* RogueString_TemplateTableEntry__init__String_Template_Int32( RogueClassString_TemplateTableEntry* THIS, RogueString* _key_0, RogueClassTemplate* _value_1, RogueInt32 _hash_2 );
RogueClassString_TemplateTableEntry* RogueString_TemplateTableEntry__init_object( RogueClassString_TemplateTableEntry* THIS );
RogueString* RogueString_TemplateTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_AugmentListTableEntryList__to_String( RogueTableEntry_of_String_AugmentListList* THIS );
RogueString* RogueString_AugmentListTableEntryList__type_name( RogueTableEntry_of_String_AugmentListList* THIS );
RogueTableEntry_of_String_AugmentListList* RogueString_AugmentListTableEntryList__init_object( RogueTableEntry_of_String_AugmentListList* THIS );
RogueTableEntry_of_String_AugmentListList* RogueString_AugmentListTableEntryList__init__Int32_String_AugmentListTableEntry( RogueTableEntry_of_String_AugmentListList* THIS, RogueInt32 initial_capacity_0, RogueClassString_AugmentListTableEntry* initial_value_1 );
RogueTableEntry_of_String_AugmentListList* RogueString_AugmentListTableEntryList__add__String_AugmentListTableEntry( RogueTableEntry_of_String_AugmentListList* THIS, RogueClassString_AugmentListTableEntry* value_0 );
RogueInt32 RogueString_AugmentListTableEntryList__capacity( RogueTableEntry_of_String_AugmentListList* THIS );
RogueTableEntry_of_String_AugmentListList* RogueString_AugmentListTableEntryList__reserve__Int32( RogueTableEntry_of_String_AugmentListList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_AugmentListTableEntry__type_name( RogueClassString_AugmentListTableEntry* THIS );
RogueClassString_AugmentListTableEntry* RogueString_AugmentListTableEntry__init__String_AugmentList_Int32( RogueClassString_AugmentListTableEntry* THIS, RogueString* _key_0, RogueAugmentList* _value_1, RogueInt32 _hash_2 );
RogueClassString_AugmentListTableEntry* RogueString_AugmentListTableEntry__init_object( RogueClassString_AugmentListTableEntry* THIS );
RogueString* RogueString_AugmentListTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueCmdLabelList__to_String( RogueCmdLabelList* THIS );
RogueString* RogueCmdLabelList__type_name( RogueCmdLabelList* THIS );
RogueCmdLabelList* RogueCmdLabelList__init_object( RogueCmdLabelList* THIS );
RogueCmdLabelList* RogueCmdLabelList__init( RogueCmdLabelList* THIS );
RogueCmdLabelList* RogueCmdLabelList__init__Int32( RogueCmdLabelList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdLabelList* RogueCmdLabelList__add__CmdLabel( RogueCmdLabelList* THIS, RogueClassCmdLabel* value_0 );
RogueInt32 RogueCmdLabelList__capacity( RogueCmdLabelList* THIS );
RogueCmdLabelList* RogueCmdLabelList__clear( RogueCmdLabelList* THIS );
RogueCmdLabelList* RogueCmdLabelList__reserve__Int32( RogueCmdLabelList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_CmdLabelTable__to_String( RogueClassString_CmdLabelTable* THIS );
RogueString* RogueString_CmdLabelTable__type_name( RogueClassString_CmdLabelTable* THIS );
RogueClassString_CmdLabelTable* RogueString_CmdLabelTable__init( RogueClassString_CmdLabelTable* THIS );
RogueClassString_CmdLabelTable* RogueString_CmdLabelTable__init__Int32( RogueClassString_CmdLabelTable* THIS, RogueInt32 bin_count_0 );
void RogueString_CmdLabelTable__clear( RogueClassString_CmdLabelTable* THIS );
RogueLogical RogueString_CmdLabelTable__contains__String( RogueClassString_CmdLabelTable* THIS, RogueString* key_0 );
RogueClassString_CmdLabelTableEntry* RogueString_CmdLabelTable__find__String( RogueClassString_CmdLabelTable* THIS, RogueString* key_0 );
RogueClassCmdLabel* RogueString_CmdLabelTable__get__String( RogueClassString_CmdLabelTable* THIS, RogueString* key_0 );
RogueClassString_CmdLabelTable* RogueString_CmdLabelTable__set__String_CmdLabel( RogueClassString_CmdLabelTable* THIS, RogueString* key_0, RogueClassCmdLabel* value_1 );
RogueStringBuilder* RogueString_CmdLabelTable__print_to__StringBuilder( RogueClassString_CmdLabelTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_CmdLabelTable* RogueString_CmdLabelTable__init_object( RogueClassString_CmdLabelTable* THIS );
RogueString* RogueCloneArgs__type_name( RogueClassCloneArgs* THIS );
RogueClassCmdLabel* RogueCloneArgs__register_label__CmdLabel( RogueClassCloneArgs* THIS, RogueClassCmdLabel* label_0 );
RogueClassCloneArgs* RogueCloneArgs__init_object( RogueClassCloneArgs* THIS );
RogueString* RogueCloneMethodArgs__type_name( RogueClassCloneMethodArgs* THIS );
RogueClassCmdLabel* RogueCloneMethodArgs__register_label__CmdLabel( RogueClassCloneMethodArgs* THIS, RogueClassCmdLabel* label_0 );
RogueClassCloneMethodArgs* RogueCloneMethodArgs__init_object( RogueClassCloneMethodArgs* THIS );
RogueClassCloneMethodArgs* RogueCloneMethodArgs__init__Method( RogueClassCloneMethodArgs* THIS, RogueClassMethod* _auto_537_0 );
RogueString* RogueCmdAccess__type_name( RogueClassCmdAccess* THIS );
RogueClassCmd* RogueCmdAccess__clone__CloneArgs( RogueClassCmdAccess* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdAccess__implicit_type__Scope( RogueClassCmdAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdAccess__resolve__Scope( RogueClassCmdAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdAccess__resolve_assignment__Scope_Cmd( RogueClassCmdAccess* THIS, RogueClassScope* scope_0, RogueClassCmd* new_value_1 );
RogueClassCmd* RogueCmdAccess__resolve_modify_and_assign__Scope_TokenType_Cmd( RogueClassCmdAccess* THIS, RogueClassScope* scope_0, RogueClassTokenType* op_1, RogueClassCmd* new_value_2 );
RogueClassType* Rogue_CmdAccess__type( RogueClassCmdAccess* THIS );
void RogueCmdAccess__write_cpp__CPPWriter_Logical( RogueClassCmdAccess* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdAccess__trace_used_code( RogueClassCmdAccess* THIS );
void RogueCmdAccess__update_this_type__Scope( RogueClassCmdAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmdAccess* RogueCmdAccess__init_object( RogueClassCmdAccess* THIS );
RogueClassCmdAccess* RogueCmdAccess__init__Token_String( RogueClassCmdAccess* THIS, RogueClassToken* _auto_542_0, RogueString* _auto_543_1 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_String_CmdArgs( RogueClassCmdAccess* THIS, RogueClassToken* _auto_544_0, RogueString* _auto_545_1, RogueClassCmdArgs* _auto_546_2 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_String_CmdArgs_CmdFlagArgList( RogueClassCmdAccess* THIS, RogueClassToken* _auto_547_0, RogueString* _auto_548_1, RogueClassCmdArgs* _auto_549_2, RogueCmdFlagArgList* _auto_550_3 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_Cmd_String( RogueClassCmdAccess* THIS, RogueClassToken* _auto_551_0, RogueClassCmd* _auto_552_1, RogueString* _auto_553_2 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_Cmd_String_CmdArgs( RogueClassCmdAccess* THIS, RogueClassToken* _auto_554_0, RogueClassCmd* _auto_555_1, RogueString* _auto_556_2, RogueClassCmdArgs* _auto_557_3 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_Cmd_String_CmdArgs_CmdFlagArgList( RogueClassCmdAccess* THIS, RogueClassToken* _auto_558_0, RogueClassCmd* _auto_559_1, RogueString* _auto_560_2, RogueClassCmdArgs* _auto_561_3, RogueCmdFlagArgList* _auto_562_4 );
RogueClassCmdAccess* RogueCmdAccess__init__Token_Cmd_String_Cmd( RogueClassCmdAccess* THIS, RogueClassToken* _auto_563_0, RogueClassCmd* _auto_564_1, RogueString* _auto_565_2, RogueClassCmd* arg_3 );
void RogueCmdAccess__check_for_recursive_getter__Scope( RogueClassCmdAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdAccess__expand_modify_and_assign__Scope_TokenType_Cmd( RogueClassCmdAccess* THIS, RogueClassScope* scope_0, RogueClassTokenType* op_1, RogueClassCmd* new_value_2 );
RogueString* RogueCmdArgs__type_name( RogueClassCmdArgs* THIS );
RogueClassCmdArgs* RogueCmdArgs__init_object( RogueClassCmdArgs* THIS );
RogueClassCmdArgs* RogueCmdArgs__init( RogueClassCmdArgs* THIS );
RogueClassCmdArgs* RogueCmdArgs__init__Int32( RogueClassCmdArgs* THIS, RogueInt32 initial_capacity_0 );
RogueClassCmdArgs* RogueCmdArgs__init__Cmd( RogueClassCmdArgs* THIS, RogueClassCmd* arg_0 );
RogueClassCmdArgs* RogueCmdArgs__init__Cmd_Cmd( RogueClassCmdArgs* THIS, RogueClassCmd* arg1_0, RogueClassCmd* arg2_1 );
RogueClassCmdArgs* RogueCmdArgs__init__Cmd_Cmd_Cmd( RogueClassCmdArgs* THIS, RogueClassCmd* arg1_0, RogueClassCmd* arg2_1, RogueClassCmd* arg3_2 );
RogueClassCmdArgs* RogueCmdArgs__clone__CloneArgs( RogueClassCmdArgs* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdArgs__resolve__Scope( RogueClassCmdArgs* THIS, RogueClassScope* scope_0 );
void RogueCmdArgs__update_this_type__Scope( RogueClassCmdArgs* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdFlagArgList__to_String( RogueCmdFlagArgList* THIS );
RogueString* RogueCmdFlagArgList__type_name( RogueCmdFlagArgList* THIS );
RogueCmdFlagArgList* RogueCmdFlagArgList__init_object( RogueCmdFlagArgList* THIS );
RogueCmdFlagArgList* RogueCmdFlagArgList__init( RogueCmdFlagArgList* THIS );
RogueCmdFlagArgList* RogueCmdFlagArgList__init__Int32( RogueCmdFlagArgList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdFlagArgList* RogueCmdFlagArgList__add__CmdFlagArg( RogueCmdFlagArgList* THIS, RogueClassCmdFlagArg* value_0 );
RogueInt32 RogueCmdFlagArgList__capacity( RogueCmdFlagArgList* THIS );
RogueOptionalInt32 RogueCmdFlagArgList__locate__CmdFlagArg( RogueCmdFlagArgList* THIS, RogueClassCmdFlagArg* value_0 );
RogueCmdFlagArgList* RogueCmdFlagArgList__reserve__Int32( RogueCmdFlagArgList* THIS, RogueInt32 additional_count_0 );
RogueClassCmdFlagArg* RogueCmdFlagArgList__remove__CmdFlagArg( RogueCmdFlagArgList* THIS, RogueClassCmdFlagArg* value_0 );
RogueClassCmdFlagArg* RogueCmdFlagArgList__remove_at__Int32( RogueCmdFlagArgList* THIS, RogueInt32 index_0 );
RogueString* RogueCmdFlagArg__type_name( RogueClassCmdFlagArg* THIS );
RogueClassCmdFlagArg* RogueCmdFlagArg__clone__CloneArgs( RogueClassCmdFlagArg* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdFlagArg* RogueCmdFlagArg__init_object( RogueClassCmdFlagArg* THIS );
RogueClassCmdFlagArg* RogueCmdFlagArg__init__Token_String_Cmd_Logical( RogueClassCmdFlagArg* THIS, RogueClassToken* _auto_566_0, RogueString* _auto_567_1, RogueClassCmd* _auto_568_2, RogueLogical _auto_569_3 );
RogueString* RogueCmdAssign__type_name( RogueClassCmdAssign* THIS );
RogueClassCmd* RogueCmdAssign__clone__CloneArgs( RogueClassCmdAssign* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAssign__resolve__Scope( RogueClassCmdAssign* THIS, RogueClassScope* scope_0 );
void RogueCmdAssign__update_this_type__Scope( RogueClassCmdAssign* THIS, RogueClassScope* scope_0 );
RogueClassCmdAssign* RogueCmdAssign__init_object( RogueClassCmdAssign* THIS );
RogueClassCmdAssign* RogueCmdAssign__init__Token_Cmd_Cmd( RogueClassCmdAssign* THIS, RogueClassToken* _auto_574_0, RogueClassCmd* _auto_575_1, RogueClassCmd* _auto_576_2 );
RogueString* RogueCmdControlStructureList__to_String( RogueCmdControlStructureList* THIS );
RogueString* RogueCmdControlStructureList__type_name( RogueCmdControlStructureList* THIS );
RogueCmdControlStructureList* RogueCmdControlStructureList__init_object( RogueCmdControlStructureList* THIS );
RogueCmdControlStructureList* RogueCmdControlStructureList__init( RogueCmdControlStructureList* THIS );
RogueCmdControlStructureList* RogueCmdControlStructureList__init__Int32( RogueCmdControlStructureList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdControlStructureList* RogueCmdControlStructureList__add__CmdControlStructure( RogueCmdControlStructureList* THIS, RogueClassCmdControlStructure* value_0 );
RogueInt32 RogueCmdControlStructureList__capacity( RogueCmdControlStructureList* THIS );
RogueCmdControlStructureList* RogueCmdControlStructureList__reserve__Int32( RogueCmdControlStructureList* THIS, RogueInt32 additional_count_0 );
RogueClassCmdControlStructure* RogueCmdControlStructureList__remove_at__Int32( RogueCmdControlStructureList* THIS, RogueInt32 index_0 );
RogueClassCmdControlStructure* RogueCmdControlStructureList__remove_last( RogueCmdControlStructureList* THIS );
RogueString* RogueCmdControlStructure__type_name( RogueClassCmdControlStructure* THIS );
RogueLogical RogueCmdControlStructure__requires_semicolon( RogueClassCmdControlStructure* THIS );
RogueClassCmdControlStructure* RogueCmdControlStructure__init_object( RogueClassCmdControlStructure* THIS );
RogueClassCmd* RogueCmdControlStructure__set_control_logic__CmdControlStructure( RogueClassCmdControlStructure* THIS, RogueClassCmdControlStructure* control_structure_0 );
RogueString* RogueCmdLiteralThis__type_name( RogueClassCmdLiteralThis* THIS );
RogueClassCmd* RogueCmdLiteralThis__clone__CloneArgs( RogueClassCmdLiteralThis* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdLiteralThis__require_type_context( RogueClassCmdLiteralThis* THIS );
RogueClassCmd* RogueCmdLiteralThis__resolve__Scope( RogueClassCmdLiteralThis* THIS, RogueClassScope* scope_0 );
RogueClassCmdLiteralThis* RogueCmdLiteralThis__init_object( RogueClassCmdLiteralThis* THIS );
RogueString* RogueCmdThisContext__type_name( RogueClassCmdThisContext* THIS );
RogueClassCmd* RogueCmdThisContext__clone__CloneArgs( RogueClassCmdThisContext* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdThisContext__implicit_type__Scope( RogueClassCmdThisContext* THIS, RogueClassScope* scope_0 );
void RogueCmdThisContext__require_type_context( RogueClassCmdThisContext* THIS );
RogueClassCmd* RogueCmdThisContext__resolve__Scope( RogueClassCmdThisContext* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdThisContext__type( RogueClassCmdThisContext* THIS );
void RogueCmdThisContext__write_cpp__CPPWriter_Logical( RogueClassCmdThisContext* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdThisContext__trace_used_code( RogueClassCmdThisContext* THIS );
void RogueCmdThisContext__update_this_type__Scope( RogueClassCmdThisContext* THIS, RogueClassScope* scope_0 );
RogueClassCmdThisContext* RogueCmdThisContext__init_object( RogueClassCmdThisContext* THIS );
RogueClassCmdThisContext* RogueCmdThisContext__init__Token_Type( RogueClassCmdThisContext* THIS, RogueClassToken* _auto_586_0, RogueClassType* _auto_587_1 );
RogueString* RogueCmdLabelArray__type_name( RogueArray* THIS );
RogueString* RogueCmdGenericLoop__type_name( RogueClassCmdGenericLoop* THIS );
RogueClassCmd* RogueCmdGenericLoop__clone__CloneArgs( RogueClassCmdGenericLoop* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdGenericLoop__resolve__Scope( RogueClassCmdGenericLoop* THIS, RogueClassScope* scope_0 );
void RogueCmdGenericLoop__write_cpp__CPPWriter_Logical( RogueClassCmdGenericLoop* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdGenericLoop__trace_used_code( RogueClassCmdGenericLoop* THIS );
void RogueCmdGenericLoop__update_this_type__Scope( RogueClassCmdGenericLoop* THIS, RogueClassScope* scope_0 );
RogueClassCmdGenericLoop* RogueCmdGenericLoop__init_object( RogueClassCmdGenericLoop* THIS );
RogueClassCmdGenericLoop* RogueCmdGenericLoop__init__Token_Int32_Cmd_CmdStatementList_CmdStatementList_CmdStatementList( RogueClassCmdGenericLoop* THIS, RogueClassToken* _auto_590_0, RogueInt32 _auto_591_1, RogueClassCmd* _auto_592_2, RogueClassCmdStatementList* _auto_593_3, RogueClassCmdStatementList* _auto_594_4, RogueClassCmdStatementList* _auto_595_5 );
void RogueCmdGenericLoop__add_control_var__Local( RogueClassCmdGenericLoop* THIS, RogueClassLocal* v_0 );
void RogueCmdGenericLoop__add_upkeep__Cmd( RogueClassCmdGenericLoop* THIS, RogueClassCmd* cmd_0 );
RogueString* RogueCmdLiteralInt32__type_name( RogueClassCmdLiteralInt32* THIS );
RogueClassCmd* RogueCmdLiteralInt32__cast_to__Type_Scope( RogueClassCmdLiteralInt32* THIS, RogueClassType* target_type_0, RogueClassScope* scope_1 );
RogueClassCmd* RogueCmdLiteralInt32__clone__CloneArgs( RogueClassCmdLiteralInt32* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralInt32__resolve__Scope( RogueClassCmdLiteralInt32* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralInt32__type( RogueClassCmdLiteralInt32* THIS );
void RogueCmdLiteralInt32__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralInt32* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralInt32* RogueCmdLiteralInt32__init_object( RogueClassCmdLiteralInt32* THIS );
RogueClassCmdLiteralInt32* RogueCmdLiteralInt32__init__Token_Int32( RogueClassCmdLiteralInt32* THIS, RogueClassToken* _auto_601_0, RogueInt32 _auto_602_1 );
RogueString* RogueCmdLiteral__type_name( RogueClassCmdLiteral* THIS );
RogueClassType* RogueCmdLiteral__implicit_type__Scope( RogueClassCmdLiteral* THIS, RogueClassScope* scope_0 );
RogueLogical RogueCmdLiteral__is_literal( RogueClassCmdLiteral* THIS );
void RogueCmdLiteral__trace_used_code( RogueClassCmdLiteral* THIS );
void RogueCmdLiteral__update_this_type__Scope( RogueClassCmdLiteral* THIS, RogueClassScope* scope_0 );
RogueClassCmdLiteral* RogueCmdLiteral__init_object( RogueClassCmdLiteral* THIS );
RogueString* RogueCmdCompareNE__type_name( RogueClassCmdCompareNE* THIS );
RogueClassCmd* RogueCmdCompareNE__clone__CloneArgs( RogueClassCmdCompareNE* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareNE__combine_literal_operands__Type( RogueClassCmdCompareNE* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareNE* RogueCmdCompareNE__init_object( RogueClassCmdCompareNE* THIS );
RogueString* RogueCmdCompareNE__symbol( RogueClassCmdCompareNE* THIS );
RogueClassCmd* RogueCmdCompareNE__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareNE* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdComparison__type_name( RogueClassCmdComparison* THIS );
RogueClassType* Rogue_CmdComparison__type( RogueClassCmdComparison* THIS );
RogueClassCmdComparison* RogueCmdComparison__init_object( RogueClassCmdComparison* THIS );
RogueLogical RogueCmdComparison__requires_parens( RogueClassCmdComparison* THIS );
RogueClassCmd* RogueCmdComparison__resolve_for_types__Scope_Type_Type( RogueClassCmdComparison* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueClassCmd* RogueCmdComparison__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdComparison* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdBinary__type_name( RogueClassCmdBinary* THIS );
RogueClassCmd* RogueCmdBinary__resolve__Scope( RogueClassCmdBinary* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdBinary__type( RogueClassCmdBinary* THIS );
void RogueCmdBinary__write_cpp__CPPWriter_Logical( RogueClassCmdBinary* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdBinary__trace_used_code( RogueClassCmdBinary* THIS );
void RogueCmdBinary__update_this_type__Scope( RogueClassCmdBinary* THIS, RogueClassScope* scope_0 );
RogueClassCmdBinary* RogueCmdBinary__init_object( RogueClassCmdBinary* THIS );
RogueClassCmdBinary* RogueCmdBinary__init__Token_Cmd_Cmd( RogueClassCmdBinary* THIS, RogueClassToken* _auto_603_0, RogueClassCmd* _auto_604_1, RogueClassCmd* _auto_605_2 );
RogueString* RogueCmdBinary__fn_name( RogueClassCmdBinary* THIS );
RogueLogical RogueCmdBinary__requires_parens( RogueClassCmdBinary* THIS );
RogueClassCmd* RogueCmdBinary__resolve_for_types__Scope_Type_Type( RogueClassCmdBinary* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueClassCmd* RogueCmdBinary__resolve_for_common_type__Scope_Type( RogueClassCmdBinary* THIS, RogueClassScope* scope_0, RogueClassType* common_type_1 );
RogueClassCmd* RogueCmdBinary__resolve_operator_method__Scope_Type_Type( RogueClassCmdBinary* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueString* RogueCmdBinary__symbol( RogueClassCmdBinary* THIS );
RogueString* RogueCmdBinary__cpp_symbol( RogueClassCmdBinary* THIS );
RogueString* RogueTaskArgs__type_name( RogueClassTaskArgs* THIS );
RogueClassTaskArgs* RogueTaskArgs__init_object( RogueClassTaskArgs* THIS );
RogueClassTaskArgs* RogueTaskArgs__init__Type_Method_Type_Method( RogueClassTaskArgs* THIS, RogueClassType* _auto_612_0, RogueClassMethod* _auto_613_1, RogueClassType* _auto_614_2, RogueClassMethod* _auto_615_3 );
RogueClassTaskArgs* RogueTaskArgs__add__Cmd( RogueClassTaskArgs* THIS, RogueClassCmd* cmd_0 );
RogueClassTaskArgs* RogueTaskArgs__add_jump__Token_CmdTaskControlSection( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassCmdTaskControlSection* to_section_1 );
RogueClassTaskArgs* RogueTaskArgs__add_conditional_jump__Cmd_CmdTaskControlSection( RogueClassTaskArgs* THIS, RogueClassCmd* condition_0, RogueClassCmdTaskControlSection* to_section_1 );
RogueClassCmd* RogueTaskArgs__create_return__Token_Cmd( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassCmd* value_1 );
RogueClassCmd* RogueTaskArgs__create_escape__Token_CmdTaskControlSection( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassCmdTaskControlSection* escape_section_1 );
RogueClassTaskArgs* RogueTaskArgs__add_yield__Token( RogueClassTaskArgs* THIS, RogueClassToken* t_0 );
RogueClassCmdTaskControlSection* RogueTaskArgs__jump_to_new_section__Token( RogueClassTaskArgs* THIS, RogueClassToken* t_0 );
RogueClassTaskArgs* RogueTaskArgs__begin_section__CmdTaskControlSection( RogueClassTaskArgs* THIS, RogueClassCmdTaskControlSection* section_0 );
RogueClassCmdTaskControlSection* RogueTaskArgs__create_section( RogueClassTaskArgs* THIS );
RogueClassCmd* RogueTaskArgs__cmd_read_this__Token( RogueClassTaskArgs* THIS, RogueClassToken* t_0 );
RogueClassCmd* RogueTaskArgs__cmd_read_context__Token( RogueClassTaskArgs* THIS, RogueClassToken* t_0 );
RogueString* RogueTaskArgs__convert_local_name__Local( RogueClassTaskArgs* THIS, RogueClassLocal* local_info_0 );
RogueClassCmd* RogueTaskArgs__cmd_read__Token_Local( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassLocal* local_info_1 );
RogueClassCmd* RogueTaskArgs__cmd_write__Token_Local_Cmd( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassLocal* local_info_1, RogueClassCmd* new_value_2 );
RogueClassCmd* RogueTaskArgs__replace_write_local__Token_Local_Cmd( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassLocal* local_info_1, RogueClassCmd* new_value_2 );
RogueClassTaskArgs* RogueTaskArgs__set_next_ip__Token_CmdTaskControlSection( RogueClassTaskArgs* THIS, RogueClassToken* t_0, RogueClassCmdTaskControlSection* to_section_1 );
RogueString* RogueCmdArray__type_name( RogueArray* THIS );
RogueString* RogueCmdTaskControl__type_name( RogueClassCmdTaskControl* THIS );
RogueLogical RogueCmdTaskControl__requires_semicolon( RogueClassCmdTaskControl* THIS );
RogueClassCmd* RogueCmdTaskControl__resolve__Scope( RogueClassCmdTaskControl* THIS, RogueClassScope* scope_0 );
void RogueCmdTaskControl__write_cpp__CPPWriter_Logical( RogueClassCmdTaskControl* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdTaskControl__trace_used_code( RogueClassCmdTaskControl* THIS );
void RogueCmdTaskControl__update_this_type__Scope( RogueClassCmdTaskControl* THIS, RogueClassScope* scope_0 );
RogueClassCmdTaskControl* RogueCmdTaskControl__init_object( RogueClassCmdTaskControl* THIS );
RogueClassCmdTaskControl* RogueCmdTaskControl__init__Token( RogueClassCmdTaskControl* THIS, RogueClassToken* _auto_618_0 );
RogueClassCmdTaskControl* RogueCmdTaskControl__add__Cmd( RogueClassCmdTaskControl* THIS, RogueClassCmd* cmd_0 );
RogueString* RogueCmdTaskControlSection__type_name( RogueClassCmdTaskControlSection* THIS );
RogueClassCmdTaskControlSection* RogueCmdTaskControlSection__init__Int32( RogueClassCmdTaskControlSection* THIS, RogueInt32 _auto_619_0 );
RogueClassCmdTaskControlSection* RogueCmdTaskControlSection__init_object( RogueClassCmdTaskControlSection* THIS );
RogueString* RogueString_MethodListTableEntryList__to_String( RogueTableEntry_of_String_MethodListList* THIS );
RogueString* RogueString_MethodListTableEntryList__type_name( RogueTableEntry_of_String_MethodListList* THIS );
RogueTableEntry_of_String_MethodListList* RogueString_MethodListTableEntryList__init_object( RogueTableEntry_of_String_MethodListList* THIS );
RogueTableEntry_of_String_MethodListList* RogueString_MethodListTableEntryList__init__Int32_String_MethodListTableEntry( RogueTableEntry_of_String_MethodListList* THIS, RogueInt32 initial_capacity_0, RogueClassString_MethodListTableEntry* initial_value_1 );
RogueTableEntry_of_String_MethodListList* RogueString_MethodListTableEntryList__add__String_MethodListTableEntry( RogueTableEntry_of_String_MethodListList* THIS, RogueClassString_MethodListTableEntry* value_0 );
RogueInt32 RogueString_MethodListTableEntryList__capacity( RogueTableEntry_of_String_MethodListList* THIS );
RogueTableEntry_of_String_MethodListList* RogueString_MethodListTableEntryList__reserve__Int32( RogueTableEntry_of_String_MethodListList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_MethodListTableEntry__type_name( RogueClassString_MethodListTableEntry* THIS );
RogueClassString_MethodListTableEntry* RogueString_MethodListTableEntry__init__String_MethodList_Int32( RogueClassString_MethodListTableEntry* THIS, RogueString* _key_0, RogueMethodList* _value_1, RogueInt32 _hash_2 );
RogueClassString_MethodListTableEntry* RogueString_MethodListTableEntry__init_object( RogueClassString_MethodListTableEntry* THIS );
RogueString* RogueString_MethodListTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueDefinitionList__to_String( RogueDefinitionList* THIS );
RogueString* RogueDefinitionList__type_name( RogueDefinitionList* THIS );
RogueDefinitionList* RogueDefinitionList__init_object( RogueDefinitionList* THIS );
RogueDefinitionList* RogueDefinitionList__init( RogueDefinitionList* THIS );
RogueDefinitionList* RogueDefinitionList__init__Int32( RogueDefinitionList* THIS, RogueInt32 initial_capacity_0 );
RogueDefinitionList* RogueDefinitionList__add__Definition( RogueDefinitionList* THIS, RogueClassDefinition* value_0 );
RogueInt32 RogueDefinitionList__capacity( RogueDefinitionList* THIS );
RogueDefinitionList* RogueDefinitionList__reserve__Int32( RogueDefinitionList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueDefinition__type_name( RogueClassDefinition* THIS );
RogueClassDefinition* RogueDefinition__init__Token_String_Cmd_Logical( RogueClassDefinition* THIS, RogueClassToken* _auto_701_0, RogueString* _auto_702_1, RogueClassCmd* _auto_703_2, RogueLogical _auto_704_3 );
RogueClassDefinition* RogueDefinition__init_object( RogueClassDefinition* THIS );
RogueString* RogueString_DefinitionTable__to_String( RogueClassString_DefinitionTable* THIS );
RogueString* RogueString_DefinitionTable__type_name( RogueClassString_DefinitionTable* THIS );
RogueClassString_DefinitionTable* RogueString_DefinitionTable__init( RogueClassString_DefinitionTable* THIS );
RogueClassString_DefinitionTable* RogueString_DefinitionTable__init__Int32( RogueClassString_DefinitionTable* THIS, RogueInt32 bin_count_0 );
RogueLogical RogueString_DefinitionTable__contains__String( RogueClassString_DefinitionTable* THIS, RogueString* key_0 );
RogueClassString_DefinitionTableEntry* RogueString_DefinitionTable__find__String( RogueClassString_DefinitionTable* THIS, RogueString* key_0 );
RogueClassDefinition* RogueString_DefinitionTable__get__String( RogueClassString_DefinitionTable* THIS, RogueString* key_0 );
RogueClassString_DefinitionTable* RogueString_DefinitionTable__set__String_Definition( RogueClassString_DefinitionTable* THIS, RogueString* key_0, RogueClassDefinition* value_1 );
RogueStringBuilder* RogueString_DefinitionTable__print_to__StringBuilder( RogueClassString_DefinitionTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_DefinitionTable* RogueString_DefinitionTable__init_object( RogueClassString_DefinitionTable* THIS );
RogueString* RogueNativePropertyList__to_String( RogueNativePropertyList* THIS );
RogueString* RogueNativePropertyList__type_name( RogueNativePropertyList* THIS );
RogueNativePropertyList* RogueNativePropertyList__init_object( RogueNativePropertyList* THIS );
RogueNativePropertyList* RogueNativePropertyList__init( RogueNativePropertyList* THIS );
RogueNativePropertyList* RogueNativePropertyList__init__Int32( RogueNativePropertyList* THIS, RogueInt32 initial_capacity_0 );
RogueNativePropertyList* RogueNativePropertyList__add__NativeProperty( RogueNativePropertyList* THIS, RogueClassNativeProperty* value_0 );
RogueInt32 RogueNativePropertyList__capacity( RogueNativePropertyList* THIS );
RogueNativePropertyList* RogueNativePropertyList__reserve__Int32( RogueNativePropertyList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueNativeProperty__type_name( RogueClassNativeProperty* THIS );
RogueClassNativeProperty* RogueNativeProperty__init__Token_String( RogueClassNativeProperty* THIS, RogueClassToken* _auto_709_0, RogueString* _auto_710_1 );
RogueClassNativeProperty* RogueNativeProperty__init_object( RogueClassNativeProperty* THIS );
RogueString* RogueString_PropertyTable__to_String( RogueClassString_PropertyTable* THIS );
RogueString* RogueString_PropertyTable__type_name( RogueClassString_PropertyTable* THIS );
RogueClassString_PropertyTable* RogueString_PropertyTable__init( RogueClassString_PropertyTable* THIS );
RogueClassString_PropertyTable* RogueString_PropertyTable__init__Int32( RogueClassString_PropertyTable* THIS, RogueInt32 bin_count_0 );
void RogueString_PropertyTable__clear( RogueClassString_PropertyTable* THIS );
RogueClassString_PropertyTableEntry* RogueString_PropertyTable__find__String( RogueClassString_PropertyTable* THIS, RogueString* key_0 );
RogueClassProperty* RogueString_PropertyTable__get__String( RogueClassString_PropertyTable* THIS, RogueString* key_0 );
RogueClassString_PropertyTable* RogueString_PropertyTable__set__String_Property( RogueClassString_PropertyTable* THIS, RogueString* key_0, RogueClassProperty* value_1 );
RogueStringBuilder* RogueString_PropertyTable__print_to__StringBuilder( RogueClassString_PropertyTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_PropertyTable* RogueString_PropertyTable__init_object( RogueClassString_PropertyTable* THIS );
RogueString* RogueCmdLiteralNull__type_name( RogueClassCmdLiteralNull* THIS );
RogueClassCmd* RogueCmdLiteralNull__cast_to__Type_Scope( RogueClassCmdLiteralNull* THIS, RogueClassType* target_type_0, RogueClassScope* scope_1 );
RogueClassCmd* RogueCmdLiteralNull__clone__CloneArgs( RogueClassCmdLiteralNull* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdLiteralNull* RogueCmdLiteralNull__resolve__Scope( RogueClassCmdLiteralNull* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralNull__type( RogueClassCmdLiteralNull* THIS );
void RogueCmdLiteralNull__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralNull* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralNull* RogueCmdLiteralNull__init_object( RogueClassCmdLiteralNull* THIS );
RogueClassCmdLiteralNull* RogueCmdLiteralNull__init__Token( RogueClassCmdLiteralNull* THIS, RogueClassToken* _auto_717_0 );
RogueString* RogueCmdCreateCompound__type_name( RogueClassCmdCreateCompound* THIS );
RogueClassCmd* RogueCmdCreateCompound__clone__CloneArgs( RogueClassCmdCreateCompound* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateCompound__resolve__Scope( RogueClassCmdCreateCompound* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCreateCompound__type( RogueClassCmdCreateCompound* THIS );
void RogueCmdCreateCompound__write_cpp__CPPWriter_Logical( RogueClassCmdCreateCompound* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCreateCompound__trace_used_code( RogueClassCmdCreateCompound* THIS );
void RogueCmdCreateCompound__update_this_type__Scope( RogueClassCmdCreateCompound* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateCompound* RogueCmdCreateCompound__init_object( RogueClassCmdCreateCompound* THIS );
RogueClassCmdCreateCompound* RogueCmdCreateCompound__init__Token_Type_CmdArgs( RogueClassCmdCreateCompound* THIS, RogueClassToken* _auto_718_0, RogueClassType* _auto_719_1, RogueClassCmdArgs* _auto_720_2 );
RogueString* RogueCmdLiteralLogical__type_name( RogueClassCmdLiteralLogical* THIS );
RogueClassCmd* RogueCmdLiteralLogical__clone__CloneArgs( RogueClassCmdLiteralLogical* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralLogical__resolve__Scope( RogueClassCmdLiteralLogical* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralLogical__type( RogueClassCmdLiteralLogical* THIS );
void RogueCmdLiteralLogical__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralLogical* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralLogical* RogueCmdLiteralLogical__init_object( RogueClassCmdLiteralLogical* THIS );
RogueClassCmdLiteralLogical* RogueCmdLiteralLogical__init__Token_Logical( RogueClassCmdLiteralLogical* THIS, RogueClassToken* _auto_721_0, RogueLogical _auto_722_1 );
RogueString* RogueCmdLiteralString__type_name( RogueClassCmdLiteralString* THIS );
RogueClassCmd* RogueCmdLiteralString__clone__CloneArgs( RogueClassCmdLiteralString* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralString__resolve__Scope( RogueClassCmdLiteralString* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralString__type( RogueClassCmdLiteralString* THIS );
void RogueCmdLiteralString__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralString* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdLiteralString__trace_used_code( RogueClassCmdLiteralString* THIS );
void RogueCmdLiteralString__update_this_type__Scope( RogueClassCmdLiteralString* THIS, RogueClassScope* scope_0 );
RogueClassCmdLiteralString* RogueCmdLiteralString__init_object( RogueClassCmdLiteralString* THIS );
RogueClassCmdLiteralString* RogueCmdLiteralString__init__Token_String_Int32( RogueClassCmdLiteralString* THIS, RogueClassToken* _auto_733_0, RogueString* _auto_734_1, RogueInt32 _auto_735_2 );
RogueString* RogueCmdWriteGlobal__type_name( RogueClassCmdWriteGlobal* THIS );
RogueClassCmd* RogueCmdWriteGlobal__clone__CloneArgs( RogueClassCmdWriteGlobal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWriteGlobal__resolve__Scope( RogueClassCmdWriteGlobal* THIS, RogueClassScope* scope_0 );
void RogueCmdWriteGlobal__write_cpp__CPPWriter_Logical( RogueClassCmdWriteGlobal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdWriteGlobal__trace_used_code( RogueClassCmdWriteGlobal* THIS );
void RogueCmdWriteGlobal__update_this_type__Scope( RogueClassCmdWriteGlobal* THIS, RogueClassScope* scope_0 );
RogueClassCmdWriteGlobal* RogueCmdWriteGlobal__init_object( RogueClassCmdWriteGlobal* THIS );
RogueClassCmdWriteGlobal* RogueCmdWriteGlobal__init__Token_Property_Cmd( RogueClassCmdWriteGlobal* THIS, RogueClassToken* _auto_744_0, RogueClassProperty* _auto_745_1, RogueClassCmd* _auto_746_2 );
RogueString* RogueCmdWriteProperty__type_name( RogueClassCmdWriteProperty* THIS );
RogueClassCmd* RogueCmdWriteProperty__clone__CloneArgs( RogueClassCmdWriteProperty* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWriteProperty__resolve__Scope( RogueClassCmdWriteProperty* THIS, RogueClassScope* scope_0 );
void RogueCmdWriteProperty__write_cpp__CPPWriter_Logical( RogueClassCmdWriteProperty* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdWriteProperty__trace_used_code( RogueClassCmdWriteProperty* THIS );
void RogueCmdWriteProperty__update_this_type__Scope( RogueClassCmdWriteProperty* THIS, RogueClassScope* scope_0 );
RogueClassCmdWriteProperty* RogueCmdWriteProperty__init_object( RogueClassCmdWriteProperty* THIS );
RogueClassCmdWriteProperty* RogueCmdWriteProperty__init__Token_Cmd_Property_Cmd( RogueClassCmdWriteProperty* THIS, RogueClassToken* _auto_747_0, RogueClassCmd* _auto_748_1, RogueClassProperty* _auto_749_2, RogueClassCmd* _auto_750_3 );
RogueString* RogueDefinitionArray__type_name( RogueArray* THIS );
RogueString* RogueNativePropertyArray__type_name( RogueArray* THIS );
RogueString* RogueString_TypeTableEntryList__to_String( RogueTableEntry_of_String_TypeList* THIS );
RogueString* RogueString_TypeTableEntryList__type_name( RogueTableEntry_of_String_TypeList* THIS );
RogueTableEntry_of_String_TypeList* RogueString_TypeTableEntryList__init_object( RogueTableEntry_of_String_TypeList* THIS );
RogueTableEntry_of_String_TypeList* RogueString_TypeTableEntryList__init__Int32_String_TypeTableEntry( RogueTableEntry_of_String_TypeList* THIS, RogueInt32 initial_capacity_0, RogueClassString_TypeTableEntry* initial_value_1 );
RogueTableEntry_of_String_TypeList* RogueString_TypeTableEntryList__add__String_TypeTableEntry( RogueTableEntry_of_String_TypeList* THIS, RogueClassString_TypeTableEntry* value_0 );
RogueInt32 RogueString_TypeTableEntryList__capacity( RogueTableEntry_of_String_TypeList* THIS );
RogueTableEntry_of_String_TypeList* RogueString_TypeTableEntryList__reserve__Int32( RogueTableEntry_of_String_TypeList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TypeTableEntry__type_name( RogueClassString_TypeTableEntry* THIS );
RogueClassString_TypeTableEntry* RogueString_TypeTableEntry__init__String_Type_Int32( RogueClassString_TypeTableEntry* THIS, RogueString* _key_0, RogueClassType* _value_1, RogueInt32 _hash_2 );
RogueClassString_TypeTableEntry* RogueString_TypeTableEntry__init_object( RogueClassString_TypeTableEntry* THIS );
RogueString* RogueString_TypeTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_Int32TableEntryList__to_String( RogueTableEntry_of_String_Int32List* THIS );
RogueString* RogueString_Int32TableEntryList__type_name( RogueTableEntry_of_String_Int32List* THIS );
RogueTableEntry_of_String_Int32List* RogueString_Int32TableEntryList__init_object( RogueTableEntry_of_String_Int32List* THIS );
RogueTableEntry_of_String_Int32List* RogueString_Int32TableEntryList__init__Int32_String_Int32TableEntry( RogueTableEntry_of_String_Int32List* THIS, RogueInt32 initial_capacity_0, RogueClassString_Int32TableEntry* initial_value_1 );
RogueTableEntry_of_String_Int32List* RogueString_Int32TableEntryList__add__String_Int32TableEntry( RogueTableEntry_of_String_Int32List* THIS, RogueClassString_Int32TableEntry* value_0 );
RogueInt32 RogueString_Int32TableEntryList__capacity( RogueTableEntry_of_String_Int32List* THIS );
RogueTableEntry_of_String_Int32List* RogueString_Int32TableEntryList__reserve__Int32( RogueTableEntry_of_String_Int32List* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_Int32TableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_StringListTableEntryList__to_String( RogueTableEntry_of_String_StringListList* THIS );
RogueString* RogueString_StringListTableEntryList__type_name( RogueTableEntry_of_String_StringListList* THIS );
RogueTableEntry_of_String_StringListList* RogueString_StringListTableEntryList__init_object( RogueTableEntry_of_String_StringListList* THIS );
RogueTableEntry_of_String_StringListList* RogueString_StringListTableEntryList__init__Int32_String_StringListTableEntry( RogueTableEntry_of_String_StringListList* THIS, RogueInt32 initial_capacity_0, RogueClassString_StringListTableEntry* initial_value_1 );
RogueTableEntry_of_String_StringListList* RogueString_StringListTableEntryList__add__String_StringListTableEntry( RogueTableEntry_of_String_StringListList* THIS, RogueClassString_StringListTableEntry* value_0 );
RogueInt32 RogueString_StringListTableEntryList__capacity( RogueTableEntry_of_String_StringListList* THIS );
RogueTableEntry_of_String_StringListList* RogueString_StringListTableEntryList__reserve__Int32( RogueTableEntry_of_String_StringListList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_StringListTableEntry__type_name( RogueClassString_StringListTableEntry* THIS );
RogueClassString_StringListTableEntry* RogueString_StringListTableEntry__init__String_StringList_Int32( RogueClassString_StringListTableEntry* THIS, RogueString* _key_0, RogueStringList* _value_1, RogueInt32 _hash_2 );
RogueClassString_StringListTableEntry* RogueString_StringListTableEntry__init_object( RogueClassString_StringListTableEntry* THIS );
RogueString* RogueString_StringListTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueInt32_TemplateTableEntryList__to_String( RogueTableEntry_of_Int32_TemplateList* THIS );
RogueString* RogueInt32_TemplateTableEntryList__type_name( RogueTableEntry_of_Int32_TemplateList* THIS );
RogueTableEntry_of_Int32_TemplateList* RogueInt32_TemplateTableEntryList__init_object( RogueTableEntry_of_Int32_TemplateList* THIS );
RogueTableEntry_of_Int32_TemplateList* RogueInt32_TemplateTableEntryList__init__Int32_Int32_TemplateTableEntry( RogueTableEntry_of_Int32_TemplateList* THIS, RogueInt32 initial_capacity_0, RogueClassInt32_TemplateTableEntry* initial_value_1 );
RogueTableEntry_of_Int32_TemplateList* RogueInt32_TemplateTableEntryList__add__Int32_TemplateTableEntry( RogueTableEntry_of_Int32_TemplateList* THIS, RogueClassInt32_TemplateTableEntry* value_0 );
RogueInt32 RogueInt32_TemplateTableEntryList__capacity( RogueTableEntry_of_Int32_TemplateList* THIS );
RogueTableEntry_of_Int32_TemplateList* RogueInt32_TemplateTableEntryList__reserve__Int32( RogueTableEntry_of_Int32_TemplateList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueInt32_TemplateTableEntry__type_name( RogueClassInt32_TemplateTableEntry* THIS );
RogueClassInt32_TemplateTableEntry* RogueInt32_TemplateTableEntry__init__Int32_Template_Int32( RogueClassInt32_TemplateTableEntry* THIS, RogueInt32 _key_0, RogueClassTemplate* _value_1, RogueInt32 _hash_2 );
RogueClassInt32_TemplateTableEntry* RogueInt32_TemplateTableEntry__init_object( RogueClassInt32_TemplateTableEntry* THIS );
RogueString* RogueInt32_TemplateTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueCmdCastToType__type_name( RogueClassCmdCastToType* THIS );
RogueClassCmd* RogueCmdCastToType__clone__CloneArgs( RogueClassCmdCastToType* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCastToType__resolve__Scope( RogueClassCmdCastToType* THIS, RogueClassScope* scope_0 );
void RogueCmdCastToType__write_cpp__CPPWriter_Logical( RogueClassCmdCastToType* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCastToType* RogueCmdCastToType__init_object( RogueClassCmdCastToType* THIS );
RogueString* RogueCmdTypeOperator__type_name( RogueClassCmdTypeOperator* THIS );
RogueClassType* Rogue_CmdTypeOperator__type( RogueClassCmdTypeOperator* THIS );
void RogueCmdTypeOperator__trace_used_code( RogueClassCmdTypeOperator* THIS );
void RogueCmdTypeOperator__update_this_type__Scope( RogueClassCmdTypeOperator* THIS, RogueClassScope* scope_0 );
RogueClassCmdTypeOperator* RogueCmdTypeOperator__init_object( RogueClassCmdTypeOperator* THIS );
RogueClassCmdTypeOperator* RogueCmdTypeOperator__init__Token_Cmd_Type( RogueClassCmdTypeOperator* THIS, RogueClassToken* _auto_869_0, RogueClassCmd* _auto_870_1, RogueClassType* _auto_871_2 );
RogueString* RogueCmdLogicalize__type_name( RogueClassCmdLogicalize* THIS );
RogueClassCmd* RogueCmdLogicalize__clone__CloneArgs( RogueClassCmdLogicalize* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLogicalize__resolve__Scope( RogueClassCmdLogicalize* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLogicalize__type( RogueClassCmdLogicalize* THIS );
RogueClassCmdLogicalize* RogueCmdLogicalize__init_object( RogueClassCmdLogicalize* THIS );
RogueString* RogueCmdLogicalize__prefix_symbol( RogueClassCmdLogicalize* THIS );
RogueString* RogueCmdLogicalize__fn_name( RogueClassCmdLogicalize* THIS );
RogueClassCmd* RogueCmdLogicalize__resolve_for_literal_operand__Scope( RogueClassCmdLogicalize* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdLogicalize__suffix_symbol( RogueClassCmdLogicalize* THIS );
RogueString* RogueCmdLogicalize__cpp_prefix_symbol( RogueClassCmdLogicalize* THIS );
RogueString* RogueCmdLogicalize__cpp_suffix_symbol( RogueClassCmdLogicalize* THIS );
RogueString* RogueCmdUnary__type_name( RogueClassCmdUnary* THIS );
RogueClassCmd* RogueCmdUnary__resolve__Scope( RogueClassCmdUnary* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdUnary__type( RogueClassCmdUnary* THIS );
void RogueCmdUnary__write_cpp__CPPWriter_Logical( RogueClassCmdUnary* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdUnary__trace_used_code( RogueClassCmdUnary* THIS );
void RogueCmdUnary__update_this_type__Scope( RogueClassCmdUnary* THIS, RogueClassScope* scope_0 );
RogueClassCmdUnary* RogueCmdUnary__init_object( RogueClassCmdUnary* THIS );
RogueClassCmdUnary* RogueCmdUnary__init__Token_Cmd( RogueClassCmdUnary* THIS, RogueClassToken* _auto_872_0, RogueClassCmd* _auto_873_1 );
RogueString* RogueCmdUnary__prefix_symbol( RogueClassCmdUnary* THIS );
RogueString* RogueCmdUnary__fn_name( RogueClassCmdUnary* THIS );
RogueClassCmd* RogueCmdUnary__resolve_for_literal_operand__Scope( RogueClassCmdUnary* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdUnary__resolve_for_operand_type__Scope_Type( RogueClassCmdUnary* THIS, RogueClassScope* scope_0, RogueClassType* operand_type_1 );
RogueString* RogueCmdUnary__suffix_symbol( RogueClassCmdUnary* THIS );
RogueString* RogueCmdUnary__cpp_prefix_symbol( RogueClassCmdUnary* THIS );
RogueString* RogueCmdUnary__cpp_suffix_symbol( RogueClassCmdUnary* THIS );
RogueString* RogueCmdCreateOptionalValue__type_name( RogueClassCmdCreateOptionalValue* THIS );
RogueClassCmd* RogueCmdCreateOptionalValue__clone__CloneArgs( RogueClassCmdCreateOptionalValue* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateOptionalValue__resolve__Scope( RogueClassCmdCreateOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCreateOptionalValue__type( RogueClassCmdCreateOptionalValue* THIS );
void RogueCmdCreateOptionalValue__write_cpp__CPPWriter_Logical( RogueClassCmdCreateOptionalValue* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCreateOptionalValue__trace_used_code( RogueClassCmdCreateOptionalValue* THIS );
void RogueCmdCreateOptionalValue__update_this_type__Scope( RogueClassCmdCreateOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateOptionalValue* RogueCmdCreateOptionalValue__init_object( RogueClassCmdCreateOptionalValue* THIS );
RogueClassCmdCreateOptionalValue* RogueCmdCreateOptionalValue__init__Token_Type_Cmd( RogueClassCmdCreateOptionalValue* THIS, RogueClassToken* _auto_874_0, RogueClassType* _auto_875_1, RogueClassCmd* _auto_876_2 );
RogueString* RogueCmdReadSingleton__type_name( RogueClassCmdReadSingleton* THIS );
RogueClassCmd* RogueCmdReadSingleton__clone__CloneArgs( RogueClassCmdReadSingleton* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdReadSingleton__require_type_context( RogueClassCmdReadSingleton* THIS );
RogueClassCmd* RogueCmdReadSingleton__resolve__Scope( RogueClassCmdReadSingleton* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdReadSingleton__type( RogueClassCmdReadSingleton* THIS );
void RogueCmdReadSingleton__write_cpp__CPPWriter_Logical( RogueClassCmdReadSingleton* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadSingleton__trace_used_code( RogueClassCmdReadSingleton* THIS );
void RogueCmdReadSingleton__update_this_type__Scope( RogueClassCmdReadSingleton* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadSingleton* RogueCmdReadSingleton__init_object( RogueClassCmdReadSingleton* THIS );
RogueClassCmdReadSingleton* RogueCmdReadSingleton__init__Token_Type( RogueClassCmdReadSingleton* THIS, RogueClassToken* _auto_919_0, RogueClassType* _auto_920_1 );
RogueString* RogueCmdFlagArgArray__type_name( RogueArray* THIS );
RogueString* RogueCmdCallInlineNativeRoutine__type_name( RogueClassCmdCallInlineNativeRoutine* THIS );
RogueClassCmd* RogueCmdCallInlineNativeRoutine__clone__CloneArgs( RogueClassCmdCallInlineNativeRoutine* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* Rogue_CmdCallInlineNativeRoutine__type( RogueClassCmdCallInlineNativeRoutine* THIS );
RogueClassCmdCallInlineNativeRoutine* RogueCmdCallInlineNativeRoutine__init_object( RogueClassCmdCallInlineNativeRoutine* THIS );
RogueClassCmdCallInlineNativeRoutine* RogueCmdCallInlineNativeRoutine__init__Token_Method_CmdArgs( RogueClassCmdCallInlineNativeRoutine* THIS, RogueClassToken* _auto_930_0, RogueClassMethod* _auto_931_1, RogueClassCmdArgs* _auto_932_2 );
RogueString* RogueCmdCallInlineNative__to_String( RogueClassCmdCallInlineNative* THIS );
RogueString* RogueCmdCallInlineNative__type_name( RogueClassCmdCallInlineNative* THIS );
RogueClassCmd* RogueCmdCallInlineNative__resolve_adjust__Scope_Int32( RogueClassCmdCallInlineNative* THIS, RogueClassScope* scope_0, RogueInt32 _auto_929_1 );
void RogueCmdCallInlineNative__write_cpp__CPPWriter_Logical( RogueClassCmdCallInlineNative* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallInlineNative* RogueCmdCallInlineNative__init_object( RogueClassCmdCallInlineNative* THIS );
void RogueCmdCallInlineNative__print_this__CPPWriter( RogueClassCmdCallInlineNative* THIS, RogueClassCPPWriter* writer_0 );
RogueString* RogueCmdCall__type_name( RogueClassCmdCall* THIS );
RogueClassType* Rogue_CmdCall__require_type( RogueClassCmdCall* THIS );
RogueClassCmd* RogueCmdCall__resolve__Scope( RogueClassCmdCall* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCall__type( RogueClassCmdCall* THIS );
void RogueCmdCall__trace_used_code( RogueClassCmdCall* THIS );
void RogueCmdCall__update_this_type__Scope( RogueClassCmdCall* THIS, RogueClassScope* scope_0 );
RogueClassCmdCall* RogueCmdCall__init_object( RogueClassCmdCall* THIS );
RogueClassCmdCall* RogueCmdCall__init__Token_Cmd_Method_CmdArgs( RogueClassCmdCall* THIS, RogueClassToken* _auto_925_0, RogueClassCmd* _auto_926_1, RogueClassMethod* _auto_927_2, RogueClassCmdArgs* _auto_928_3 );
RogueString* RogueCmdCallNativeRoutine__type_name( RogueClassCmdCallNativeRoutine* THIS );
RogueClassCmd* RogueCmdCallNativeRoutine__clone__CloneArgs( RogueClassCmdCallNativeRoutine* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCallNativeRoutine__write_cpp__CPPWriter_Logical( RogueClassCmdCallNativeRoutine* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallNativeRoutine* RogueCmdCallNativeRoutine__init_object( RogueClassCmdCallNativeRoutine* THIS );
RogueClassCmdCallNativeRoutine* RogueCmdCallNativeRoutine__init__Token_Method_CmdArgs( RogueClassCmdCallNativeRoutine* THIS, RogueClassToken* _auto_933_0, RogueClassMethod* _auto_934_1, RogueClassCmdArgs* _auto_935_2 );
RogueString* RogueMacroArgs__type_name( RogueClassMacroArgs* THIS );
RogueClassMacroArgs* RogueMacroArgs__init_object( RogueClassMacroArgs* THIS );
RogueClassMacroArgs* RogueMacroArgs__init__Cmd_Method_CmdArgs( RogueClassMacroArgs* THIS, RogueClassCmd* _auto_936_0, RogueClassMethod* _auto_937_1, RogueClassCmdArgs* args_2 );
RogueClassCmd* RogueMacroArgs__inline_this( RogueClassMacroArgs* THIS );
RogueClassCmd* RogueMacroArgs__inline_access__CmdAccess( RogueClassMacroArgs* THIS, RogueClassCmdAccess* access_0 );
RogueClassCmd* RogueMacroArgs__inline_read_local__CmdReadLocal( RogueClassMacroArgs* THIS, RogueClassCmdReadLocal* read_cmd_0 );
RogueClassCmd* RogueMacroArgs__inline_write_local__CmdWriteLocal( RogueClassMacroArgs* THIS, RogueClassCmdWriteLocal* write_cmd_0 );
RogueString* RogueCmdInlineNative__type_name( RogueClassCmdInlineNative* THIS );
RogueClassCmdInlineNative* RogueCmdInlineNative__clone__CloneArgs( RogueClassCmdInlineNative* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdInlineNative__requires_semicolon( RogueClassCmdInlineNative* THIS );
RogueClassCmd* RogueCmdInlineNative__resolve__Scope( RogueClassCmdInlineNative* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdInlineNative__resolve_assignment__Scope_Cmd( RogueClassCmdInlineNative* THIS, RogueClassScope* scope_0, RogueClassCmd* _auto_943_1 );
RogueClassCmd* RogueCmdInlineNative__resolve_adjust__Scope_Int32( RogueClassCmdInlineNative* THIS, RogueClassScope* scope_0, RogueInt32 _auto_946_1 );
RogueClassCmd* RogueCmdInlineNative__resolve_modify_and_assign__Scope_TokenType_Cmd( RogueClassCmdInlineNative* THIS, RogueClassScope* scope_0, RogueClassTokenType* _auto_944_1, RogueClassCmd* _auto_945_2 );
RogueClassType* Rogue_CmdInlineNative__type( RogueClassCmdInlineNative* THIS );
void RogueCmdInlineNative__write_cpp__CPPWriter_Logical( RogueClassCmdInlineNative* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdInlineNative__trace_used_code( RogueClassCmdInlineNative* THIS );
void RogueCmdInlineNative__update_this_type__Scope( RogueClassCmdInlineNative* THIS, RogueClassScope* scope_0 );
RogueClassCmdInlineNative* RogueCmdInlineNative__init_object( RogueClassCmdInlineNative* THIS );
RogueClassCmdInlineNative* RogueCmdInlineNative__init__Token_String_Type( RogueClassCmdInlineNative* THIS, RogueClassToken* _auto_940_0, RogueString* _auto_941_1, RogueClassType* _auto_942_2 );
RogueString* RogueCmdCallRoutine__type_name( RogueClassCmdCallRoutine* THIS );
RogueClassCmd* RogueCmdCallRoutine__clone__CloneArgs( RogueClassCmdCallRoutine* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCallRoutine__write_cpp__CPPWriter_Logical( RogueClassCmdCallRoutine* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallRoutine* RogueCmdCallRoutine__init_object( RogueClassCmdCallRoutine* THIS );
RogueClassCmdCallRoutine* RogueCmdCallRoutine__init__Token_Method_CmdArgs( RogueClassCmdCallRoutine* THIS, RogueClassToken* _auto_947_0, RogueClassMethod* _auto_948_1, RogueClassCmdArgs* _auto_949_2 );
RogueString* RogueCmdReadArrayCount__type_name( RogueClassCmdReadArrayCount* THIS );
RogueClassCmd* RogueCmdReadArrayCount__clone__CloneArgs( RogueClassCmdReadArrayCount* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReadArrayCount__resolve__Scope( RogueClassCmdReadArrayCount* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdReadArrayCount__type( RogueClassCmdReadArrayCount* THIS );
void RogueCmdReadArrayCount__write_cpp__CPPWriter_Logical( RogueClassCmdReadArrayCount* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadArrayCount__trace_used_code( RogueClassCmdReadArrayCount* THIS );
void RogueCmdReadArrayCount__update_this_type__Scope( RogueClassCmdReadArrayCount* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadArrayCount* RogueCmdReadArrayCount__init_object( RogueClassCmdReadArrayCount* THIS );
RogueClassCmdReadArrayCount* RogueCmdReadArrayCount__init__Token_Cmd( RogueClassCmdReadArrayCount* THIS, RogueClassToken* _auto_950_0, RogueClassCmd* _auto_951_1 );
RogueString* RogueCmdCallInlineNativeMethod__type_name( RogueClassCmdCallInlineNativeMethod* THIS );
RogueClassCmd* RogueCmdCallInlineNativeMethod__clone__CloneArgs( RogueClassCmdCallInlineNativeMethod* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* Rogue_CmdCallInlineNativeMethod__type( RogueClassCmdCallInlineNativeMethod* THIS );
RogueClassCmdCallInlineNativeMethod* RogueCmdCallInlineNativeMethod__init_object( RogueClassCmdCallInlineNativeMethod* THIS );
void RogueCmdCallInlineNativeMethod__print_this__CPPWriter( RogueClassCmdCallInlineNativeMethod* THIS, RogueClassCPPWriter* writer_0 );
RogueString* RogueCmdCallNativeMethod__type_name( RogueClassCmdCallNativeMethod* THIS );
RogueClassCmd* RogueCmdCallNativeMethod__clone__CloneArgs( RogueClassCmdCallNativeMethod* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCallNativeMethod__write_cpp__CPPWriter_Logical( RogueClassCmdCallNativeMethod* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallNativeMethod* RogueCmdCallNativeMethod__init_object( RogueClassCmdCallNativeMethod* THIS );
RogueString* RogueCmdCallAspectMethod__type_name( RogueClassCmdCallAspectMethod* THIS );
RogueClassCmd* RogueCmdCallAspectMethod__clone__CloneArgs( RogueClassCmdCallAspectMethod* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCallAspectMethod__write_cpp__CPPWriter_Logical( RogueClassCmdCallAspectMethod* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallAspectMethod* RogueCmdCallAspectMethod__init_object( RogueClassCmdCallAspectMethod* THIS );
RogueString* RogueCmdCallDynamicMethod__type_name( RogueClassCmdCallDynamicMethod* THIS );
RogueClassCmd* RogueCmdCallDynamicMethod__clone__CloneArgs( RogueClassCmdCallDynamicMethod* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCallDynamicMethod__write_cpp__CPPWriter_Logical( RogueClassCmdCallDynamicMethod* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCallDynamicMethod__trace_used_code( RogueClassCmdCallDynamicMethod* THIS );
RogueClassCmdCallDynamicMethod* RogueCmdCallDynamicMethod__init_object( RogueClassCmdCallDynamicMethod* THIS );
RogueString* RogueCmdCallMethod__type_name( RogueClassCmdCallMethod* THIS );
RogueClassCmd* RogueCmdCallMethod__call_prior__Scope( RogueClassCmdCallMethod* THIS, RogueClassScope* scope_0 );
RogueClassCmdCallMethod* RogueCmdCallMethod__init_object( RogueClassCmdCallMethod* THIS );
RogueString* RogueCandidateMethods__type_name( RogueClassCandidateMethods* THIS );
RogueClassCandidateMethods* RogueCandidateMethods__init__Type_CmdAccess_Logical( RogueClassCandidateMethods* THIS, RogueClassType* _auto_955_0, RogueClassCmdAccess* _auto_956_1, RogueLogical _auto_957_2 );
RogueLogical RogueCandidateMethods__has_match( RogueClassCandidateMethods* THIS );
RogueClassMethod* RogueCandidateMethods__match( RogueClassCandidateMethods* THIS );
RogueLogical RogueCandidateMethods__refine_matches( RogueClassCandidateMethods* THIS );
RogueLogical RogueCandidateMethods__update_available( RogueClassCandidateMethods* THIS );
RogueLogical RogueCandidateMethods__update_matches( RogueClassCandidateMethods* THIS );
RogueLogical RogueCandidateMethods__update__Logical( RogueClassCandidateMethods* THIS, RogueLogical require_compatible_0 );
RogueClassCandidateMethods* RogueCandidateMethods__init_object( RogueClassCandidateMethods* THIS );
RogueString* RogueCmdCreateFunction__type_name( RogueClassCmdCreateFunction* THIS );
RogueClassCmdCreateFunction* RogueCmdCreateFunction__clone__CloneArgs( RogueClassCmdCreateFunction* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdCreateFunction__is_generic_function( RogueClassCmdCreateFunction* THIS );
RogueClassCmd* RogueCmdCreateFunction__resolve__Scope( RogueClassCmdCreateFunction* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCreateFunction__type( RogueClassCmdCreateFunction* THIS );
RogueClassCmdCreateFunction* RogueCmdCreateFunction__init_object( RogueClassCmdCreateFunction* THIS );
RogueClassCmdCreateFunction* RogueCmdCreateFunction__init__Token_FnParamList_Type_FnArgList_CmdStatementList_Logical( RogueClassCmdCreateFunction* THIS, RogueClassToken* _auto_984_0, RogueFnParamList* _auto_985_1, RogueClassType* _auto_986_2, RogueFnArgList* _auto_987_3, RogueClassCmdStatementList* _auto_988_4, RogueLogical _auto_989_5 );
RogueString* RogueFnParamList__to_String( RogueFnParamList* THIS );
RogueString* RogueFnParamList__type_name( RogueFnParamList* THIS );
RogueFnParamList* RogueFnParamList__init_object( RogueFnParamList* THIS );
RogueFnParamList* RogueFnParamList__init( RogueFnParamList* THIS );
RogueFnParamList* RogueFnParamList__init__Int32( RogueFnParamList* THIS, RogueInt32 initial_capacity_0 );
RogueFnParamList* RogueFnParamList__add__FnParam( RogueFnParamList* THIS, RogueClassFnParam* value_0 );
RogueInt32 RogueFnParamList__capacity( RogueFnParamList* THIS );
RogueClassFnParam* RogueFnParamList__last( RogueFnParamList* THIS );
RogueFnParamList* RogueFnParamList__reserve__Int32( RogueFnParamList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueFnParam__type_name( RogueClassFnParam* THIS );
RogueClassFnParam* RogueFnParam__init__String( RogueClassFnParam* THIS, RogueString* _auto_991_0 );
RogueClassFnParam* RogueFnParam__init_object( RogueClassFnParam* THIS );
RogueString* RogueFnParamArray__type_name( RogueArray* THIS );
RogueString* RogueTypeSpecializerArray__type_name( RogueArray* THIS );
RogueString* RogueString_MethodTableEntryList__to_String( RogueTableEntry_of_String_MethodList* THIS );
RogueString* RogueString_MethodTableEntryList__type_name( RogueTableEntry_of_String_MethodList* THIS );
RogueTableEntry_of_String_MethodList* RogueString_MethodTableEntryList__init_object( RogueTableEntry_of_String_MethodList* THIS );
RogueTableEntry_of_String_MethodList* RogueString_MethodTableEntryList__init__Int32_String_MethodTableEntry( RogueTableEntry_of_String_MethodList* THIS, RogueInt32 initial_capacity_0, RogueClassString_MethodTableEntry* initial_value_1 );
RogueTableEntry_of_String_MethodList* RogueString_MethodTableEntryList__add__String_MethodTableEntry( RogueTableEntry_of_String_MethodList* THIS, RogueClassString_MethodTableEntry* value_0 );
RogueInt32 RogueString_MethodTableEntryList__capacity( RogueTableEntry_of_String_MethodList* THIS );
RogueTableEntry_of_String_MethodList* RogueString_MethodTableEntryList__reserve__Int32( RogueTableEntry_of_String_MethodList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_MethodTableEntry__type_name( RogueClassString_MethodTableEntry* THIS );
RogueClassString_MethodTableEntry* RogueString_MethodTableEntry__init__String_Method_Int32( RogueClassString_MethodTableEntry* THIS, RogueString* _key_0, RogueClassMethod* _value_1, RogueInt32 _hash_2 );
RogueClassString_MethodTableEntry* RogueString_MethodTableEntry__init_object( RogueClassString_MethodTableEntry* THIS );
RogueString* RogueString_MethodTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_LogicalTableEntryList__to_String( RogueTableEntry_of_String_LogicalList* THIS );
RogueString* RogueString_LogicalTableEntryList__type_name( RogueTableEntry_of_String_LogicalList* THIS );
RogueTableEntry_of_String_LogicalList* RogueString_LogicalTableEntryList__init_object( RogueTableEntry_of_String_LogicalList* THIS );
RogueTableEntry_of_String_LogicalList* RogueString_LogicalTableEntryList__init__Int32_String_LogicalTableEntry( RogueTableEntry_of_String_LogicalList* THIS, RogueInt32 initial_capacity_0, RogueClassString_LogicalTableEntry* initial_value_1 );
RogueTableEntry_of_String_LogicalList* RogueString_LogicalTableEntryList__add__String_LogicalTableEntry( RogueTableEntry_of_String_LogicalList* THIS, RogueClassString_LogicalTableEntry* value_0 );
RogueInt32 RogueString_LogicalTableEntryList__capacity( RogueTableEntry_of_String_LogicalList* THIS );
RogueTableEntry_of_String_LogicalList* RogueString_LogicalTableEntryList__reserve__Int32( RogueTableEntry_of_String_LogicalList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_LogicalTableEntry__type_name( RogueClassString_LogicalTableEntry* THIS );
RogueClassString_LogicalTableEntry* RogueString_LogicalTableEntry__init__String_Logical_Int32( RogueClassString_LogicalTableEntry* THIS, RogueString* _key_0, RogueLogical _value_1, RogueInt32 _hash_2 );
RogueClassString_LogicalTableEntry* RogueString_LogicalTableEntry__init_object( RogueClassString_LogicalTableEntry* THIS );
RogueString* RogueString_LogicalTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueTokenReader__type_name( RogueClassTokenReader* THIS );
RogueClassTokenReader* RogueTokenReader__init__TokenList( RogueClassTokenReader* THIS, RogueTokenList* _auto_1202_0 );
RogueClassError* RogueTokenReader__error__String( RogueClassTokenReader* THIS, RogueString* message_0 );
RogueLogical RogueTokenReader__has_another( RogueClassTokenReader* THIS );
RogueLogical RogueTokenReader__next_is__TokenType( RogueClassTokenReader* THIS, RogueClassTokenType* type_0 );
RogueLogical RogueTokenReader__next_is_statement_token( RogueClassTokenReader* THIS );
RogueClassToken* RogueTokenReader__peek( RogueClassTokenReader* THIS );
RogueClassToken* RogueTokenReader__peek__Int32( RogueClassTokenReader* THIS, RogueInt32 num_ahead_0 );
RogueClassToken* RogueTokenReader__read( RogueClassTokenReader* THIS );
RogueClassTokenReader* RogueTokenReader__init_object( RogueClassTokenReader* THIS );
RogueString* RogueString_StringTable__to_String( RogueClassString_StringTable* THIS );
RogueString* RogueString_StringTable__type_name( RogueClassString_StringTable* THIS );
RogueClassString_StringTable* RogueString_StringTable__init( RogueClassString_StringTable* THIS );
RogueClassString_StringTable* RogueString_StringTable__init__Int32( RogueClassString_StringTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_StringTableEntry* RogueString_StringTable__find__String( RogueClassString_StringTable* THIS, RogueString* key_0 );
RogueString* RogueString_StringTable__get__String( RogueClassString_StringTable* THIS, RogueString* key_0 );
RogueClassString_StringTable* RogueString_StringTable__set__String_String( RogueClassString_StringTable* THIS, RogueString* key_0, RogueString* value_1 );
RogueStringBuilder* RogueString_StringTable__print_to__StringBuilder( RogueClassString_StringTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_StringTable* RogueString_StringTable__init_object( RogueClassString_StringTable* THIS );
RogueString* RogueTokenizer__type_name( RogueClassTokenizer* THIS );
RogueTokenList* RogueTokenizer__tokenize__String( RogueClassTokenizer* THIS, RogueString* _auto_1207_0 );
RogueTokenList* RogueTokenizer__tokenize__Token_String_String( RogueClassTokenizer* THIS, RogueClassToken* reference_t_0, RogueString* _auto_1208_1, RogueString* data_2 );
RogueTokenList* RogueTokenizer__tokenize__ParseReader( RogueClassTokenizer* THIS, RogueClassParseReader* _auto_1209_0 );
RogueLogical RogueTokenizer__add_new_string_or_character_token_from_buffer__Character( RogueClassTokenizer* THIS, RogueCharacter terminator_0 );
RogueLogical RogueTokenizer__add_new_token__TokenType( RogueClassTokenizer* THIS, RogueClassTokenType* type_0 );
RogueLogical RogueTokenizer__add_new_token__TokenType_Character( RogueClassTokenizer* THIS, RogueClassTokenType* type_0, RogueCharacter value_1 );
RogueLogical RogueTokenizer__add_new_token__TokenType_Int64( RogueClassTokenizer* THIS, RogueClassTokenType* type_0, RogueInt64 value_1 );
RogueLogical RogueTokenizer__add_new_token__TokenType_Int32( RogueClassTokenizer* THIS, RogueClassTokenType* type_0, RogueInt32 value_1 );
RogueLogical RogueTokenizer__add_new_token__TokenType_Real64( RogueClassTokenizer* THIS, RogueClassTokenType* type_0, RogueReal64 value_1 );
RogueLogical RogueTokenizer__add_new_token__TokenType_String( RogueClassTokenizer* THIS, RogueClassTokenType* type_0, RogueString* value_1 );
void RogueTokenizer__configure_token_types( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__consume__Character( RogueClassTokenizer* THIS, RogueCharacter ch_0 );
RogueLogical RogueTokenizer__consume__String( RogueClassTokenizer* THIS, RogueString* st_0 );
RogueLogical RogueTokenizer__consume_id__String( RogueClassTokenizer* THIS, RogueString* st_0 );
RogueLogical RogueTokenizer__consume_spaces( RogueClassTokenizer* THIS );
RogueClassTokenType* RogueTokenizer__define__TokenType( RogueClassTokenizer* THIS, RogueClassTokenType* type_0 );
RogueClassRogueError* RogueTokenizer__error__String( RogueClassTokenizer* THIS, RogueString* message_0 );
RogueClassTokenType* Rogue_Tokenizer__get_symbol_token_type( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__next_is_hex_digit( RogueClassTokenizer* THIS );
void RogueTokenizer__read_character( RogueClassTokenizer* THIS );
RogueInt32 RogueTokenizer__read_hex_value__Int32( RogueClassTokenizer* THIS, RogueInt32 digits_0 );
RogueString* RogueTokenizer__read_identifier( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__tokenize_alternate_string__Character( RogueClassTokenizer* THIS, RogueCharacter terminator_0 );
RogueLogical RogueTokenizer__tokenize_another( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__tokenize_comment( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__tokenize_integer_in_base__Int32( RogueClassTokenizer* THIS, RogueInt32 base_0 );
RogueLogical RogueTokenizer__tokenize_number( RogueClassTokenizer* THIS );
RogueReal64 RogueTokenizer__scan_real( RogueClassTokenizer* THIS );
RogueInt64 RogueTokenizer__scan_long( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__scan_native_code( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__scan_native_header( RogueClassTokenizer* THIS );
RogueLogical RogueTokenizer__tokenize_string__Character( RogueClassTokenizer* THIS, RogueCharacter terminator_0 );
RogueLogical RogueTokenizer__tokenize_verbatim_string( RogueClassTokenizer* THIS );
RogueClassTokenizer* RogueTokenizer__init_object( RogueClassTokenizer* THIS );
RogueString* RogueParseReader__type_name( RogueClassParseReader* THIS );
RogueLogical RogueParseReader__has_another( RogueClassParseReader* THIS );
RogueCharacter RogueParseReader__peek( RogueClassParseReader* THIS );
RogueCharacter RogueParseReader__read( RogueClassParseReader* THIS );
RogueClassParseReader* RogueParseReader__init__String_Int32( RogueClassParseReader* THIS, RogueString* source_0, RogueInt32 _auto_1214_1 );
RogueClassParseReader* RogueParseReader__init__File_Int32( RogueClassParseReader* THIS, RogueClassFile* file_0, RogueInt32 _auto_1215_1 );
RogueClassParseReader* RogueParseReader__init__CharacterList_Int32( RogueClassParseReader* THIS, RogueCharacterList* source_0, RogueInt32 _auto_1216_1 );
RogueLogical RogueParseReader__consume__Character( RogueClassParseReader* THIS, RogueCharacter ch_0 );
RogueLogical RogueParseReader__consume__String( RogueClassParseReader* THIS, RogueString* text_0 );
RogueLogical RogueParseReader__consume_id__String( RogueClassParseReader* THIS, RogueString* text_0 );
RogueLogical RogueParseReader__consume_spaces( RogueClassParseReader* THIS );
RogueLogical RogueParseReader__has_another__Int32( RogueClassParseReader* THIS, RogueInt32 n_0 );
RogueCharacter RogueParseReader__peek__Int32( RogueClassParseReader* THIS, RogueInt32 num_ahead_0 );
RogueClassParseReader* RogueParseReader__seek_location__Int32_Int32( RogueClassParseReader* THIS, RogueInt32 new_line_0, RogueInt32 new_column_1 );
RogueClassParseReader* RogueParseReader__set_location__Int32_Int32( RogueClassParseReader* THIS, RogueInt32 _auto_1217_0, RogueInt32 _auto_1218_1 );
RogueClassParseReader* RogueParseReader__init_object( RogueClassParseReader* THIS );
RogueString* RoguePreprocessor__type_name( RogueClassPreprocessor* THIS );
RogueClassPreprocessor* RoguePreprocessor__init__Parser( RogueClassPreprocessor* THIS, RogueClassParser* _auto_1219_0 );
RogueTokenList* RoguePreprocessor__process__TokenList( RogueClassPreprocessor* THIS, RogueTokenList* _auto_1220_0 );
RogueLogical RoguePreprocessor__consume__TokenType( RogueClassPreprocessor* THIS, RogueClassTokenType* type_0 );
void RoguePreprocessor__process__Logical_Int32_Logical( RogueClassPreprocessor* THIS, RogueLogical keep_tokens_0, RogueInt32 depth_1, RogueLogical stop_on_eol_2 );
void RoguePreprocessor__must_consume__TokenType( RogueClassPreprocessor* THIS, RogueClassTokenType* type_0 );
RogueLogical RoguePreprocessor__parse_logical_expression( RogueClassPreprocessor* THIS );
RogueLogical RoguePreprocessor__parse_logical_or( RogueClassPreprocessor* THIS );
RogueLogical RoguePreprocessor__parse_logical_or__Logical( RogueClassPreprocessor* THIS, RogueLogical lhs_0 );
RogueLogical RoguePreprocessor__parse_logical_and( RogueClassPreprocessor* THIS );
RogueLogical RoguePreprocessor__parse_logical_and__Logical( RogueClassPreprocessor* THIS, RogueLogical lhs_0 );
RogueLogical RoguePreprocessor__parse_logical_term( RogueClassPreprocessor* THIS );
RogueTokenList* RoguePreprocessor__reprocess__TokenList( RogueClassPreprocessor* THIS, RogueTokenList* _auto_1221_0 );
RogueClassPreprocessor* RoguePreprocessor__init_object( RogueClassPreprocessor* THIS );
RogueString* RogueEOLToken__to_String( RogueClassEOLToken* THIS );
RogueString* RogueEOLToken__type_name( RogueClassEOLToken* THIS );
RogueClassEOLToken* RogueEOLToken__init_object( RogueClassEOLToken* THIS );
RogueClassEOLToken* RogueEOLToken__init__TokenType_String( RogueClassEOLToken* THIS, RogueClassTokenType* _auto_1224_0, RogueString* _auto_1225_1 );
RogueClassEOLToken* RogueEOLToken__init__Token( RogueClassEOLToken* THIS, RogueClassToken* existing_0 );
RogueString* RogueCmdAdd__type_name( RogueClassCmdAdd* THIS );
RogueClassCmd* RogueCmdAdd__clone__CloneArgs( RogueClassCmdAdd* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAdd__combine_literal_operands__Type( RogueClassCmdAdd* THIS, RogueClassType* common_type_0 );
RogueClassCmdAdd* RogueCmdAdd__init_object( RogueClassCmdAdd* THIS );
RogueString* RogueCmdAdd__fn_name( RogueClassCmdAdd* THIS );
RogueClassCmd* RogueCmdAdd__resolve_operator_method__Scope_Type_Type( RogueClassCmdAdd* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueString* RogueCmdAdd__symbol( RogueClassCmdAdd* THIS );
RogueString* RogueCmdNativeHeader__type_name( RogueClassCmdNativeHeader* THIS );
RogueClassCmdNativeHeader* RogueCmdNativeHeader__clone__CloneArgs( RogueClassCmdNativeHeader* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdNativeHeader__requires_semicolon( RogueClassCmdNativeHeader* THIS );
RogueClassCmd* RogueCmdNativeHeader__resolve__Scope( RogueClassCmdNativeHeader* THIS, RogueClassScope* scope_0 );
void RogueCmdNativeHeader__write_cpp__CPPWriter_Logical( RogueClassCmdNativeHeader* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdNativeHeader__trace_used_code( RogueClassCmdNativeHeader* THIS );
void RogueCmdNativeHeader__update_this_type__Scope( RogueClassCmdNativeHeader* THIS, RogueClassScope* scope_0 );
RogueClassCmdNativeHeader* RogueCmdNativeHeader__init_object( RogueClassCmdNativeHeader* THIS );
RogueClassCmdNativeHeader* RogueCmdNativeHeader__init__Token_String( RogueClassCmdNativeHeader* THIS, RogueClassToken* _auto_1232_0, RogueString* _auto_1233_1 );
RogueString* RogueCmdNativeCode__type_name( RogueClassCmdNativeCode* THIS );
RogueClassCmdNativeCode* RogueCmdNativeCode__clone__CloneArgs( RogueClassCmdNativeCode* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdNativeCode__requires_semicolon( RogueClassCmdNativeCode* THIS );
RogueClassCmd* RogueCmdNativeCode__resolve__Scope( RogueClassCmdNativeCode* THIS, RogueClassScope* scope_0 );
void RogueCmdNativeCode__write_cpp__CPPWriter_Logical( RogueClassCmdNativeCode* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdNativeCode__trace_used_code( RogueClassCmdNativeCode* THIS );
void RogueCmdNativeCode__update_this_type__Scope( RogueClassCmdNativeCode* THIS, RogueClassScope* scope_0 );
RogueClassCmdNativeCode* RogueCmdNativeCode__init_object( RogueClassCmdNativeCode* THIS );
RogueClassCmdNativeCode* RogueCmdNativeCode__init__Token_String( RogueClassCmdNativeCode* THIS, RogueClassToken* _auto_1234_0, RogueString* _auto_1235_1 );
RogueString* RogueCmdIf__type_name( RogueClassCmdIf* THIS );
RogueClassCmd* RogueCmdIf__clone__CloneArgs( RogueClassCmdIf* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdIf__resolve__Scope( RogueClassCmdIf* THIS, RogueClassScope* scope_0 );
void RogueCmdIf__write_cpp__CPPWriter_Logical( RogueClassCmdIf* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdIf__trace_used_code( RogueClassCmdIf* THIS );
void RogueCmdIf__update_this_type__Scope( RogueClassCmdIf* THIS, RogueClassScope* scope_0 );
RogueClassCmdIf* RogueCmdIf__init_object( RogueClassCmdIf* THIS );
RogueClassCmdIf* RogueCmdIf__init__Token_Cmd_Int32( RogueClassCmdIf* THIS, RogueClassToken* _auto_1236_0, RogueClassCmd* _auto_1237_1, RogueInt32 _auto_1238_2 );
RogueClassCmdIf* RogueCmdIf__init__Token_Cmd_CmdStatementList_Int32( RogueClassCmdIf* THIS, RogueClassToken* _auto_1239_0, RogueClassCmd* _auto_1240_1, RogueClassCmdStatementList* _auto_1241_2, RogueInt32 _auto_1242_3 );
RogueString* RogueCmdWhich__type_name( RogueClassCmdWhich* THIS );
RogueClassCmdWhich* RogueCmdWhich__clone__CloneArgs( RogueClassCmdWhich* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWhich__resolve__Scope( RogueClassCmdWhich* THIS, RogueClassScope* scope_0 );
void RogueCmdWhich__update_this_type__Scope( RogueClassCmdWhich* THIS, RogueClassScope* scope_0 );
RogueClassCmdWhich* RogueCmdWhich__init_object( RogueClassCmdWhich* THIS );
RogueClassCmdWhich* RogueCmdWhich__init__Token_Cmd_CmdWhichCaseList_CmdWhichCase_Int32( RogueClassCmdWhich* THIS, RogueClassToken* _auto_1243_0, RogueClassCmd* _auto_1244_1, RogueCmdWhichCaseList* _auto_1245_2, RogueClassCmdWhichCase* _auto_1246_3, RogueInt32 _auto_1247_4 );
RogueClassCmdWhichCase* RogueCmdWhich__add_case__Token( RogueClassCmdWhich* THIS, RogueClassToken* case_t_0 );
RogueClassCmdWhichCase* RogueCmdWhich__add_case_others__Token( RogueClassCmdWhich* THIS, RogueClassToken* case_t_0 );
RogueString* RogueCmdContingent__type_name( RogueClassCmdContingent* THIS );
RogueClassCmd* RogueCmdContingent__clone__CloneArgs( RogueClassCmdContingent* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdContingent* RogueCmdContingent__resolve__Scope( RogueClassCmdContingent* THIS, RogueClassScope* scope_0 );
void RogueCmdContingent__write_cpp__CPPWriter_Logical( RogueClassCmdContingent* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdContingent__trace_used_code( RogueClassCmdContingent* THIS );
void RogueCmdContingent__update_this_type__Scope( RogueClassCmdContingent* THIS, RogueClassScope* scope_0 );
RogueClassCmdContingent* RogueCmdContingent__init_object( RogueClassCmdContingent* THIS );
RogueClassCmd* RogueCmdContingent__set_control_logic__CmdControlStructure( RogueClassCmdContingent* THIS, RogueClassCmdControlStructure* original_0 );
RogueClassCmdContingent* RogueCmdContingent__init__Token_CmdStatementList( RogueClassCmdContingent* THIS, RogueClassToken* _auto_1248_0, RogueClassCmdStatementList* _auto_1249_1 );
RogueString* RogueCmdTry__type_name( RogueClassCmdTry* THIS );
RogueClassCmdTry* RogueCmdTry__clone__CloneArgs( RogueClassCmdTry* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdTry__resolve__Scope( RogueClassCmdTry* THIS, RogueClassScope* scope_0 );
void RogueCmdTry__write_cpp__CPPWriter_Logical( RogueClassCmdTry* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdTry__trace_used_code( RogueClassCmdTry* THIS );
void RogueCmdTry__update_this_type__Scope( RogueClassCmdTry* THIS, RogueClassScope* scope_0 );
RogueClassCmdTry* RogueCmdTry__init_object( RogueClassCmdTry* THIS );
RogueClassCmdTry* RogueCmdTry__init__Token_CmdStatementList_CmdCatchList( RogueClassCmdTry* THIS, RogueClassToken* _auto_1250_0, RogueClassCmdStatementList* _auto_1251_1, RogueCmdCatchList* _auto_1252_2 );
RogueClassCmdCatch* RogueCmdTry__add_catch__Token( RogueClassCmdTry* THIS, RogueClassToken* catch_t_0 );
RogueString* RogueCmdAwait__type_name( RogueClassCmdAwait* THIS );
RogueClassCmd* RogueCmdAwait__clone__CloneArgs( RogueClassCmdAwait* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAwait__resolve__Scope( RogueClassCmdAwait* THIS, RogueClassScope* scope_0 );
RogueClassCmdAwait* RogueCmdAwait__init_object( RogueClassCmdAwait* THIS );
RogueClassCmdAwait* RogueCmdAwait__init__Token_Cmd_CmdStatementList_Local( RogueClassCmdAwait* THIS, RogueClassToken* _auto_1253_0, RogueClassCmd* _auto_1254_1, RogueClassCmdStatementList* _auto_1255_2, RogueClassLocal* _auto_1256_3 );
RogueString* RogueCmdYield__type_name( RogueClassCmdYield* THIS );
RogueClassCmd* RogueCmdYield__clone__CloneArgs( RogueClassCmdYield* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdYield__resolve__Scope( RogueClassCmdYield* THIS, RogueClassScope* scope_0 );
RogueClassCmdYield* RogueCmdYield__init_object( RogueClassCmdYield* THIS );
RogueClassCmdYield* RogueCmdYield__init__Token( RogueClassCmdYield* THIS, RogueClassToken* _auto_1257_0 );
RogueString* RogueCmdThrow__type_name( RogueClassCmdThrow* THIS );
RogueClassCmdThrow* RogueCmdThrow__clone__CloneArgs( RogueClassCmdThrow* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdThrow__resolve__Scope( RogueClassCmdThrow* THIS, RogueClassScope* scope_0 );
void RogueCmdThrow__write_cpp__CPPWriter_Logical( RogueClassCmdThrow* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdThrow__trace_used_code( RogueClassCmdThrow* THIS );
void RogueCmdThrow__update_this_type__Scope( RogueClassCmdThrow* THIS, RogueClassScope* scope_0 );
RogueClassCmdThrow* RogueCmdThrow__init_object( RogueClassCmdThrow* THIS );
RogueClassCmdThrow* RogueCmdThrow__init__Token_Cmd( RogueClassCmdThrow* THIS, RogueClassToken* _auto_1258_0, RogueClassCmd* _auto_1259_1 );
RogueString* RogueCmdFormattedString__type_name( RogueClassCmdFormattedString* THIS );
RogueClassCmd* RogueCmdFormattedString__clone__CloneArgs( RogueClassCmdFormattedString* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdFormattedString__implicit_type__Scope( RogueClassCmdFormattedString* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdFormattedString__resolve__Scope( RogueClassCmdFormattedString* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdFormattedString__type( RogueClassCmdFormattedString* THIS );
void RogueCmdFormattedString__update_this_type__Scope( RogueClassCmdFormattedString* THIS, RogueClassScope* scope_0 );
RogueClassCmdFormattedString* RogueCmdFormattedString__init_object( RogueClassCmdFormattedString* THIS );
RogueClassCmdFormattedString* RogueCmdFormattedString__init__Token_String_CmdArgs( RogueClassCmdFormattedString* THIS, RogueClassToken* _auto_1260_0, RogueString* _auto_1261_1, RogueClassCmdArgs* _auto_1262_2 );
RogueString* RogueCmdTrace__type_name( RogueClassCmdTrace* THIS );
RogueClassCmdTrace* RogueCmdTrace__clone__CloneArgs( RogueClassCmdTrace* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdTrace__resolve__Scope( RogueClassCmdTrace* THIS, RogueClassScope* scope_0 );
void RogueCmdTrace__trace_used_code( RogueClassCmdTrace* THIS );
void RogueCmdTrace__update_this_type__Scope( RogueClassCmdTrace* THIS, RogueClassScope* scope_0 );
RogueClassCmdTrace* RogueCmdTrace__init_object( RogueClassCmdTrace* THIS );
RogueClassCmdTrace* RogueCmdTrace__init__Token_Cmd( RogueClassCmdTrace* THIS, RogueClassToken* _auto_1263_0, RogueClassCmd* _auto_1264_1 );
RogueString* RogueCmdEscape__type_name( RogueClassCmdEscape* THIS );
RogueClassCmd* RogueCmdEscape__clone__CloneArgs( RogueClassCmdEscape* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdEscape__resolve__Scope( RogueClassCmdEscape* THIS, RogueClassScope* scope_0 );
void RogueCmdEscape__write_cpp__CPPWriter_Logical( RogueClassCmdEscape* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdEscape__trace_used_code( RogueClassCmdEscape* THIS );
void RogueCmdEscape__update_this_type__Scope( RogueClassCmdEscape* THIS, RogueClassScope* scope_0 );
RogueClassCmdEscape* RogueCmdEscape__init_object( RogueClassCmdEscape* THIS );
RogueClassCmdEscape* RogueCmdEscape__init__Token_Int32_CmdControlStructure( RogueClassCmdEscape* THIS, RogueClassToken* _auto_1265_0, RogueInt32 _auto_1266_1, RogueClassCmdControlStructure* _auto_1267_2 );
RogueString* RogueCmdNextIteration__type_name( RogueClassCmdNextIteration* THIS );
RogueClassCmd* RogueCmdNextIteration__clone__CloneArgs( RogueClassCmdNextIteration* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdNextIteration__resolve__Scope( RogueClassCmdNextIteration* THIS, RogueClassScope* scope_0 );
void RogueCmdNextIteration__write_cpp__CPPWriter_Logical( RogueClassCmdNextIteration* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdNextIteration__trace_used_code( RogueClassCmdNextIteration* THIS );
void RogueCmdNextIteration__update_this_type__Scope( RogueClassCmdNextIteration* THIS, RogueClassScope* scope_0 );
RogueClassCmdNextIteration* RogueCmdNextIteration__init_object( RogueClassCmdNextIteration* THIS );
RogueClassCmdNextIteration* RogueCmdNextIteration__init__Token_CmdControlStructure( RogueClassCmdNextIteration* THIS, RogueClassToken* _auto_1268_0, RogueClassCmdControlStructure* _auto_1269_1 );
RogueString* RogueCmdNecessary__type_name( RogueClassCmdNecessary* THIS );
RogueClassCmd* RogueCmdNecessary__clone__CloneArgs( RogueClassCmdNecessary* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdNecessary__resolve__Scope( RogueClassCmdNecessary* THIS, RogueClassScope* scope_0 );
void RogueCmdNecessary__write_cpp__CPPWriter_Logical( RogueClassCmdNecessary* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdNecessary__trace_used_code( RogueClassCmdNecessary* THIS );
void RogueCmdNecessary__update_this_type__Scope( RogueClassCmdNecessary* THIS, RogueClassScope* scope_0 );
RogueClassCmdNecessary* RogueCmdNecessary__init_object( RogueClassCmdNecessary* THIS );
RogueClassCmdNecessary* RogueCmdNecessary__init__Token_Cmd_CmdContingent( RogueClassCmdNecessary* THIS, RogueClassToken* _auto_1270_0, RogueClassCmd* _auto_1271_1, RogueClassCmdContingent* _auto_1272_2 );
RogueString* RogueCmdSufficient__type_name( RogueClassCmdSufficient* THIS );
RogueClassCmd* RogueCmdSufficient__clone__CloneArgs( RogueClassCmdSufficient* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdSufficient__resolve__Scope( RogueClassCmdSufficient* THIS, RogueClassScope* scope_0 );
void RogueCmdSufficient__write_cpp__CPPWriter_Logical( RogueClassCmdSufficient* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdSufficient__trace_used_code( RogueClassCmdSufficient* THIS );
void RogueCmdSufficient__update_this_type__Scope( RogueClassCmdSufficient* THIS, RogueClassScope* scope_0 );
RogueClassCmdSufficient* RogueCmdSufficient__init_object( RogueClassCmdSufficient* THIS );
RogueClassCmdSufficient* RogueCmdSufficient__init__Token_Cmd_CmdContingent( RogueClassCmdSufficient* THIS, RogueClassToken* _auto_1273_0, RogueClassCmd* _auto_1274_1, RogueClassCmdContingent* _auto_1275_2 );
RogueString* RogueCmdAdjust__type_name( RogueClassCmdAdjust* THIS );
RogueClassCmd* RogueCmdAdjust__resolve__Scope( RogueClassCmdAdjust* THIS, RogueClassScope* scope_0 );
RogueClassCmdAdjust* RogueCmdAdjust__init_object( RogueClassCmdAdjust* THIS );
RogueClassCmdAdjust* RogueCmdAdjust__init__Token_Cmd_Int32( RogueClassCmdAdjust* THIS, RogueClassToken* _auto_1276_0, RogueClassCmd* _auto_1277_1, RogueInt32 _auto_1278_2 );
RogueString* RogueCmdOpWithAssign__type_name( RogueClassCmdOpWithAssign* THIS );
RogueClassCmd* RogueCmdOpWithAssign__clone__CloneArgs( RogueClassCmdOpWithAssign* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdOpWithAssign__resolve__Scope( RogueClassCmdOpWithAssign* THIS, RogueClassScope* scope_0 );
RogueClassCmdOpWithAssign* RogueCmdOpWithAssign__init_object( RogueClassCmdOpWithAssign* THIS );
RogueClassCmdOpWithAssign* RogueCmdOpWithAssign__init__Token_Cmd_TokenType_Cmd( RogueClassCmdOpWithAssign* THIS, RogueClassToken* _auto_1279_0, RogueClassCmd* _auto_1280_1, RogueClassTokenType* _auto_1281_2, RogueClassCmd* _auto_1282_3 );
RogueString* RogueCmdWhichCaseList__to_String( RogueCmdWhichCaseList* THIS );
RogueString* RogueCmdWhichCaseList__type_name( RogueCmdWhichCaseList* THIS );
RogueCmdWhichCaseList* RogueCmdWhichCaseList__init_object( RogueCmdWhichCaseList* THIS );
RogueCmdWhichCaseList* RogueCmdWhichCaseList__init( RogueCmdWhichCaseList* THIS );
RogueCmdWhichCaseList* RogueCmdWhichCaseList__init__Int32( RogueCmdWhichCaseList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdWhichCaseList* RogueCmdWhichCaseList__add__CmdWhichCase( RogueCmdWhichCaseList* THIS, RogueClassCmdWhichCase* value_0 );
RogueInt32 RogueCmdWhichCaseList__capacity( RogueCmdWhichCaseList* THIS );
RogueCmdWhichCaseList* RogueCmdWhichCaseList__reserve__Int32( RogueCmdWhichCaseList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueCmdWhichCase__type_name( RogueClassCmdWhichCase* THIS );
RogueClassCmdWhichCase* RogueCmdWhichCase__clone__CloneArgs( RogueClassCmdWhichCase* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdWhichCase__trace_used_code( RogueClassCmdWhichCase* THIS );
void RogueCmdWhichCase__update_this_type__Scope( RogueClassCmdWhichCase* THIS, RogueClassScope* scope_0 );
RogueClassCmdWhichCase* RogueCmdWhichCase__init_object( RogueClassCmdWhichCase* THIS );
RogueClassCmdWhichCase* RogueCmdWhichCase__init__Token_CmdArgs_CmdStatementList( RogueClassCmdWhichCase* THIS, RogueClassToken* _auto_1283_0, RogueClassCmdArgs* _auto_1284_1, RogueClassCmdStatementList* _auto_1285_2 );
RogueClassCmd* RogueCmdWhichCase__as_conditional__String( RogueClassCmdWhichCase* THIS, RogueString* expression_var_name_0 );
RogueString* RogueCmdCatchList__to_String( RogueCmdCatchList* THIS );
RogueString* RogueCmdCatchList__type_name( RogueCmdCatchList* THIS );
RogueCmdCatchList* RogueCmdCatchList__init_object( RogueCmdCatchList* THIS );
RogueCmdCatchList* RogueCmdCatchList__init( RogueCmdCatchList* THIS );
RogueCmdCatchList* RogueCmdCatchList__init__Int32( RogueCmdCatchList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdCatchList* RogueCmdCatchList__add__CmdCatch( RogueCmdCatchList* THIS, RogueClassCmdCatch* value_0 );
RogueInt32 RogueCmdCatchList__capacity( RogueCmdCatchList* THIS );
RogueCmdCatchList* RogueCmdCatchList__reserve__Int32( RogueCmdCatchList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueCmdCatch__type_name( RogueClassCmdCatch* THIS );
RogueClassCmdCatch* RogueCmdCatch__clone__CloneArgs( RogueClassCmdCatch* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCatch__resolve__Scope( RogueClassCmdCatch* THIS, RogueClassScope* scope_0 );
void RogueCmdCatch__write_cpp__CPPWriter_Logical( RogueClassCmdCatch* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCatch__trace_used_code( RogueClassCmdCatch* THIS );
void RogueCmdCatch__update_this_type__Scope( RogueClassCmdCatch* THIS, RogueClassScope* scope_0 );
RogueClassCmdCatch* RogueCmdCatch__init_object( RogueClassCmdCatch* THIS );
RogueClassCmdCatch* RogueCmdCatch__init__Token_Local_CmdStatementList( RogueClassCmdCatch* THIS, RogueClassToken* _auto_1290_0, RogueClassLocal* _auto_1291_1, RogueClassCmdStatementList* _auto_1292_2 );
RogueString* RogueCmdLocalDeclaration__type_name( RogueClassCmdLocalDeclaration* THIS );
RogueClassCmd* RogueCmdLocalDeclaration__clone__CloneArgs( RogueClassCmdLocalDeclaration* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdLocalDeclaration__exit_scope__Scope( RogueClassCmdLocalDeclaration* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdLocalDeclaration__resolve__Scope( RogueClassCmdLocalDeclaration* THIS, RogueClassScope* scope_0 );
void RogueCmdLocalDeclaration__write_cpp__CPPWriter_Logical( RogueClassCmdLocalDeclaration* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdLocalDeclaration__trace_used_code( RogueClassCmdLocalDeclaration* THIS );
void RogueCmdLocalDeclaration__update_this_type__Scope( RogueClassCmdLocalDeclaration* THIS, RogueClassScope* scope_0 );
RogueClassCmdLocalDeclaration* RogueCmdLocalDeclaration__init_object( RogueClassCmdLocalDeclaration* THIS );
RogueClassCmdLocalDeclaration* RogueCmdLocalDeclaration__init__Token_Local_Logical( RogueClassCmdLocalDeclaration* THIS, RogueClassToken* _auto_1303_0, RogueClassLocal* _auto_1304_1, RogueLogical _auto_1305_2 );
RogueString* RogueCmdAdjustLocal__type_name( RogueClassCmdAdjustLocal* THIS );
RogueClassCmd* RogueCmdAdjustLocal__clone__CloneArgs( RogueClassCmdAdjustLocal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAdjustLocal__resolve__Scope( RogueClassCmdAdjustLocal* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdAdjustLocal__type( RogueClassCmdAdjustLocal* THIS );
void RogueCmdAdjustLocal__write_cpp__CPPWriter_Logical( RogueClassCmdAdjustLocal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdAdjustLocal__trace_used_code( RogueClassCmdAdjustLocal* THIS );
void RogueCmdAdjustLocal__update_this_type__Scope( RogueClassCmdAdjustLocal* THIS, RogueClassScope* scope_0 );
RogueClassCmdAdjustLocal* RogueCmdAdjustLocal__init_object( RogueClassCmdAdjustLocal* THIS );
RogueClassCmdAdjustLocal* RogueCmdAdjustLocal__init__Token_Local_Int32( RogueClassCmdAdjustLocal* THIS, RogueClassToken* _auto_1312_0, RogueClassLocal* _auto_1313_1, RogueInt32 _auto_1314_2 );
RogueString* RogueCmdReadLocal__type_name( RogueClassCmdReadLocal* THIS );
RogueClassCmd* RogueCmdReadLocal__clone__CloneArgs( RogueClassCmdReadLocal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReadLocal__resolve__Scope( RogueClassCmdReadLocal* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdReadLocal__resolve_adjust__Scope_Int32( RogueClassCmdReadLocal* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
RogueClassType* Rogue_CmdReadLocal__type( RogueClassCmdReadLocal* THIS );
void RogueCmdReadLocal__write_cpp__CPPWriter_Logical( RogueClassCmdReadLocal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadLocal__trace_used_code( RogueClassCmdReadLocal* THIS );
void RogueCmdReadLocal__update_this_type__Scope( RogueClassCmdReadLocal* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadLocal* RogueCmdReadLocal__init_object( RogueClassCmdReadLocal* THIS );
RogueClassCmdReadLocal* RogueCmdReadLocal__init__Token_Local( RogueClassCmdReadLocal* THIS, RogueClassToken* _auto_1315_0, RogueClassLocal* _auto_1316_1 );
RogueString* RogueCmdCompareLE__type_name( RogueClassCmdCompareLE* THIS );
RogueClassCmd* RogueCmdCompareLE__clone__CloneArgs( RogueClassCmdCompareLE* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareLE__combine_literal_operands__Type( RogueClassCmdCompareLE* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareLE* RogueCmdCompareLE__init_object( RogueClassCmdCompareLE* THIS );
RogueString* RogueCmdCompareLE__symbol( RogueClassCmdCompareLE* THIS );
RogueClassCmd* RogueCmdCompareLE__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareLE* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdRange__type_name( RogueClassCmdRange* THIS );
RogueClassCmd* RogueCmdRange__resolve__Scope( RogueClassCmdRange* THIS, RogueClassScope* scope_0 );
void RogueCmdRange__trace_used_code( RogueClassCmdRange* THIS );
void RogueCmdRange__update_this_type__Scope( RogueClassCmdRange* THIS, RogueClassScope* scope_0 );
RogueClassCmdRange* RogueCmdRange__init_object( RogueClassCmdRange* THIS );
RogueClassCmdRange* RogueCmdRange__init__Token_Cmd_Cmd_Cmd( RogueClassCmdRange* THIS, RogueClassToken* _auto_1317_0, RogueClassCmd* _auto_1318_1, RogueClassCmd* _auto_1319_2, RogueClassCmd* _auto_1320_3 );
RogueInt32 RogueCmdRange__default_step_size( RogueClassCmdRange* THIS );
RogueString* RogueCmdLocalOpWithAssign__type_name( RogueClassCmdLocalOpWithAssign* THIS );
RogueClassCmd* RogueCmdLocalOpWithAssign__clone__CloneArgs( RogueClassCmdLocalOpWithAssign* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLocalOpWithAssign__resolve__Scope( RogueClassCmdLocalOpWithAssign* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLocalOpWithAssign__type( RogueClassCmdLocalOpWithAssign* THIS );
void RogueCmdLocalOpWithAssign__write_cpp__CPPWriter_Logical( RogueClassCmdLocalOpWithAssign* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdLocalOpWithAssign__trace_used_code( RogueClassCmdLocalOpWithAssign* THIS );
void RogueCmdLocalOpWithAssign__update_this_type__Scope( RogueClassCmdLocalOpWithAssign* THIS, RogueClassScope* scope_0 );
RogueClassCmdLocalOpWithAssign* RogueCmdLocalOpWithAssign__init_object( RogueClassCmdLocalOpWithAssign* THIS );
RogueClassCmdLocalOpWithAssign* RogueCmdLocalOpWithAssign__init__Token_Local_TokenType_Cmd( RogueClassCmdLocalOpWithAssign* THIS, RogueClassToken* _auto_1321_0, RogueClassLocal* _auto_1322_1, RogueClassTokenType* _auto_1323_2, RogueClassCmd* _auto_1324_3 );
RogueString* RogueCmdResolvedOpWithAssign__type_name( RogueClassCmdResolvedOpWithAssign* THIS );
RogueClassCmdResolvedOpWithAssign* RogueCmdResolvedOpWithAssign__init_object( RogueClassCmdResolvedOpWithAssign* THIS );
RogueString* RogueCmdResolvedOpWithAssign__symbol( RogueClassCmdResolvedOpWithAssign* THIS );
RogueString* RogueCmdResolvedOpWithAssign__cpp_symbol( RogueClassCmdResolvedOpWithAssign* THIS );
RogueString* RogueCmdRangeUpTo__type_name( RogueClassCmdRangeUpTo* THIS );
RogueClassCmd* RogueCmdRangeUpTo__clone__CloneArgs( RogueClassCmdRangeUpTo* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdRangeUpTo* RogueCmdRangeUpTo__init_object( RogueClassCmdRangeUpTo* THIS );
RogueString* RogueCmdCompareGE__type_name( RogueClassCmdCompareGE* THIS );
RogueClassCmd* RogueCmdCompareGE__clone__CloneArgs( RogueClassCmdCompareGE* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareGE__combine_literal_operands__Type( RogueClassCmdCompareGE* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareGE* RogueCmdCompareGE__init_object( RogueClassCmdCompareGE* THIS );
RogueString* RogueCmdCompareGE__symbol( RogueClassCmdCompareGE* THIS );
RogueClassCmd* RogueCmdCompareGE__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareGE* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdForEach__type_name( RogueClassCmdForEach* THIS );
RogueClassCmd* RogueCmdForEach__clone__CloneArgs( RogueClassCmdForEach* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdForEach__resolve__Scope( RogueClassCmdForEach* THIS, RogueClassScope* scope_0 );
void RogueCmdForEach__trace_used_code( RogueClassCmdForEach* THIS );
void RogueCmdForEach__update_this_type__Scope( RogueClassCmdForEach* THIS, RogueClassScope* scope_0 );
RogueClassCmdForEach* RogueCmdForEach__init_object( RogueClassCmdForEach* THIS );
RogueClassCmdForEach* RogueCmdForEach__init__Token_String_String_Cmd_Cmd_CmdStatementList( RogueClassCmdForEach* THIS, RogueClassToken* _auto_1325_0, RogueString* _auto_1326_1, RogueString* _auto_1327_2, RogueClassCmd* _auto_1328_3, RogueClassCmd* _auto_1329_4, RogueClassCmdStatementList* _auto_1330_5 );
RogueString* RogueCmdRangeDownTo__type_name( RogueClassCmdRangeDownTo* THIS );
RogueClassCmd* RogueCmdRangeDownTo__clone__CloneArgs( RogueClassCmdRangeDownTo* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdRangeDownTo* RogueCmdRangeDownTo__init_object( RogueClassCmdRangeDownTo* THIS );
RogueInt32 RogueCmdRangeDownTo__default_step_size( RogueClassCmdRangeDownTo* THIS );
RogueString* RogueCmdLogicalXor__type_name( RogueClassCmdLogicalXor* THIS );
RogueClassCmd* RogueCmdLogicalXor__clone__CloneArgs( RogueClassCmdLogicalXor* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdLogicalXor* RogueCmdLogicalXor__init_object( RogueClassCmdLogicalXor* THIS );
RogueString* RogueCmdLogicalXor__symbol( RogueClassCmdLogicalXor* THIS );
RogueString* RogueCmdLogicalXor__cpp_symbol( RogueClassCmdLogicalXor* THIS );
RogueLogical RogueCmdLogicalXor__combine_literal_operands__Logical_Logical( RogueClassCmdLogicalXor* THIS, RogueLogical a_0, RogueLogical b_1 );
RogueString* RogueCmdBinaryLogical__type_name( RogueClassCmdBinaryLogical* THIS );
RogueClassCmd* RogueCmdBinaryLogical__resolve__Scope( RogueClassCmdBinaryLogical* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdBinaryLogical__type( RogueClassCmdBinaryLogical* THIS );
RogueClassCmdBinaryLogical* RogueCmdBinaryLogical__init_object( RogueClassCmdBinaryLogical* THIS );
RogueClassCmd* RogueCmdBinaryLogical__resolve_operator_method__Scope_Type_Type( RogueClassCmdBinaryLogical* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueLogical RogueCmdBinaryLogical__combine_literal_operands__Logical_Logical( RogueClassCmdBinaryLogical* THIS, RogueLogical a_0, RogueLogical b_1 );
RogueString* RogueCmdLogicalOr__type_name( RogueClassCmdLogicalOr* THIS );
RogueClassCmd* RogueCmdLogicalOr__clone__CloneArgs( RogueClassCmdLogicalOr* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdLogicalOr* RogueCmdLogicalOr__init_object( RogueClassCmdLogicalOr* THIS );
RogueString* RogueCmdLogicalOr__symbol( RogueClassCmdLogicalOr* THIS );
RogueString* RogueCmdLogicalOr__cpp_symbol( RogueClassCmdLogicalOr* THIS );
RogueLogical RogueCmdLogicalOr__combine_literal_operands__Logical_Logical( RogueClassCmdLogicalOr* THIS, RogueLogical a_0, RogueLogical b_1 );
RogueString* RogueCmdLogicalAnd__type_name( RogueClassCmdLogicalAnd* THIS );
RogueClassCmd* RogueCmdLogicalAnd__clone__CloneArgs( RogueClassCmdLogicalAnd* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdLogicalAnd* RogueCmdLogicalAnd__init_object( RogueClassCmdLogicalAnd* THIS );
RogueString* RogueCmdLogicalAnd__symbol( RogueClassCmdLogicalAnd* THIS );
RogueString* RogueCmdLogicalAnd__cpp_symbol( RogueClassCmdLogicalAnd* THIS );
RogueLogical RogueCmdLogicalAnd__combine_literal_operands__Logical_Logical( RogueClassCmdLogicalAnd* THIS, RogueLogical a_0, RogueLogical b_1 );
RogueString* RogueCmdCompareEQ__type_name( RogueClassCmdCompareEQ* THIS );
RogueClassCmd* RogueCmdCompareEQ__clone__CloneArgs( RogueClassCmdCompareEQ* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareEQ__combine_literal_operands__Type( RogueClassCmdCompareEQ* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareEQ* RogueCmdCompareEQ__init_object( RogueClassCmdCompareEQ* THIS );
RogueLogical RogueCmdCompareEQ__requires_parens( RogueClassCmdCompareEQ* THIS );
RogueString* RogueCmdCompareEQ__symbol( RogueClassCmdCompareEQ* THIS );
RogueClassCmd* RogueCmdCompareEQ__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareEQ* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdCompareIs__type_name( RogueClassCmdCompareIs* THIS );
RogueClassCmd* RogueCmdCompareIs__clone__CloneArgs( RogueClassCmdCompareIs* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCompareIs__write_cpp__CPPWriter_Logical( RogueClassCmdCompareIs* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCompareIs* RogueCmdCompareIs__init_object( RogueClassCmdCompareIs* THIS );
RogueClassCmd* RogueCmdCompareIs__resolve_for_types__Scope_Type_Type( RogueClassCmdCompareIs* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueString* RogueCmdCompareIs__symbol( RogueClassCmdCompareIs* THIS );
RogueString* RogueCmdCompareIs__cpp_symbol( RogueClassCmdCompareIs* THIS );
RogueString* RogueCmdCompareIsNot__type_name( RogueClassCmdCompareIsNot* THIS );
RogueClassCmd* RogueCmdCompareIsNot__clone__CloneArgs( RogueClassCmdCompareIsNot* THIS, RogueClassCloneArgs* clone_args_0 );
void RogueCmdCompareIsNot__write_cpp__CPPWriter_Logical( RogueClassCmdCompareIsNot* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCompareIsNot* RogueCmdCompareIsNot__init_object( RogueClassCmdCompareIsNot* THIS );
RogueClassCmd* RogueCmdCompareIsNot__resolve_for_types__Scope_Type_Type( RogueClassCmdCompareIsNot* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueString* RogueCmdCompareIsNot__symbol( RogueClassCmdCompareIsNot* THIS );
RogueString* RogueCmdCompareIsNot__cpp_symbol( RogueClassCmdCompareIsNot* THIS );
RogueString* RogueCmdCompareLT__type_name( RogueClassCmdCompareLT* THIS );
RogueClassCmd* RogueCmdCompareLT__clone__CloneArgs( RogueClassCmdCompareLT* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareLT__combine_literal_operands__Type( RogueClassCmdCompareLT* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareLT* RogueCmdCompareLT__init_object( RogueClassCmdCompareLT* THIS );
RogueString* RogueCmdCompareLT__symbol( RogueClassCmdCompareLT* THIS );
RogueClassCmd* RogueCmdCompareLT__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareLT* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdCompareGT__type_name( RogueClassCmdCompareGT* THIS );
RogueClassCmd* RogueCmdCompareGT__clone__CloneArgs( RogueClassCmdCompareGT* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCompareGT__combine_literal_operands__Type( RogueClassCmdCompareGT* THIS, RogueClassType* common_type_0 );
RogueClassCmdCompareGT* RogueCmdCompareGT__init_object( RogueClassCmdCompareGT* THIS );
RogueString* RogueCmdCompareGT__symbol( RogueClassCmdCompareGT* THIS );
RogueClassCmd* RogueCmdCompareGT__resolve_for_reference__Scope_Type_Type_Logical( RogueClassCmdCompareGT* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2, RogueLogical force_error_3 );
RogueString* RogueCmdInstanceOf__type_name( RogueClassCmdInstanceOf* THIS );
RogueClassCmd* RogueCmdInstanceOf__clone__CloneArgs( RogueClassCmdInstanceOf* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdInstanceOf__resolve__Scope( RogueClassCmdInstanceOf* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdInstanceOf__type( RogueClassCmdInstanceOf* THIS );
void RogueCmdInstanceOf__write_cpp__CPPWriter_Logical( RogueClassCmdInstanceOf* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdInstanceOf* RogueCmdInstanceOf__init_object( RogueClassCmdInstanceOf* THIS );
RogueString* RogueCmdLogicalNot__type_name( RogueClassCmdLogicalNot* THIS );
RogueClassCmd* RogueCmdLogicalNot__clone__CloneArgs( RogueClassCmdLogicalNot* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLogicalNot__resolve__Scope( RogueClassCmdLogicalNot* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLogicalNot__type( RogueClassCmdLogicalNot* THIS );
RogueClassCmdLogicalNot* RogueCmdLogicalNot__init_object( RogueClassCmdLogicalNot* THIS );
RogueString* RogueCmdLogicalNot__prefix_symbol( RogueClassCmdLogicalNot* THIS );
RogueClassCmd* RogueCmdLogicalNot__resolve_for_literal_operand__Scope( RogueClassCmdLogicalNot* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdLogicalNot__cpp_prefix_symbol( RogueClassCmdLogicalNot* THIS );
RogueString* RogueCmdBitwiseXor__type_name( RogueClassCmdBitwiseXor* THIS );
RogueClassCmd* RogueCmdBitwiseXor__clone__CloneArgs( RogueClassCmdBitwiseXor* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseXor__combine_literal_operands__Type( RogueClassCmdBitwiseXor* THIS, RogueClassType* common_type_0 );
RogueClassCmdBitwiseXor* RogueCmdBitwiseXor__init_object( RogueClassCmdBitwiseXor* THIS );
RogueString* RogueCmdBitwiseXor__symbol( RogueClassCmdBitwiseXor* THIS );
RogueString* RogueCmdBitwiseXor__cpp_symbol( RogueClassCmdBitwiseXor* THIS );
RogueString* RogueCmdBitwiseOp__type_name( RogueClassCmdBitwiseOp* THIS );
RogueClassCmdBitwiseOp* RogueCmdBitwiseOp__init_object( RogueClassCmdBitwiseOp* THIS );
RogueClassCmd* RogueCmdBitwiseOp__resolve_for_common_type__Scope_Type( RogueClassCmdBitwiseOp* THIS, RogueClassScope* scope_0, RogueClassType* common_type_1 );
RogueClassCmd* RogueCmdBitwiseOp__resolve_operator_method__Scope_Type_Type( RogueClassCmdBitwiseOp* THIS, RogueClassScope* scope_0, RogueClassType* left_type_1, RogueClassType* right_type_2 );
RogueString* RogueCmdBitwiseOr__type_name( RogueClassCmdBitwiseOr* THIS );
RogueClassCmd* RogueCmdBitwiseOr__clone__CloneArgs( RogueClassCmdBitwiseOr* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseOr__combine_literal_operands__Type( RogueClassCmdBitwiseOr* THIS, RogueClassType* common_type_0 );
RogueClassCmdBitwiseOr* RogueCmdBitwiseOr__init_object( RogueClassCmdBitwiseOr* THIS );
RogueString* RogueCmdBitwiseOr__symbol( RogueClassCmdBitwiseOr* THIS );
RogueString* RogueCmdBitwiseAnd__type_name( RogueClassCmdBitwiseAnd* THIS );
RogueClassCmd* RogueCmdBitwiseAnd__clone__CloneArgs( RogueClassCmdBitwiseAnd* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseAnd__combine_literal_operands__Type( RogueClassCmdBitwiseAnd* THIS, RogueClassType* common_type_0 );
RogueClassCmdBitwiseAnd* RogueCmdBitwiseAnd__init_object( RogueClassCmdBitwiseAnd* THIS );
RogueString* RogueCmdBitwiseAnd__symbol( RogueClassCmdBitwiseAnd* THIS );
RogueString* RogueCmdBitwiseShiftLeft__type_name( RogueClassCmdBitwiseShiftLeft* THIS );
RogueClassCmd* RogueCmdBitwiseShiftLeft__clone__CloneArgs( RogueClassCmdBitwiseShiftLeft* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseShiftLeft__combine_literal_operands__Type( RogueClassCmdBitwiseShiftLeft* THIS, RogueClassType* common_type_0 );
RogueClassCmdBitwiseShiftLeft* RogueCmdBitwiseShiftLeft__init_object( RogueClassCmdBitwiseShiftLeft* THIS );
RogueString* RogueCmdBitwiseShiftLeft__symbol( RogueClassCmdBitwiseShiftLeft* THIS );
RogueString* RogueCmdBitwiseShiftLeft__cpp_symbol( RogueClassCmdBitwiseShiftLeft* THIS );
RogueString* RogueCmdBitwiseShiftRight__type_name( RogueClassCmdBitwiseShiftRight* THIS );
RogueClassCmd* RogueCmdBitwiseShiftRight__clone__CloneArgs( RogueClassCmdBitwiseShiftRight* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseShiftRight__combine_literal_operands__Type( RogueClassCmdBitwiseShiftRight* THIS, RogueClassType* common_type_0 );
RogueClassCmd* RogueCmdBitwiseShiftRight__resolve__Scope( RogueClassCmdBitwiseShiftRight* THIS, RogueClassScope* scope_0 );
RogueClassCmdBitwiseShiftRight* RogueCmdBitwiseShiftRight__init_object( RogueClassCmdBitwiseShiftRight* THIS );
RogueString* RogueCmdBitwiseShiftRight__symbol( RogueClassCmdBitwiseShiftRight* THIS );
RogueString* RogueCmdBitwiseShiftRightX__type_name( RogueClassCmdBitwiseShiftRightX* THIS );
RogueClassCmd* RogueCmdBitwiseShiftRightX__clone__CloneArgs( RogueClassCmdBitwiseShiftRightX* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdBitwiseShiftRightX__combine_literal_operands__Type( RogueClassCmdBitwiseShiftRightX* THIS, RogueClassType* common_type_0 );
RogueClassCmdBitwiseShiftRightX* RogueCmdBitwiseShiftRightX__init_object( RogueClassCmdBitwiseShiftRightX* THIS );
RogueString* RogueCmdBitwiseShiftRightX__symbol( RogueClassCmdBitwiseShiftRightX* THIS );
RogueString* RogueCmdBitwiseShiftRightX__cpp_symbol( RogueClassCmdBitwiseShiftRightX* THIS );
RogueString* RogueCmdSubtract__type_name( RogueClassCmdSubtract* THIS );
RogueClassCmd* RogueCmdSubtract__clone__CloneArgs( RogueClassCmdSubtract* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdSubtract__combine_literal_operands__Type( RogueClassCmdSubtract* THIS, RogueClassType* common_type_0 );
RogueClassCmdSubtract* RogueCmdSubtract__init_object( RogueClassCmdSubtract* THIS );
RogueString* RogueCmdSubtract__fn_name( RogueClassCmdSubtract* THIS );
RogueString* RogueCmdSubtract__symbol( RogueClassCmdSubtract* THIS );
RogueString* RogueCmdMultiply__type_name( RogueClassCmdMultiply* THIS );
RogueClassCmd* RogueCmdMultiply__clone__CloneArgs( RogueClassCmdMultiply* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdMultiply__combine_literal_operands__Type( RogueClassCmdMultiply* THIS, RogueClassType* common_type_0 );
RogueClassCmdMultiply* RogueCmdMultiply__init_object( RogueClassCmdMultiply* THIS );
RogueString* RogueCmdMultiply__fn_name( RogueClassCmdMultiply* THIS );
RogueString* RogueCmdMultiply__symbol( RogueClassCmdMultiply* THIS );
RogueString* RogueCmdDivide__type_name( RogueClassCmdDivide* THIS );
RogueClassCmd* RogueCmdDivide__clone__CloneArgs( RogueClassCmdDivide* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdDivide__combine_literal_operands__Type( RogueClassCmdDivide* THIS, RogueClassType* common_type_0 );
RogueClassCmdDivide* RogueCmdDivide__init_object( RogueClassCmdDivide* THIS );
RogueString* RogueCmdDivide__fn_name( RogueClassCmdDivide* THIS );
RogueString* RogueCmdDivide__symbol( RogueClassCmdDivide* THIS );
RogueString* RogueCmdMod__type_name( RogueClassCmdMod* THIS );
RogueClassCmd* RogueCmdMod__clone__CloneArgs( RogueClassCmdMod* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdMod__combine_literal_operands__Type( RogueClassCmdMod* THIS, RogueClassType* common_type_0 );
RogueClassCmd* RogueCmdMod__resolve__Scope( RogueClassCmdMod* THIS, RogueClassScope* scope_0 );
RogueClassCmdMod* RogueCmdMod__init_object( RogueClassCmdMod* THIS );
RogueString* RogueCmdMod__fn_name( RogueClassCmdMod* THIS );
RogueString* RogueCmdMod__symbol( RogueClassCmdMod* THIS );
RogueString* RogueCmdPower__type_name( RogueClassCmdPower* THIS );
RogueClassCmd* RogueCmdPower__clone__CloneArgs( RogueClassCmdPower* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdPower__combine_literal_operands__Type( RogueClassCmdPower* THIS, RogueClassType* common_type_0 );
void RogueCmdPower__write_cpp__CPPWriter_Logical( RogueClassCmdPower* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdPower* RogueCmdPower__init_object( RogueClassCmdPower* THIS );
RogueString* RogueCmdPower__fn_name( RogueClassCmdPower* THIS );
RogueString* RogueCmdPower__symbol( RogueClassCmdPower* THIS );
RogueString* RogueCmdNegate__type_name( RogueClassCmdNegate* THIS );
RogueClassCmd* RogueCmdNegate__clone__CloneArgs( RogueClassCmdNegate* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdNegate__implicit_type__Scope( RogueClassCmdNegate* THIS, RogueClassScope* scope_0 );
RogueClassCmdNegate* RogueCmdNegate__init_object( RogueClassCmdNegate* THIS );
RogueString* RogueCmdNegate__prefix_symbol( RogueClassCmdNegate* THIS );
RogueString* RogueCmdNegate__fn_name( RogueClassCmdNegate* THIS );
RogueClassCmd* RogueCmdNegate__resolve_for_literal_operand__Scope( RogueClassCmdNegate* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdNegate__suffix_symbol( RogueClassCmdNegate* THIS );
RogueString* RogueCmdBitwiseNot__type_name( RogueClassCmdBitwiseNot* THIS );
RogueClassCmd* RogueCmdBitwiseNot__clone__CloneArgs( RogueClassCmdBitwiseNot* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* Rogue_CmdBitwiseNot__type( RogueClassCmdBitwiseNot* THIS );
RogueClassCmdBitwiseNot* RogueCmdBitwiseNot__init_object( RogueClassCmdBitwiseNot* THIS );
RogueString* RogueCmdBitwiseNot__prefix_symbol( RogueClassCmdBitwiseNot* THIS );
RogueString* RogueCmdBitwiseNot__fn_name( RogueClassCmdBitwiseNot* THIS );
RogueClassCmd* RogueCmdBitwiseNot__resolve_for_literal_operand__Scope( RogueClassCmdBitwiseNot* THIS, RogueClassScope* scope_0 );
RogueString* RogueCmdBitwiseNot__cpp_prefix_symbol( RogueClassCmdBitwiseNot* THIS );
RogueString* RogueCmdGetOptionalValue__type_name( RogueClassCmdGetOptionalValue* THIS );
RogueClassCmd* RogueCmdGetOptionalValue__clone__CloneArgs( RogueClassCmdGetOptionalValue* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdGetOptionalValue* RogueCmdGetOptionalValue__resolve__Scope( RogueClassCmdGetOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdGetOptionalValue__type( RogueClassCmdGetOptionalValue* THIS );
void RogueCmdGetOptionalValue__write_cpp__CPPWriter_Logical( RogueClassCmdGetOptionalValue* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdGetOptionalValue__trace_used_code( RogueClassCmdGetOptionalValue* THIS );
void RogueCmdGetOptionalValue__update_this_type__Scope( RogueClassCmdGetOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassCmdGetOptionalValue* RogueCmdGetOptionalValue__init_object( RogueClassCmdGetOptionalValue* THIS );
RogueClassCmdGetOptionalValue* RogueCmdGetOptionalValue__init__Token_Cmd( RogueClassCmdGetOptionalValue* THIS, RogueClassToken* _auto_1331_0, RogueClassCmd* _auto_1332_1 );
RogueString* RogueCmdElementAccess__type_name( RogueClassCmdElementAccess* THIS );
RogueClassCmd* RogueCmdElementAccess__clone__CloneArgs( RogueClassCmdElementAccess* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdElementAccess__resolve__Scope( RogueClassCmdElementAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdElementAccess__resolve_assignment__Scope_Cmd( RogueClassCmdElementAccess* THIS, RogueClassScope* scope_0, RogueClassCmd* new_value_1 );
RogueClassCmd* RogueCmdElementAccess__resolve_adjust__Scope_Int32( RogueClassCmdElementAccess* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
void RogueCmdElementAccess__update_this_type__Scope( RogueClassCmdElementAccess* THIS, RogueClassScope* scope_0 );
RogueClassCmdElementAccess* RogueCmdElementAccess__init_object( RogueClassCmdElementAccess* THIS );
RogueClassCmdElementAccess* RogueCmdElementAccess__init__Token_Cmd_Cmd( RogueClassCmdElementAccess* THIS, RogueClassToken* _auto_1333_0, RogueClassCmd* _auto_1334_1, RogueClassCmd* _auto_1335_2 );
RogueString* RogueCmdListConvert__type_name( RogueClassCmdListConvert* THIS );
RogueClassCmdListConvert* RogueCmdListConvert__clone__CloneArgs( RogueClassCmdListConvert* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdListConvert__implicit_type__Scope( RogueClassCmdListConvert* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdListConvert__resolve__Scope( RogueClassCmdListConvert* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdListConvert__type( RogueClassCmdListConvert* THIS );
RogueClassCmdListConvert* RogueCmdListConvert__init_object( RogueClassCmdListConvert* THIS );
RogueClassCmdListConvert* RogueCmdListConvert__init__Token_Cmd_Type_Cmd( RogueClassCmdListConvert* THIS, RogueClassToken* _auto_1336_0, RogueClassCmd* _auto_1337_1, RogueClassType* _auto_1338_2, RogueClassCmd* _auto_1339_3 );
RogueString* RogueCmdConvertToType__type_name( RogueClassCmdConvertToType* THIS );
RogueClassCmd* RogueCmdConvertToType__clone__CloneArgs( RogueClassCmdConvertToType* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdConvertToType__resolve__Scope( RogueClassCmdConvertToType* THIS, RogueClassScope* scope_0 );
RogueClassCmdConvertToType* RogueCmdConvertToType__init_object( RogueClassCmdConvertToType* THIS );
RogueString* RogueCmdCreateCallback__type_name( RogueClassCmdCreateCallback* THIS );
RogueClassCmdCreateCallback* RogueCmdCreateCallback__clone__CloneArgs( RogueClassCmdCreateCallback* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateCallback__resolve__Scope( RogueClassCmdCreateCallback* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateCallback* RogueCmdCreateCallback__init_object( RogueClassCmdCreateCallback* THIS );
RogueClassCmdCreateCallback* RogueCmdCreateCallback__init__Token_Cmd_String_String_Type( RogueClassCmdCreateCallback* THIS, RogueClassToken* _auto_1340_0, RogueClassCmd* _auto_1341_1, RogueString* _auto_1342_2, RogueString* _auto_1343_3, RogueClassType* _auto_1344_4 );
RogueString* RogueCmdAs__type_name( RogueClassCmdAs* THIS );
RogueClassCmd* RogueCmdAs__clone__CloneArgs( RogueClassCmdAs* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAs__resolve__Scope( RogueClassCmdAs* THIS, RogueClassScope* scope_0 );
void RogueCmdAs__write_cpp__CPPWriter_Logical( RogueClassCmdAs* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdAs* RogueCmdAs__init_object( RogueClassCmdAs* THIS );
RogueString* RogueCmdDefaultValue__type_name( RogueClassCmdDefaultValue* THIS );
RogueClassCmd* RogueCmdDefaultValue__clone__CloneArgs( RogueClassCmdDefaultValue* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdDefaultValue__resolve__Scope( RogueClassCmdDefaultValue* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdDefaultValue__type( RogueClassCmdDefaultValue* THIS );
RogueClassCmdDefaultValue* RogueCmdDefaultValue__init_object( RogueClassCmdDefaultValue* THIS );
RogueClassCmdDefaultValue* RogueCmdDefaultValue__init__Token_Type( RogueClassCmdDefaultValue* THIS, RogueClassToken* _auto_1345_0, RogueClassType* _auto_1346_1 );
RogueString* RogueCmdLiteralReal64__type_name( RogueClassCmdLiteralReal64* THIS );
RogueClassCmd* RogueCmdLiteralReal64__clone__CloneArgs( RogueClassCmdLiteralReal64* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralReal64__resolve__Scope( RogueClassCmdLiteralReal64* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralReal64__type( RogueClassCmdLiteralReal64* THIS );
void RogueCmdLiteralReal64__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralReal64* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralReal64* RogueCmdLiteralReal64__init_object( RogueClassCmdLiteralReal64* THIS );
RogueClassCmdLiteralReal64* RogueCmdLiteralReal64__init__Token_Real64( RogueClassCmdLiteralReal64* THIS, RogueClassToken* _auto_1347_0, RogueReal64 _auto_1348_1 );
RogueString* RogueCmdLiteralInt64__type_name( RogueClassCmdLiteralInt64* THIS );
RogueClassCmd* RogueCmdLiteralInt64__cast_to__Type_Scope( RogueClassCmdLiteralInt64* THIS, RogueClassType* target_type_0, RogueClassScope* scope_1 );
RogueClassCmd* RogueCmdLiteralInt64__clone__CloneArgs( RogueClassCmdLiteralInt64* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralInt64__resolve__Scope( RogueClassCmdLiteralInt64* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralInt64__type( RogueClassCmdLiteralInt64* THIS );
void RogueCmdLiteralInt64__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralInt64* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralInt64* RogueCmdLiteralInt64__init_object( RogueClassCmdLiteralInt64* THIS );
RogueClassCmdLiteralInt64* RogueCmdLiteralInt64__init__Token_Int64( RogueClassCmdLiteralInt64* THIS, RogueClassToken* _auto_1349_0, RogueInt64 _auto_1350_1 );
RogueString* RogueCmdLiteralCharacter__type_name( RogueClassCmdLiteralCharacter* THIS );
RogueClassCmd* RogueCmdLiteralCharacter__clone__CloneArgs( RogueClassCmdLiteralCharacter* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdLiteralCharacter__resolve__Scope( RogueClassCmdLiteralCharacter* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLiteralCharacter__type( RogueClassCmdLiteralCharacter* THIS );
void RogueCmdLiteralCharacter__write_cpp__CPPWriter_Logical( RogueClassCmdLiteralCharacter* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdLiteralCharacter* RogueCmdLiteralCharacter__init_object( RogueClassCmdLiteralCharacter* THIS );
RogueClassCmdLiteralCharacter* RogueCmdLiteralCharacter__init__Token_Character( RogueClassCmdLiteralCharacter* THIS, RogueClassToken* _auto_1351_0, RogueCharacter _auto_1352_1 );
RogueString* RogueCmdCreateList__type_name( RogueClassCmdCreateList* THIS );
RogueClassCmd* RogueCmdCreateList__clone__CloneArgs( RogueClassCmdCreateList* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateList__resolve__Scope( RogueClassCmdCreateList* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateList* RogueCmdCreateList__init_object( RogueClassCmdCreateList* THIS );
RogueClassCmdCreateList* RogueCmdCreateList__init__Token_CmdArgs_Type( RogueClassCmdCreateList* THIS, RogueClassToken* _auto_1353_0, RogueClassCmdArgs* _auto_1354_1, RogueClassType* _auto_1355_2 );
RogueString* RogueCmdCallPriorMethod__type_name( RogueClassCmdCallPriorMethod* THIS );
RogueClassCmd* RogueCmdCallPriorMethod__clone__CloneArgs( RogueClassCmdCallPriorMethod* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCallPriorMethod__resolve__Scope( RogueClassCmdCallPriorMethod* THIS, RogueClassScope* scope_0 );
void RogueCmdCallPriorMethod__update_this_type__Scope( RogueClassCmdCallPriorMethod* THIS, RogueClassScope* scope_0 );
RogueClassCmdCallPriorMethod* RogueCmdCallPriorMethod__init_object( RogueClassCmdCallPriorMethod* THIS );
RogueClassCmdCallPriorMethod* RogueCmdCallPriorMethod__init__Token_String_CmdArgs_CmdFlagArgList( RogueClassCmdCallPriorMethod* THIS, RogueClassToken* _auto_1356_0, RogueString* _auto_1357_1, RogueClassCmdArgs* _auto_1358_2, RogueCmdFlagArgList* _auto_1359_3 );
RogueString* RogueFnArgList__to_String( RogueFnArgList* THIS );
RogueString* RogueFnArgList__type_name( RogueFnArgList* THIS );
RogueFnArgList* RogueFnArgList__init_object( RogueFnArgList* THIS );
RogueFnArgList* RogueFnArgList__init( RogueFnArgList* THIS );
RogueFnArgList* RogueFnArgList__init__Int32( RogueFnArgList* THIS, RogueInt32 initial_capacity_0 );
RogueFnArgList* RogueFnArgList__add__FnArg( RogueFnArgList* THIS, RogueClassFnArg* value_0 );
RogueInt32 RogueFnArgList__capacity( RogueFnArgList* THIS );
RogueFnArgList* RogueFnArgList__reserve__Int32( RogueFnArgList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueFnArg__type_name( RogueClassFnArg* THIS );
RogueClassFnArg* RogueFnArg__init__String_Cmd( RogueClassFnArg* THIS, RogueString* _auto_1360_0, RogueClassCmd* _auto_1361_1 );
RogueClassFnArg* RogueFnArg__set_type__Type( RogueClassFnArg* THIS, RogueClassType* _auto_1362_0 );
RogueClassFnArg* RogueFnArg__init_object( RogueClassFnArg* THIS );
RogueString* RogueCmdSelect__type_name( RogueClassCmdSelect* THIS );
RogueClassCmdSelect* RogueCmdSelect__clone__CloneArgs( RogueClassCmdSelect* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdSelect__implicit_type__Scope( RogueClassCmdSelect* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdSelect__resolve__Scope( RogueClassCmdSelect* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdSelect__type( RogueClassCmdSelect* THIS );
void RogueCmdSelect__write_cpp__CPPWriter_Logical( RogueClassCmdSelect* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdSelect__trace_used_code( RogueClassCmdSelect* THIS );
void RogueCmdSelect__update_this_type__Scope( RogueClassCmdSelect* THIS, RogueClassScope* scope_0 );
RogueClassCmdSelect* RogueCmdSelect__init_object( RogueClassCmdSelect* THIS );
RogueClassCmdSelect* RogueCmdSelect__init__Token_Local_Cmd_CmdSelectCaseList( RogueClassCmdSelect* THIS, RogueClassToken* _auto_1367_0, RogueClassLocal* _auto_1368_1, RogueClassCmd* _auto_1369_2, RogueCmdSelectCaseList* _auto_1370_3 );
void RogueCmdSelect__write_cpp__CmdSelectCaseListReader_CPPWriter_Logical( RogueClassCmdSelect* THIS, RogueClassCmdSelectCaseListReader* reader_0, RogueClassCPPWriter* writer_1, RogueLogical first_case_2 );
RogueString* RogueCmdSelectCaseList__to_String( RogueCmdSelectCaseList* THIS );
RogueString* RogueCmdSelectCaseList__type_name( RogueCmdSelectCaseList* THIS );
RogueCmdSelectCaseList* RogueCmdSelectCaseList__init_object( RogueCmdSelectCaseList* THIS );
RogueCmdSelectCaseList* RogueCmdSelectCaseList__init( RogueCmdSelectCaseList* THIS );
RogueCmdSelectCaseList* RogueCmdSelectCaseList__init__Int32( RogueCmdSelectCaseList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdSelectCaseList* RogueCmdSelectCaseList__add__CmdSelectCase( RogueCmdSelectCaseList* THIS, RogueClassCmdSelectCase* value_0 );
RogueInt32 RogueCmdSelectCaseList__capacity( RogueCmdSelectCaseList* THIS );
RogueClassCmdSelectCase* RogueCmdSelectCaseList__last( RogueCmdSelectCaseList* THIS );
RogueClassCmdSelectCaseListReader* RogueCmdSelectCaseList__reader( RogueCmdSelectCaseList* THIS );
RogueCmdSelectCaseList* RogueCmdSelectCaseList__reserve__Int32( RogueCmdSelectCaseList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueCmdSelectCase__type_name( RogueClassCmdSelectCase* THIS );
RogueClassCmdSelectCase* RogueCmdSelectCase__clone__CloneArgs( RogueClassCmdSelectCase* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdSelectCase__implicit_type__Scope( RogueClassCmdSelectCase* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdSelectCase__resolve__Scope( RogueClassCmdSelectCase* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdSelectCase__type( RogueClassCmdSelectCase* THIS );
void RogueCmdSelectCase__trace_used_code( RogueClassCmdSelectCase* THIS );
void RogueCmdSelectCase__update_this_type__Scope( RogueClassCmdSelectCase* THIS, RogueClassScope* scope_0 );
RogueClassCmdSelectCase* RogueCmdSelectCase__init_object( RogueClassCmdSelectCase* THIS );
RogueClassCmdSelectCase* RogueCmdSelectCase__init__Token_CmdList_Cmd( RogueClassCmdSelectCase* THIS, RogueClassToken* _auto_1371_0, RogueCmdList* _auto_1372_1, RogueClassCmd* _auto_1373_2 );
void RogueCmdSelectCase__cast_conditions__Type_Scope( RogueClassCmdSelectCase* THIS, RogueClassType* to_type_0, RogueClassScope* scope_1 );
RogueString* RogueCmdTypedLiteralList__type_name( RogueClassCmdTypedLiteralList* THIS );
RogueClassCmdTypedLiteralList* RogueCmdTypedLiteralList__clone__CloneArgs( RogueClassCmdTypedLiteralList* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassType* RogueCmdTypedLiteralList__implicit_type__Scope( RogueClassCmdTypedLiteralList* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdTypedLiteralList__resolve__Scope( RogueClassCmdTypedLiteralList* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdTypedLiteralList__type( RogueClassCmdTypedLiteralList* THIS );
RogueClassCmdTypedLiteralList* RogueCmdTypedLiteralList__init_object( RogueClassCmdTypedLiteralList* THIS );
RogueClassCmdTypedLiteralList* RogueCmdTypedLiteralList__init__Token_String_CmdArgs( RogueClassCmdTypedLiteralList* THIS, RogueClassToken* _auto_1379_0, RogueString* _auto_1380_1, RogueClassCmdArgs* _auto_1381_2 );
RogueClassCmdTypedLiteralList* RogueCmdTypedLiteralList__add__Cmd( RogueClassCmdTypedLiteralList* THIS, RogueClassCmd* element_0 );
RogueString* RogueString_ParseReaderTableEntryList__to_String( RogueTableEntry_of_String_ParseReaderList* THIS );
RogueString* RogueString_ParseReaderTableEntryList__type_name( RogueTableEntry_of_String_ParseReaderList* THIS );
RogueTableEntry_of_String_ParseReaderList* RogueString_ParseReaderTableEntryList__init_object( RogueTableEntry_of_String_ParseReaderList* THIS );
RogueTableEntry_of_String_ParseReaderList* RogueString_ParseReaderTableEntryList__init__Int32_String_ParseReaderTableEntry( RogueTableEntry_of_String_ParseReaderList* THIS, RogueInt32 initial_capacity_0, RogueClassString_ParseReaderTableEntry* initial_value_1 );
RogueTableEntry_of_String_ParseReaderList* RogueString_ParseReaderTableEntryList__add__String_ParseReaderTableEntry( RogueTableEntry_of_String_ParseReaderList* THIS, RogueClassString_ParseReaderTableEntry* value_0 );
RogueInt32 RogueString_ParseReaderTableEntryList__capacity( RogueTableEntry_of_String_ParseReaderList* THIS );
RogueTableEntry_of_String_ParseReaderList* RogueString_ParseReaderTableEntryList__reserve__Int32( RogueTableEntry_of_String_ParseReaderList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_ParseReaderTableEntry__type_name( RogueClassString_ParseReaderTableEntry* THIS );
RogueClassString_ParseReaderTableEntry* RogueString_ParseReaderTableEntry__init__String_ParseReader_Int32( RogueClassString_ParseReaderTableEntry* THIS, RogueString* _key_0, RogueClassParseReader* _value_1, RogueInt32 _hash_2 );
RogueClassString_ParseReaderTableEntry* RogueString_ParseReaderTableEntry__init_object( RogueClassString_ParseReaderTableEntry* THIS );
RogueString* RogueString_ParseReaderTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueFileReader__type_name( RogueClassFileReader* THIS );
RogueLogical RogueFileReader__has_another( RogueClassFileReader* THIS );
RogueCharacter RogueFileReader__peek( RogueClassFileReader* THIS );
RogueCharacter RogueFileReader__read( RogueClassFileReader* THIS );
RogueClassFileReader* RogueFileReader__init__String( RogueClassFileReader* THIS, RogueString* _filepath_0 );
RogueClassFileReader* RogueFileReader__close( RogueClassFileReader* THIS );
RogueLogical RogueFileReader__open__String( RogueClassFileReader* THIS, RogueString* _auto_1389_0 );
RogueClassFileReader* RogueFileReader__init_object( RogueClassFileReader* THIS );
RogueString* RogueFileWriter__type_name( RogueClassFileWriter* THIS );
RogueClassFileWriter* RogueFileWriter__close( RogueClassFileWriter* THIS );
RogueClassFileWriter* RogueFileWriter__flush( RogueClassFileWriter* THIS );
RogueClassFileWriter* RogueFileWriter__write__Character( RogueClassFileWriter* THIS, RogueCharacter ch_0 );
RogueClassFileWriter* RogueFileWriter__init__String( RogueClassFileWriter* THIS, RogueString* _filepath_0 );
RogueLogical RogueFileWriter__open__String( RogueClassFileWriter* THIS, RogueString* _auto_1390_0 );
RogueClassFileWriter* RogueFileWriter__init_object( RogueClassFileWriter* THIS );
RogueString* RogueTokenListRebuilder__type_name( RogueClassTokenListRebuilder* THIS );
RogueClassTokenListRebuilder* RogueTokenListRebuilder__init__TokenList( RogueClassTokenListRebuilder* THIS, RogueTokenList* _auto_1422_0 );
RogueLogical RogueTokenListRebuilder__has_another( RogueClassTokenListRebuilder* THIS );
RogueClassToken* RogueTokenListRebuilder__peek__Int32( RogueClassTokenListRebuilder* THIS, RogueInt32 lookahead_0 );
RogueClassToken* RogueTokenListRebuilder__read( RogueClassTokenListRebuilder* THIS );
RogueClassTokenListRebuilder* RogueTokenListRebuilder__write__Token( RogueClassTokenListRebuilder* THIS, RogueClassToken* value_0 );
RogueClassTokenListRebuilder* RogueTokenListRebuilder__init_object( RogueClassTokenListRebuilder* THIS );
RogueString* RogueString_TokenTypeTableEntryList__to_String( RogueTableEntry_of_String_TokenTypeList* THIS );
RogueString* RogueString_TokenTypeTableEntryList__type_name( RogueTableEntry_of_String_TokenTypeList* THIS );
RogueTableEntry_of_String_TokenTypeList* RogueString_TokenTypeTableEntryList__init_object( RogueTableEntry_of_String_TokenTypeList* THIS );
RogueTableEntry_of_String_TokenTypeList* RogueString_TokenTypeTableEntryList__init__Int32_String_TokenTypeTableEntry( RogueTableEntry_of_String_TokenTypeList* THIS, RogueInt32 initial_capacity_0, RogueClassString_TokenTypeTableEntry* initial_value_1 );
RogueTableEntry_of_String_TokenTypeList* RogueString_TokenTypeTableEntryList__add__String_TokenTypeTableEntry( RogueTableEntry_of_String_TokenTypeList* THIS, RogueClassString_TokenTypeTableEntry* value_0 );
RogueInt32 RogueString_TokenTypeTableEntryList__capacity( RogueTableEntry_of_String_TokenTypeList* THIS );
RogueTableEntry_of_String_TokenTypeList* RogueString_TokenTypeTableEntryList__reserve__Int32( RogueTableEntry_of_String_TokenTypeList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TokenTypeTableEntry__type_name( RogueClassString_TokenTypeTableEntry* THIS );
RogueClassString_TokenTypeTableEntry* RogueString_TokenTypeTableEntry__init__String_TokenType_Int32( RogueClassString_TokenTypeTableEntry* THIS, RogueString* _key_0, RogueClassTokenType* _value_1, RogueInt32 _hash_2 );
RogueClassString_TokenTypeTableEntry* RogueString_TokenTypeTableEntry__init_object( RogueClassString_TokenTypeTableEntry* THIS );
RogueString* RogueString_TokenTypeTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_TypeSpecializerTableEntryList__to_String( RogueTableEntry_of_String_TypeSpecializerList* THIS );
RogueString* RogueString_TypeSpecializerTableEntryList__type_name( RogueTableEntry_of_String_TypeSpecializerList* THIS );
RogueTableEntry_of_String_TypeSpecializerList* RogueString_TypeSpecializerTableEntryList__init_object( RogueTableEntry_of_String_TypeSpecializerList* THIS );
RogueTableEntry_of_String_TypeSpecializerList* RogueString_TypeSpecializerTableEntryList__init__Int32_String_TypeSpecializerTableEntry( RogueTableEntry_of_String_TypeSpecializerList* THIS, RogueInt32 initial_capacity_0, RogueClassString_TypeSpecializerTableEntry* initial_value_1 );
RogueTableEntry_of_String_TypeSpecializerList* RogueString_TypeSpecializerTableEntryList__add__String_TypeSpecializerTableEntry( RogueTableEntry_of_String_TypeSpecializerList* THIS, RogueClassString_TypeSpecializerTableEntry* value_0 );
RogueInt32 RogueString_TypeSpecializerTableEntryList__capacity( RogueTableEntry_of_String_TypeSpecializerList* THIS );
RogueTableEntry_of_String_TypeSpecializerList* RogueString_TypeSpecializerTableEntryList__reserve__Int32( RogueTableEntry_of_String_TypeSpecializerList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TypeSpecializerTableEntry__type_name( RogueClassString_TypeSpecializerTableEntry* THIS );
RogueClassString_TypeSpecializerTableEntry* RogueString_TypeSpecializerTableEntry__init__String_TypeSpecializer_Int32( RogueClassString_TypeSpecializerTableEntry* THIS, RogueString* _key_0, RogueClassTypeSpecializer* _value_1, RogueInt32 _hash_2 );
RogueClassString_TypeSpecializerTableEntry* RogueString_TypeSpecializerTableEntry__init_object( RogueClassString_TypeSpecializerTableEntry* THIS );
RogueString* RogueString_TypeSpecializerTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_CmdLabelTableEntryList__to_String( RogueTableEntry_of_String_CmdLabelList* THIS );
RogueString* RogueString_CmdLabelTableEntryList__type_name( RogueTableEntry_of_String_CmdLabelList* THIS );
RogueTableEntry_of_String_CmdLabelList* RogueString_CmdLabelTableEntryList__init_object( RogueTableEntry_of_String_CmdLabelList* THIS );
RogueTableEntry_of_String_CmdLabelList* RogueString_CmdLabelTableEntryList__init__Int32_String_CmdLabelTableEntry( RogueTableEntry_of_String_CmdLabelList* THIS, RogueInt32 initial_capacity_0, RogueClassString_CmdLabelTableEntry* initial_value_1 );
RogueTableEntry_of_String_CmdLabelList* RogueString_CmdLabelTableEntryList__add__String_CmdLabelTableEntry( RogueTableEntry_of_String_CmdLabelList* THIS, RogueClassString_CmdLabelTableEntry* value_0 );
RogueInt32 RogueString_CmdLabelTableEntryList__capacity( RogueTableEntry_of_String_CmdLabelList* THIS );
RogueTableEntry_of_String_CmdLabelList* RogueString_CmdLabelTableEntryList__reserve__Int32( RogueTableEntry_of_String_CmdLabelList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_CmdLabelTableEntry__type_name( RogueClassString_CmdLabelTableEntry* THIS );
RogueClassString_CmdLabelTableEntry* RogueString_CmdLabelTableEntry__init__String_CmdLabel_Int32( RogueClassString_CmdLabelTableEntry* THIS, RogueString* _key_0, RogueClassCmdLabel* _value_1, RogueInt32 _hash_2 );
RogueClassString_CmdLabelTableEntry* RogueString_CmdLabelTableEntry__init_object( RogueClassString_CmdLabelTableEntry* THIS );
RogueString* RogueString_CmdLabelTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueCmdCreateArray__type_name( RogueClassCmdCreateArray* THIS );
RogueClassCmd* RogueCmdCreateArray__clone__CloneArgs( RogueClassCmdCreateArray* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateArray__resolve__Scope( RogueClassCmdCreateArray* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCreateArray__type( RogueClassCmdCreateArray* THIS );
void RogueCmdCreateArray__write_cpp__CPPWriter_Logical( RogueClassCmdCreateArray* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCreateArray__trace_used_code( RogueClassCmdCreateArray* THIS );
void RogueCmdCreateArray__update_this_type__Scope( RogueClassCmdCreateArray* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateArray* RogueCmdCreateArray__init_object( RogueClassCmdCreateArray* THIS );
RogueClassCmdCreateArray* RogueCmdCreateArray__init__Token_Type_CmdArgs( RogueClassCmdCreateArray* THIS, RogueClassToken* _auto_1654_0, RogueClassType* _auto_1655_1, RogueClassCmdArgs* args_2 );
RogueClassCmdCreateArray* RogueCmdCreateArray__init__Token_Type_Cmd( RogueClassCmdCreateArray* THIS, RogueClassToken* _auto_1656_0, RogueClassType* _auto_1657_1, RogueClassCmd* _auto_1658_2 );
RogueString* RogueCmdCreateObject__type_name( RogueClassCmdCreateObject* THIS );
RogueClassCmd* RogueCmdCreateObject__clone__CloneArgs( RogueClassCmdCreateObject* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCreateObject__resolve__Scope( RogueClassCmdCreateObject* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdCreateObject__type( RogueClassCmdCreateObject* THIS );
void RogueCmdCreateObject__write_cpp__CPPWriter_Logical( RogueClassCmdCreateObject* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdCreateObject__trace_used_code( RogueClassCmdCreateObject* THIS );
void RogueCmdCreateObject__update_this_type__Scope( RogueClassCmdCreateObject* THIS, RogueClassScope* scope_0 );
RogueClassCmdCreateObject* RogueCmdCreateObject__init_object( RogueClassCmdCreateObject* THIS );
RogueClassCmdCreateObject* RogueCmdCreateObject__init__Token_Type( RogueClassCmdCreateObject* THIS, RogueClassToken* _auto_1659_0, RogueClassType* _auto_1660_1 );
RogueString* RogueCmdReadGlobal__type_name( RogueClassCmdReadGlobal* THIS );
RogueClassCmd* RogueCmdReadGlobal__clone__CloneArgs( RogueClassCmdReadGlobal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReadGlobal__resolve__Scope( RogueClassCmdReadGlobal* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdReadGlobal__resolve_adjust__Scope_Int32( RogueClassCmdReadGlobal* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
RogueClassType* Rogue_CmdReadGlobal__type( RogueClassCmdReadGlobal* THIS );
void RogueCmdReadGlobal__write_cpp__CPPWriter_Logical( RogueClassCmdReadGlobal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadGlobal__trace_used_code( RogueClassCmdReadGlobal* THIS );
void RogueCmdReadGlobal__update_this_type__Scope( RogueClassCmdReadGlobal* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadGlobal* RogueCmdReadGlobal__init_object( RogueClassCmdReadGlobal* THIS );
RogueClassCmdReadGlobal* RogueCmdReadGlobal__init__Token_Property( RogueClassCmdReadGlobal* THIS, RogueClassToken* _auto_1661_0, RogueClassProperty* _auto_1662_1 );
RogueString* RogueCmdReadProperty__type_name( RogueClassCmdReadProperty* THIS );
RogueClassCmd* RogueCmdReadProperty__clone__CloneArgs( RogueClassCmdReadProperty* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReadProperty__resolve__Scope( RogueClassCmdReadProperty* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdReadProperty__resolve_adjust__Scope_Int32( RogueClassCmdReadProperty* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
RogueClassType* Rogue_CmdReadProperty__type( RogueClassCmdReadProperty* THIS );
void RogueCmdReadProperty__write_cpp__CPPWriter_Logical( RogueClassCmdReadProperty* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadProperty__trace_used_code( RogueClassCmdReadProperty* THIS );
void RogueCmdReadProperty__update_this_type__Scope( RogueClassCmdReadProperty* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadProperty* RogueCmdReadProperty__init_object( RogueClassCmdReadProperty* THIS );
RogueClassCmdReadProperty* RogueCmdReadProperty__init__Token_Cmd_Property( RogueClassCmdReadProperty* THIS, RogueClassToken* _auto_1663_0, RogueClassCmd* _auto_1664_1, RogueClassProperty* _auto_1665_2 );
RogueString* RogueCmdLogicalizeOptionalValue__type_name( RogueClassCmdLogicalizeOptionalValue* THIS );
RogueClassCmd* RogueCmdLogicalizeOptionalValue__clone__CloneArgs( RogueClassCmdLogicalizeOptionalValue* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmdLogicalizeOptionalValue* RogueCmdLogicalizeOptionalValue__resolve__Scope( RogueClassCmdLogicalizeOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdLogicalizeOptionalValue__type( RogueClassCmdLogicalizeOptionalValue* THIS );
void RogueCmdLogicalizeOptionalValue__write_cpp__CPPWriter_Logical( RogueClassCmdLogicalizeOptionalValue* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdLogicalizeOptionalValue__trace_used_code( RogueClassCmdLogicalizeOptionalValue* THIS );
void RogueCmdLogicalizeOptionalValue__update_this_type__Scope( RogueClassCmdLogicalizeOptionalValue* THIS, RogueClassScope* scope_0 );
RogueClassCmdLogicalizeOptionalValue* RogueCmdLogicalizeOptionalValue__init_object( RogueClassCmdLogicalizeOptionalValue* THIS );
RogueClassCmdLogicalizeOptionalValue* RogueCmdLogicalizeOptionalValue__init__Token_Cmd_Logical( RogueClassCmdLogicalizeOptionalValue* THIS, RogueClassToken* _auto_1666_0, RogueClassCmd* _auto_1667_1, RogueLogical _auto_1668_2 );
RogueString* RogueCmdWriteSingleton__type_name( RogueClassCmdWriteSingleton* THIS );
RogueClassCmd* RogueCmdWriteSingleton__clone__CloneArgs( RogueClassCmdWriteSingleton* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWriteSingleton__resolve__Scope( RogueClassCmdWriteSingleton* THIS, RogueClassScope* scope_0 );
void RogueCmdWriteSingleton__write_cpp__CPPWriter_Logical( RogueClassCmdWriteSingleton* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdWriteSingleton__trace_used_code( RogueClassCmdWriteSingleton* THIS );
void RogueCmdWriteSingleton__update_this_type__Scope( RogueClassCmdWriteSingleton* THIS, RogueClassScope* scope_0 );
RogueClassCmdWriteSingleton* RogueCmdWriteSingleton__init_object( RogueClassCmdWriteSingleton* THIS );
RogueClassCmdWriteSingleton* RogueCmdWriteSingleton__init__Token_Type_Cmd( RogueClassCmdWriteSingleton* THIS, RogueClassToken* _auto_1670_0, RogueClassType* _auto_1671_1, RogueClassCmd* _auto_1672_2 );
RogueString* RogueCmdWriteLocal__type_name( RogueClassCmdWriteLocal* THIS );
RogueClassCmd* RogueCmdWriteLocal__clone__CloneArgs( RogueClassCmdWriteLocal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWriteLocal__resolve__Scope( RogueClassCmdWriteLocal* THIS, RogueClassScope* scope_0 );
void RogueCmdWriteLocal__write_cpp__CPPWriter_Logical( RogueClassCmdWriteLocal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdWriteLocal__trace_used_code( RogueClassCmdWriteLocal* THIS );
void RogueCmdWriteLocal__update_this_type__Scope( RogueClassCmdWriteLocal* THIS, RogueClassScope* scope_0 );
RogueClassCmdWriteLocal* RogueCmdWriteLocal__init_object( RogueClassCmdWriteLocal* THIS );
RogueClassCmdWriteLocal* RogueCmdWriteLocal__init__Token_Local_Cmd( RogueClassCmdWriteLocal* THIS, RogueClassToken* _auto_1673_0, RogueClassLocal* _auto_1674_1, RogueClassCmd* _auto_1675_2 );
RogueString* RogueCmdOpAssignGlobal__type_name( RogueClassCmdOpAssignGlobal* THIS );
RogueClassCmd* RogueCmdOpAssignGlobal__clone__CloneArgs( RogueClassCmdOpAssignGlobal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdOpAssignGlobal__resolve__Scope( RogueClassCmdOpAssignGlobal* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdOpAssignGlobal__type( RogueClassCmdOpAssignGlobal* THIS );
void RogueCmdOpAssignGlobal__write_cpp__CPPWriter_Logical( RogueClassCmdOpAssignGlobal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdOpAssignGlobal__trace_used_code( RogueClassCmdOpAssignGlobal* THIS );
void RogueCmdOpAssignGlobal__update_this_type__Scope( RogueClassCmdOpAssignGlobal* THIS, RogueClassScope* scope_0 );
RogueClassCmdOpAssignGlobal* RogueCmdOpAssignGlobal__init_object( RogueClassCmdOpAssignGlobal* THIS );
RogueClassCmdOpAssignGlobal* RogueCmdOpAssignGlobal__init__Token_Property_TokenType_Cmd( RogueClassCmdOpAssignGlobal* THIS, RogueClassToken* _auto_1676_0, RogueClassProperty* _auto_1677_1, RogueClassTokenType* _auto_1678_2, RogueClassCmd* _auto_1679_3 );
RogueString* RogueCmdOpAssignProperty__type_name( RogueClassCmdOpAssignProperty* THIS );
RogueClassCmd* RogueCmdOpAssignProperty__clone__CloneArgs( RogueClassCmdOpAssignProperty* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdOpAssignProperty__resolve__Scope( RogueClassCmdOpAssignProperty* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdOpAssignProperty__type( RogueClassCmdOpAssignProperty* THIS );
void RogueCmdOpAssignProperty__write_cpp__CPPWriter_Logical( RogueClassCmdOpAssignProperty* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdOpAssignProperty__trace_used_code( RogueClassCmdOpAssignProperty* THIS );
void RogueCmdOpAssignProperty__update_this_type__Scope( RogueClassCmdOpAssignProperty* THIS, RogueClassScope* scope_0 );
RogueClassCmdOpAssignProperty* RogueCmdOpAssignProperty__init_object( RogueClassCmdOpAssignProperty* THIS );
RogueClassCmdOpAssignProperty* RogueCmdOpAssignProperty__init__Token_Cmd_Property_TokenType_Cmd( RogueClassCmdOpAssignProperty* THIS, RogueClassToken* _auto_1680_0, RogueClassCmd* _auto_1681_1, RogueClassProperty* _auto_1682_2, RogueClassTokenType* _auto_1683_3, RogueClassCmd* _auto_1684_4 );
RogueString* RogueCmdControlStructureArray__type_name( RogueArray* THIS );
RogueString* RogueCmdTaskControlSectionList__to_String( RogueCmdTaskControlSectionList* THIS );
RogueString* RogueCmdTaskControlSectionList__type_name( RogueCmdTaskControlSectionList* THIS );
RogueCmdTaskControlSectionList* RogueCmdTaskControlSectionList__init_object( RogueCmdTaskControlSectionList* THIS );
RogueCmdTaskControlSectionList* RogueCmdTaskControlSectionList__init( RogueCmdTaskControlSectionList* THIS );
RogueCmdTaskControlSectionList* RogueCmdTaskControlSectionList__init__Int32( RogueCmdTaskControlSectionList* THIS, RogueInt32 initial_capacity_0 );
RogueCmdTaskControlSectionList* RogueCmdTaskControlSectionList__add__CmdTaskControlSection( RogueCmdTaskControlSectionList* THIS, RogueClassCmdTaskControlSection* value_0 );
RogueInt32 RogueCmdTaskControlSectionList__capacity( RogueCmdTaskControlSectionList* THIS );
RogueCmdTaskControlSectionList* RogueCmdTaskControlSectionList__reserve__Int32( RogueCmdTaskControlSectionList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueCmdBlock__type_name( RogueClassCmdBlock* THIS );
RogueClassCmd* RogueCmdBlock__clone__CloneArgs( RogueClassCmdBlock* THIS, RogueClassCloneArgs* clone_args_0 );
RogueLogical RogueCmdBlock__requires_semicolon( RogueClassCmdBlock* THIS );
RogueClassCmdBlock* RogueCmdBlock__resolve__Scope( RogueClassCmdBlock* THIS, RogueClassScope* scope_0 );
void RogueCmdBlock__write_cpp__CPPWriter_Logical( RogueClassCmdBlock* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdBlock__trace_used_code( RogueClassCmdBlock* THIS );
void RogueCmdBlock__update_this_type__Scope( RogueClassCmdBlock* THIS, RogueClassScope* scope_0 );
RogueClassCmdBlock* RogueCmdBlock__init_object( RogueClassCmdBlock* THIS );
RogueClassCmdBlock* RogueCmdBlock__init__Token_Int32( RogueClassCmdBlock* THIS, RogueClassToken* _auto_1775_0, RogueInt32 _auto_1776_1 );
RogueClassCmdBlock* RogueCmdBlock__init__Token_CmdStatementList_Int32( RogueClassCmdBlock* THIS, RogueClassToken* _auto_1777_0, RogueClassCmdStatementList* _auto_1778_1, RogueInt32 _auto_1779_2 );
RogueString* RogueCmdTaskControlSectionArray__type_name( RogueArray* THIS );
RogueString* RogueString_DefinitionTableEntryList__to_String( RogueTableEntry_of_String_DefinitionList* THIS );
RogueString* RogueString_DefinitionTableEntryList__type_name( RogueTableEntry_of_String_DefinitionList* THIS );
RogueTableEntry_of_String_DefinitionList* RogueString_DefinitionTableEntryList__init_object( RogueTableEntry_of_String_DefinitionList* THIS );
RogueTableEntry_of_String_DefinitionList* RogueString_DefinitionTableEntryList__init__Int32_String_DefinitionTableEntry( RogueTableEntry_of_String_DefinitionList* THIS, RogueInt32 initial_capacity_0, RogueClassString_DefinitionTableEntry* initial_value_1 );
RogueTableEntry_of_String_DefinitionList* RogueString_DefinitionTableEntryList__add__String_DefinitionTableEntry( RogueTableEntry_of_String_DefinitionList* THIS, RogueClassString_DefinitionTableEntry* value_0 );
RogueInt32 RogueString_DefinitionTableEntryList__capacity( RogueTableEntry_of_String_DefinitionList* THIS );
RogueTableEntry_of_String_DefinitionList* RogueString_DefinitionTableEntryList__reserve__Int32( RogueTableEntry_of_String_DefinitionList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_DefinitionTableEntry__type_name( RogueClassString_DefinitionTableEntry* THIS );
RogueClassString_DefinitionTableEntry* RogueString_DefinitionTableEntry__init__String_Definition_Int32( RogueClassString_DefinitionTableEntry* THIS, RogueString* _key_0, RogueClassDefinition* _value_1, RogueInt32 _hash_2 );
RogueClassString_DefinitionTableEntry* RogueString_DefinitionTableEntry__init_object( RogueClassString_DefinitionTableEntry* THIS );
RogueString* RogueString_DefinitionTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_PropertyTableEntryList__to_String( RogueTableEntry_of_String_PropertyList* THIS );
RogueString* RogueString_PropertyTableEntryList__type_name( RogueTableEntry_of_String_PropertyList* THIS );
RogueTableEntry_of_String_PropertyList* RogueString_PropertyTableEntryList__init_object( RogueTableEntry_of_String_PropertyList* THIS );
RogueTableEntry_of_String_PropertyList* RogueString_PropertyTableEntryList__init__Int32_String_PropertyTableEntry( RogueTableEntry_of_String_PropertyList* THIS, RogueInt32 initial_capacity_0, RogueClassString_PropertyTableEntry* initial_value_1 );
RogueTableEntry_of_String_PropertyList* RogueString_PropertyTableEntryList__add__String_PropertyTableEntry( RogueTableEntry_of_String_PropertyList* THIS, RogueClassString_PropertyTableEntry* value_0 );
RogueInt32 RogueString_PropertyTableEntryList__capacity( RogueTableEntry_of_String_PropertyList* THIS );
RogueTableEntry_of_String_PropertyList* RogueString_PropertyTableEntryList__reserve__Int32( RogueTableEntry_of_String_PropertyList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_PropertyTableEntry__type_name( RogueClassString_PropertyTableEntry* THIS );
RogueClassString_PropertyTableEntry* RogueString_PropertyTableEntry__init__String_Property_Int32( RogueClassString_PropertyTableEntry* THIS, RogueString* _key_0, RogueClassProperty* _value_1, RogueInt32 _hash_2 );
RogueClassString_PropertyTableEntry* RogueString_PropertyTableEntry__init_object( RogueClassString_PropertyTableEntry* THIS );
RogueString* RogueString_PropertyTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueString_CmdTable__to_String( RogueClassString_CmdTable* THIS );
RogueString* RogueString_CmdTable__type_name( RogueClassString_CmdTable* THIS );
RogueClassString_CmdTable* RogueString_CmdTable__init( RogueClassString_CmdTable* THIS );
RogueClassString_CmdTable* RogueString_CmdTable__init__Int32( RogueClassString_CmdTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_CmdTableEntry* RogueString_CmdTable__find__String( RogueClassString_CmdTable* THIS, RogueString* key_0 );
RogueClassCmd* RogueString_CmdTable__get__String( RogueClassString_CmdTable* THIS, RogueString* key_0 );
RogueClassString_CmdTable* RogueString_CmdTable__set__String_Cmd( RogueClassString_CmdTable* THIS, RogueString* key_0, RogueClassCmd* value_1 );
RogueStringBuilder* RogueString_CmdTable__print_to__StringBuilder( RogueClassString_CmdTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_CmdTable* RogueString_CmdTable__init_object( RogueClassString_CmdTable* THIS );
RogueString* RogueCmdCallStaticMethod__type_name( RogueClassCmdCallStaticMethod* THIS );
RogueClassCmd* RogueCmdCallStaticMethod__clone__CloneArgs( RogueClassCmdCallStaticMethod* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdCallStaticMethod__resolve__Scope( RogueClassCmdCallStaticMethod* THIS, RogueClassScope* scope_0 );
void RogueCmdCallStaticMethod__write_cpp__CPPWriter_Logical( RogueClassCmdCallStaticMethod* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdCallStaticMethod* RogueCmdCallStaticMethod__init_object( RogueClassCmdCallStaticMethod* THIS );
RogueString* RogueFnArgArray__type_name( RogueArray* THIS );
RogueString* RogueString_StringTableEntryList__to_String( RogueTableEntry_of_String_StringList* THIS );
RogueString* RogueString_StringTableEntryList__type_name( RogueTableEntry_of_String_StringList* THIS );
RogueTableEntry_of_String_StringList* RogueString_StringTableEntryList__init_object( RogueTableEntry_of_String_StringList* THIS );
RogueTableEntry_of_String_StringList* RogueString_StringTableEntryList__init__Int32_String_StringTableEntry( RogueTableEntry_of_String_StringList* THIS, RogueInt32 initial_capacity_0, RogueClassString_StringTableEntry* initial_value_1 );
RogueTableEntry_of_String_StringList* RogueString_StringTableEntryList__add__String_StringTableEntry( RogueTableEntry_of_String_StringList* THIS, RogueClassString_StringTableEntry* value_0 );
RogueInt32 RogueString_StringTableEntryList__capacity( RogueTableEntry_of_String_StringList* THIS );
RogueTableEntry_of_String_StringList* RogueString_StringTableEntryList__reserve__Int32( RogueTableEntry_of_String_StringList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_StringTableEntry__type_name( RogueClassString_StringTableEntry* THIS );
RogueClassString_StringTableEntry* RogueString_StringTableEntry__init__String_String_Int32( RogueClassString_StringTableEntry* THIS, RogueString* _key_0, RogueString* _value_1, RogueInt32 _hash_2 );
RogueClassString_StringTableEntry* RogueString_StringTableEntry__init_object( RogueClassString_StringTableEntry* THIS );
RogueString* RogueString_StringTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueDirectiveTokenType__type_name( RogueClassDirectiveTokenType* THIS );
RogueClassToken* RogueDirectiveTokenType__create_token__String_Int32_Int32( RogueClassDirectiveTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2 );
RogueLogical RogueDirectiveTokenType__is_directive( RogueClassDirectiveTokenType* THIS );
RogueClassDirectiveTokenType* RogueDirectiveTokenType__init_object( RogueClassDirectiveTokenType* THIS );
RogueString* RogueStructuralDirectiveTokenType__type_name( RogueClassStructuralDirectiveTokenType* THIS );
RogueClassToken* RogueStructuralDirectiveTokenType__create_token__String_Int32_Int32( RogueClassStructuralDirectiveTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2 );
RogueLogical RogueStructuralDirectiveTokenType__is_directive( RogueClassStructuralDirectiveTokenType* THIS );
RogueLogical RogueStructuralDirectiveTokenType__is_structure( RogueClassStructuralDirectiveTokenType* THIS );
RogueClassStructuralDirectiveTokenType* RogueStructuralDirectiveTokenType__init_object( RogueClassStructuralDirectiveTokenType* THIS );
RogueString* RogueEOLTokenType__type_name( RogueClassEOLTokenType* THIS );
RogueClassToken* RogueEOLTokenType__create_token__String_Int32_Int32( RogueClassEOLTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2 );
RogueClassToken* RogueEOLTokenType__create_token__String_Int32_Int32_String( RogueClassEOLTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueString* value_3 );
RogueLogical RogueEOLTokenType__is_structure( RogueClassEOLTokenType* THIS );
RogueClassEOLTokenType* RogueEOLTokenType__init_object( RogueClassEOLTokenType* THIS );
RogueString* RogueStructureTokenType__type_name( RogueClassStructureTokenType* THIS );
RogueClassToken* RogueStructureTokenType__create_token__String_Int32_Int32( RogueClassStructureTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2 );
RogueLogical RogueStructureTokenType__is_structure( RogueClassStructureTokenType* THIS );
RogueClassStructureTokenType* RogueStructureTokenType__init_object( RogueClassStructureTokenType* THIS );
RogueString* RogueNativeCodeTokenType__type_name( RogueClassNativeCodeTokenType* THIS );
RogueClassToken* RogueNativeCodeTokenType__create_token__String_Int32_Int32_String( RogueClassNativeCodeTokenType* THIS, RogueString* filepath_0, RogueInt32 line_1, RogueInt32 column_2, RogueString* value_3 );
RogueClassToken* RogueNativeCodeTokenType__create_token__Token_String( RogueClassNativeCodeTokenType* THIS, RogueClassToken* existing_0, RogueString* value_1 );
RogueClassNativeCodeTokenType* RogueNativeCodeTokenType__init_object( RogueClassNativeCodeTokenType* THIS );
RogueString* RogueOpWithAssignTokenType__type_name( RogueClassOpWithAssignTokenType* THIS );
RogueLogical RogueOpWithAssignTokenType__is_op_with_assign( RogueClassOpWithAssignTokenType* THIS );
RogueClassOpWithAssignTokenType* RogueOpWithAssignTokenType__init_object( RogueClassOpWithAssignTokenType* THIS );
RogueString* RogueString_TokenListTable__to_String( RogueClassString_TokenListTable* THIS );
RogueString* RogueString_TokenListTable__type_name( RogueClassString_TokenListTable* THIS );
RogueClassString_TokenListTable* RogueString_TokenListTable__init( RogueClassString_TokenListTable* THIS );
RogueClassString_TokenListTable* RogueString_TokenListTable__init__Int32( RogueClassString_TokenListTable* THIS, RogueInt32 bin_count_0 );
RogueClassString_TokenListTableEntry* RogueString_TokenListTable__find__String( RogueClassString_TokenListTable* THIS, RogueString* key_0 );
RogueTokenList* RogueString_TokenListTable__get__String( RogueClassString_TokenListTable* THIS, RogueString* key_0 );
RogueClassString_TokenListTable* RogueString_TokenListTable__set__String_TokenList( RogueClassString_TokenListTable* THIS, RogueString* key_0, RogueTokenList* value_1 );
RogueStringBuilder* RogueString_TokenListTable__print_to__StringBuilder( RogueClassString_TokenListTable* THIS, RogueStringBuilder* buffer_0 );
RogueClassString_TokenListTable* RogueString_TokenListTable__init_object( RogueClassString_TokenListTable* THIS );
RogueString* RoguePreprocessorTokenReader__type_name( RogueClassPreprocessorTokenReader* THIS );
RogueClassPreprocessorTokenReader* RoguePreprocessorTokenReader__init__TokenList( RogueClassPreprocessorTokenReader* THIS, RogueTokenList* _auto_2221_0 );
RogueClassError* RoguePreprocessorTokenReader__error__String( RogueClassPreprocessorTokenReader* THIS, RogueString* message_0 );
void RoguePreprocessorTokenReader__expand_definition__Token( RogueClassPreprocessorTokenReader* THIS, RogueClassToken* t_0 );
RogueLogical RoguePreprocessorTokenReader__has_another( RogueClassPreprocessorTokenReader* THIS );
RogueLogical RoguePreprocessorTokenReader__next_is__TokenType( RogueClassPreprocessorTokenReader* THIS, RogueClassTokenType* type_0 );
RogueClassToken* RoguePreprocessorTokenReader__peek( RogueClassPreprocessorTokenReader* THIS );
RogueClassToken* RoguePreprocessorTokenReader__peek__Int32( RogueClassPreprocessorTokenReader* THIS, RogueInt32 num_ahead_0 );
void RoguePreprocessorTokenReader__push__Token( RogueClassPreprocessorTokenReader* THIS, RogueClassToken* t_0 );
RogueClassToken* RoguePreprocessorTokenReader__read( RogueClassPreprocessorTokenReader* THIS );
RogueString* RoguePreprocessorTokenReader__read_identifier( RogueClassPreprocessorTokenReader* THIS );
RogueClassPreprocessorTokenReader* RoguePreprocessorTokenReader__init_object( RogueClassPreprocessorTokenReader* THIS );
RogueString* RogueCmdWhichCaseArray__type_name( RogueArray* THIS );
RogueString* RogueCmdSwitch__type_name( RogueClassCmdSwitch* THIS );
RogueClassCmdSwitch* RogueCmdSwitch__clone__CloneArgs( RogueClassCmdSwitch* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdSwitch__resolve__Scope( RogueClassCmdSwitch* THIS, RogueClassScope* scope_0 );
void RogueCmdSwitch__write_cpp__CPPWriter_Logical( RogueClassCmdSwitch* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdSwitch__trace_used_code( RogueClassCmdSwitch* THIS );
void RogueCmdSwitch__update_this_type__Scope( RogueClassCmdSwitch* THIS, RogueClassScope* scope_0 );
RogueClassCmdSwitch* RogueCmdSwitch__init_object( RogueClassCmdSwitch* THIS );
RogueClassCmdSwitch* RogueCmdSwitch__init__Token_Cmd_CmdWhichCaseList_CmdWhichCase_Int32( RogueClassCmdSwitch* THIS, RogueClassToken* _auto_2235_0, RogueClassCmd* _auto_2236_1, RogueCmdWhichCaseList* _auto_2237_2, RogueClassCmdWhichCase* _auto_2238_3, RogueInt32 _auto_2239_4 );
RogueString* RogueCmdCatchArray__type_name( RogueArray* THIS );
RogueString* RogueCmdReadArrayElement__type_name( RogueClassCmdReadArrayElement* THIS );
RogueClassCmd* RogueCmdReadArrayElement__clone__CloneArgs( RogueClassCmdReadArrayElement* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdReadArrayElement__resolve__Scope( RogueClassCmdReadArrayElement* THIS, RogueClassScope* scope_0 );
RogueClassCmd* RogueCmdReadArrayElement__resolve_adjust__Scope_Int32( RogueClassCmdReadArrayElement* THIS, RogueClassScope* scope_0, RogueInt32 delta_1 );
RogueClassType* Rogue_CmdReadArrayElement__type( RogueClassCmdReadArrayElement* THIS );
void RogueCmdReadArrayElement__write_cpp__CPPWriter_Logical( RogueClassCmdReadArrayElement* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdReadArrayElement__trace_used_code( RogueClassCmdReadArrayElement* THIS );
void RogueCmdReadArrayElement__update_this_type__Scope( RogueClassCmdReadArrayElement* THIS, RogueClassScope* scope_0 );
RogueClassCmdReadArrayElement* RogueCmdReadArrayElement__init_object( RogueClassCmdReadArrayElement* THIS );
RogueClassCmdReadArrayElement* RogueCmdReadArrayElement__init__Token_Cmd_Cmd( RogueClassCmdReadArrayElement* THIS, RogueClassToken* _auto_2336_0, RogueClassCmd* _auto_2337_1, RogueClassCmd* _auto_2338_2 );
RogueString* RogueCmdWriteArrayElement__type_name( RogueClassCmdWriteArrayElement* THIS );
RogueClassCmd* RogueCmdWriteArrayElement__clone__CloneArgs( RogueClassCmdWriteArrayElement* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdWriteArrayElement__resolve__Scope( RogueClassCmdWriteArrayElement* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdWriteArrayElement__type( RogueClassCmdWriteArrayElement* THIS );
void RogueCmdWriteArrayElement__write_cpp__CPPWriter_Logical( RogueClassCmdWriteArrayElement* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdWriteArrayElement__trace_used_code( RogueClassCmdWriteArrayElement* THIS );
void RogueCmdWriteArrayElement__update_this_type__Scope( RogueClassCmdWriteArrayElement* THIS, RogueClassScope* scope_0 );
RogueClassCmdWriteArrayElement* RogueCmdWriteArrayElement__init_object( RogueClassCmdWriteArrayElement* THIS );
RogueClassCmdWriteArrayElement* RogueCmdWriteArrayElement__init__Token_Cmd_Cmd_Cmd( RogueClassCmdWriteArrayElement* THIS, RogueClassToken* _auto_2339_0, RogueClassCmd* _auto_2340_1, RogueClassCmd* _auto_2341_2, RogueClassCmd* _auto_2342_3 );
RogueString* RogueCmdConvertToPrimitiveType__type_name( RogueClassCmdConvertToPrimitiveType* THIS );
RogueClassCmd* RogueCmdConvertToPrimitiveType__clone__CloneArgs( RogueClassCmdConvertToPrimitiveType* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdConvertToPrimitiveType__resolve__Scope( RogueClassCmdConvertToPrimitiveType* THIS, RogueClassScope* scope_0 );
void RogueCmdConvertToPrimitiveType__write_cpp__CPPWriter_Logical( RogueClassCmdConvertToPrimitiveType* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
RogueClassCmdConvertToPrimitiveType* RogueCmdConvertToPrimitiveType__init_object( RogueClassCmdConvertToPrimitiveType* THIS );
RogueString* RogueCmdSelectCaseArray__type_name( RogueArray* THIS );
RogueString* RogueCmdSelectCaseListReader__type_name( RogueClassCmdSelectCaseListReader* THIS );
RogueLogical RogueCmdSelectCaseListReader__has_another( RogueClassCmdSelectCaseListReader* THIS );
RogueClassCmdSelectCase* RogueCmdSelectCaseListReader__read( RogueClassCmdSelectCaseListReader* THIS );
RogueClassCmdSelectCaseListReader* RogueCmdSelectCaseListReader__init__CmdSelectCaseList_Int32( RogueClassCmdSelectCaseListReader* THIS, RogueCmdSelectCaseList* _auto_2399_0, RogueInt32 _auto_2400_1 );
RogueClassCmdSelectCaseListReader* RogueCmdSelectCaseListReader__init_object( RogueClassCmdSelectCaseListReader* THIS );
RogueString* RogueCmdAdjustGlobal__type_name( RogueClassCmdAdjustGlobal* THIS );
RogueClassCmd* RogueCmdAdjustGlobal__clone__CloneArgs( RogueClassCmdAdjustGlobal* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAdjustGlobal__resolve__Scope( RogueClassCmdAdjustGlobal* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdAdjustGlobal__type( RogueClassCmdAdjustGlobal* THIS );
void RogueCmdAdjustGlobal__write_cpp__CPPWriter_Logical( RogueClassCmdAdjustGlobal* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdAdjustGlobal__trace_used_code( RogueClassCmdAdjustGlobal* THIS );
void RogueCmdAdjustGlobal__update_this_type__Scope( RogueClassCmdAdjustGlobal* THIS, RogueClassScope* scope_0 );
RogueClassCmdAdjustGlobal* RogueCmdAdjustGlobal__init_object( RogueClassCmdAdjustGlobal* THIS );
RogueClassCmdAdjustGlobal* RogueCmdAdjustGlobal__init__Token_Property_Int32( RogueClassCmdAdjustGlobal* THIS, RogueClassToken* _auto_2610_0, RogueClassProperty* _auto_2611_1, RogueInt32 _auto_2612_2 );
RogueString* RogueCmdAdjustProperty__type_name( RogueClassCmdAdjustProperty* THIS );
RogueClassCmd* RogueCmdAdjustProperty__clone__CloneArgs( RogueClassCmdAdjustProperty* THIS, RogueClassCloneArgs* clone_args_0 );
RogueClassCmd* RogueCmdAdjustProperty__resolve__Scope( RogueClassCmdAdjustProperty* THIS, RogueClassScope* scope_0 );
RogueClassType* Rogue_CmdAdjustProperty__type( RogueClassCmdAdjustProperty* THIS );
void RogueCmdAdjustProperty__write_cpp__CPPWriter_Logical( RogueClassCmdAdjustProperty* THIS, RogueClassCPPWriter* writer_0, RogueLogical is_statement_1 );
void RogueCmdAdjustProperty__trace_used_code( RogueClassCmdAdjustProperty* THIS );
void RogueCmdAdjustProperty__update_this_type__Scope( RogueClassCmdAdjustProperty* THIS, RogueClassScope* scope_0 );
RogueClassCmdAdjustProperty* RogueCmdAdjustProperty__init_object( RogueClassCmdAdjustProperty* THIS );
RogueClassCmdAdjustProperty* RogueCmdAdjustProperty__init__Token_Cmd_Property_Int32( RogueClassCmdAdjustProperty* THIS, RogueClassToken* _auto_2613_0, RogueClassCmd* _auto_2614_1, RogueClassProperty* _auto_2615_2, RogueInt32 _auto_2616_3 );
RogueString* RogueString_CmdTableEntryList__to_String( RogueTableEntry_of_String_CmdList* THIS );
RogueString* RogueString_CmdTableEntryList__type_name( RogueTableEntry_of_String_CmdList* THIS );
RogueTableEntry_of_String_CmdList* RogueString_CmdTableEntryList__init_object( RogueTableEntry_of_String_CmdList* THIS );
RogueTableEntry_of_String_CmdList* RogueString_CmdTableEntryList__init__Int32_String_CmdTableEntry( RogueTableEntry_of_String_CmdList* THIS, RogueInt32 initial_capacity_0, RogueClassString_CmdTableEntry* initial_value_1 );
RogueTableEntry_of_String_CmdList* RogueString_CmdTableEntryList__add__String_CmdTableEntry( RogueTableEntry_of_String_CmdList* THIS, RogueClassString_CmdTableEntry* value_0 );
RogueInt32 RogueString_CmdTableEntryList__capacity( RogueTableEntry_of_String_CmdList* THIS );
RogueTableEntry_of_String_CmdList* RogueString_CmdTableEntryList__reserve__Int32( RogueTableEntry_of_String_CmdList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_CmdTableEntry__type_name( RogueClassString_CmdTableEntry* THIS );
RogueClassString_CmdTableEntry* RogueString_CmdTableEntry__init__String_Cmd_Int32( RogueClassString_CmdTableEntry* THIS, RogueString* _key_0, RogueClassCmd* _value_1, RogueInt32 _hash_2 );
RogueClassString_CmdTableEntry* RogueString_CmdTableEntry__init_object( RogueClassString_CmdTableEntry* THIS );
RogueString* RogueString_CmdTableEntryArray__type_name( RogueArray* THIS );
RogueString* RogueNativeCodeToken__to_String( RogueClassNativeCodeToken* THIS );
RogueString* RogueNativeCodeToken__type_name( RogueClassNativeCodeToken* THIS );
RogueString* RogueNativeCodeToken__quoted_name( RogueClassNativeCodeToken* THIS );
RogueClassNativeCodeToken* RogueNativeCodeToken__init_object( RogueClassNativeCodeToken* THIS );
RogueClassNativeCodeToken* RogueNativeCodeToken__init__TokenType_String( RogueClassNativeCodeToken* THIS, RogueClassTokenType* _auto_2818_0, RogueString* _auto_2819_1 );
RogueString* RogueString_TokenListTableEntryList__to_String( RogueTableEntry_of_String_TokenListList* THIS );
RogueString* RogueString_TokenListTableEntryList__type_name( RogueTableEntry_of_String_TokenListList* THIS );
RogueTableEntry_of_String_TokenListList* RogueString_TokenListTableEntryList__init_object( RogueTableEntry_of_String_TokenListList* THIS );
RogueTableEntry_of_String_TokenListList* RogueString_TokenListTableEntryList__init__Int32_String_TokenListTableEntry( RogueTableEntry_of_String_TokenListList* THIS, RogueInt32 initial_capacity_0, RogueClassString_TokenListTableEntry* initial_value_1 );
RogueTableEntry_of_String_TokenListList* RogueString_TokenListTableEntryList__add__String_TokenListTableEntry( RogueTableEntry_of_String_TokenListList* THIS, RogueClassString_TokenListTableEntry* value_0 );
RogueInt32 RogueString_TokenListTableEntryList__capacity( RogueTableEntry_of_String_TokenListList* THIS );
RogueTableEntry_of_String_TokenListList* RogueString_TokenListTableEntryList__reserve__Int32( RogueTableEntry_of_String_TokenListList* THIS, RogueInt32 additional_count_0 );
RogueString* RogueString_TokenListTableEntry__type_name( RogueClassString_TokenListTableEntry* THIS );
RogueClassString_TokenListTableEntry* RogueString_TokenListTableEntry__init__String_TokenList_Int32( RogueClassString_TokenListTableEntry* THIS, RogueString* _key_0, RogueTokenList* _value_1, RogueInt32 _hash_2 );
RogueClassString_TokenListTableEntry* RogueString_TokenListTableEntry__init_object( RogueClassString_TokenListTableEntry* THIS );
RogueString* RogueString_TokenListTableEntryArray__type_name( RogueArray* THIS );

void Rogue_trace();


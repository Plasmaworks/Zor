//=============================================================================
//  RogueType.h
//
//  2015.08.30 by Abe Pralle
//=============================================================================
#pragma once
#ifndef ROGUE_TYPE_H
#define ROGUE_TYPE_H

//-----------------------------------------------------------------------------
//  Dynamic Functions
//-----------------------------------------------------------------------------
typedef int          (*RogueEqualsFn)( void* object_a, void* object_b );
typedef int          (*RogueEqualsCStringFn)( void* object_a, const char* b );
typedef RogueInteger (*RogueHashCodeFn)( void* object );
typedef void         (*RoguePrintFn)( void* object, RogueStringBuilder* builder );
typedef void         (*RogueTraceFn)( void* object );

int          RogueEqualsFn_default( void* object_a, void* object_b );
int          RogueEqualsCStringFn_default( void* object_a, const char* b );
RogueInteger RogueHashCodeFn_default( void* object );
void         RoguePrintFn_default( void* object, RogueStringBuilder* builder );


//-----------------------------------------------------------------------------
//  RogueType
//-----------------------------------------------------------------------------
struct RogueType
{
  RogueVM*      vm;
  char*         name;
  RogueString*  name_object;
  RogueInteger  object_size;
  RogueInteger  element_size;

  RogueEqualsFn        equals;
  RogueEqualsCStringFn equals_c_string;
  RogueHashCodeFn      hash_code;
  RoguePrintFn         print;
  RogueTraceFn         trace;
};

RogueType* RogueType_create( RogueVM* vm, const char* name, RogueInteger object_size );
RogueType* RogueType_delete( RogueType* THIS );

void* RogueType_create_object( RogueType* THIS, RogueInteger size );

#endif // ROGUE_TYPE_H

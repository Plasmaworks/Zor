//=============================================================================
//  RogueCmd.h
//
//  2015.08.29 by Abe Pralle
//=============================================================================
#pragma once
#ifndef ROGUE_CMD_H
#define ROGUE_CMD_H

#include "Rogue.h"

//-----------------------------------------------------------------------------
//  RogueCmdType
//-----------------------------------------------------------------------------
struct RogueCmdType
{
  RogueVM*       vm;
  RogueTokenType token_type;
  RogueInteger   object_size;
};

RogueCmdType* RogueCmdType_create( RogueVM* vm, RogueTokenType token_type, RogueInteger object_size );

//-----------------------------------------------------------------------------
//  RogueCmd
//-----------------------------------------------------------------------------
struct RogueCmd
{
  RogueAllocation allocation;
  RogueCmdType*   type;
};

void* RogueCmd_create( RogueCmdType* of_type );

#endif // ROGUE_CMD_H

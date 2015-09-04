//=============================================================================
//  RogueParseReader.c
//
//  2015.09.03 by Abe Pralle
//=============================================================================
#include "Rogue.h"

//-----------------------------------------------------------------------------
//  ParseReader Type
//-----------------------------------------------------------------------------
RogueType* RogueTypeParseReader_create( RogueVM* vm )
{
  RogueType* THIS = RogueType_create( vm, "ParseReader", sizeof(RogueParseReader) );
  return THIS;
}

//-----------------------------------------------------------------------------
//  ParseReader Object
//-----------------------------------------------------------------------------
RogueParseReader* RogueParseReader_create( RogueVM* vm, RogueString* filepath )
{
  RogueParseReader* reader = RogueType_create_object( vm->type_ParseReader, -1 );
  reader->filepath = RogueVM_consolidate_string( vm, filepath );
  reader->position.line = 1;
  reader->position.column = 1;

  {
    RogueInteger size;
    FILE* fp = fopen( RogueString_to_c_string(filepath), "rb" );
    if (fp)
    {
      RogueArray* file_bytes;
      RogueInteger decoded_count;

      fseek( fp, 0, SEEK_END );
      size = (RogueInteger) ftell( fp );
      fseek( fp, 0, SEEK_SET );

      file_bytes = RogueByteArray_create( vm, size );
      fread( file_bytes->bytes, 1, size, fp );
      fclose( fp );

      decoded_count = RogueUTF8_decoded_count( (char*) file_bytes->bytes, size );
      reader->data = RogueCharacterArray_create( vm, decoded_count );
      RogueUTF8_decode( (char*) file_bytes->bytes, size, reader->data->characters );

      reader->count = size;
    }
  }

  return reader;
}

int RogueParseReader_has_another( RogueParseReader* THIS )
{
  return (THIS->position.index < THIS->count);
}

RogueCharacter RogueParseReader_peek( RogueParseReader* THIS, RogueInteger lookahead )
{
  RogueInteger index = THIS->position.index + lookahead;
  if (index >= THIS->count) return 0;
  return THIS->data->characters[ index ];
}

RogueCharacter RogueParseReader_read( RogueParseReader* THIS )
{
  RogueCharacter result = THIS->data->characters[ THIS->position.index++ ];
  switch (result)
  {
    case '\t':
      THIS->position.column += 2;
      return result;

    case '\n':
      THIS->position.column = 1;
      ++THIS->position.line;
      return result;

    default:
      ++THIS->position.column;
      return result;
  }
}


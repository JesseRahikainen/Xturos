#include "text.h"

// TODO: Ability to cache string images

#include <SDL_rwops.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../System/memory.h"

#include <stb_rect_pack.h>


//#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>


#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)	((void)(u),mem_Allocate(x))
#define STBTT_free(x,u)		((void)(u),mem_Release(x))
#include <stb_truetype.h>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(sz)        mem_Allocate(sz)
#define STBIW_REALLOC(p,newsz)  mem_Resize(p,newsz)
#define STBIW_FREE(p)           mem_Release(p)

#pragma warning( push )
#pragma warning( disable : 4204 )
#include <stb_image_write.h>
#pragma warning( pop )

#include "../Utils/stretchyBuffer.h"
#include "../Graphics/images.h"
#include "../Math/mathUtil.h"

#include "../System/platformLog.h"

#include "../System/jobQueue.h"

#include "../Graphics/gfxUtil.h"

typedef struct {
	int32_t codepoint;
	int imageID;
	float advance;
} Glyph;

static const uint32_t LINE_FEED = 0xA;

// used for when we want to modify a string but don't want to change what was passed in
static uint32_t* sbStringCodepointBuffer = NULL;

#define MAX_FONTS 32
typedef struct {
	// this will be sorted by the codepoint entry in all the structs, make it easier to search
	//  could also preprocess strings to just be a list of indices into the buffer
	// will be a stretchy buffer
	Glyph* glyphsBuffer;
	int packageID;

	int missingCharGlyphIdx;

	float descent;
	float lineGap;
	float ascent;

	float nextLineDescent;

	int baseSize;
} Font;

static int missingChar = 0x3F; // '?'

static Font fonts[MAX_FONTS] = { 0 };
// TODO: for localization we can define stbtt_pack_range for each language and link them together to be loaded
stbtt_pack_range fontPackRange = { 0 };

// Sets up the default codepoints to load and clears out any currently loaded fonts.
int txt_Init( void )
{
	// add the visible ASCII characters
	for( int c = 0x20; c <= 0x7E; ++c ) {
		txt_AddCharacterToLoad( c );
	}

	for( int i = 0; i < MAX_FONTS; ++i ) {
		if( fonts[i].glyphsBuffer != NULL ) {
			sb_Release( fonts[i].glyphsBuffer );
		}
		fonts[i].glyphsBuffer = NULL;
	}

	sb_Add( sbStringCodepointBuffer, 1024 );

	return 0;
}

// Adds a codepoint to the set of codepoints to get glyphs for when loading a font. This does
//  not currently cause any currently loaded fonts to be reloaded, so they will not be able to
//  display this codepoint if it had not previously been added.
void txt_AddCharacterToLoad( int c )
{
	// check to see if the character already exists
	int cnt = sb_Count( fontPackRange.array_of_unicode_codepoints );
	for( int i = 0; i < cnt; ++i ) {
		if( fontPackRange.array_of_unicode_codepoints[i] == c ) {
			return;
		}
	}

	sb_Push( fontPackRange.array_of_unicode_codepoints, c );
	fontPackRange.num_chars = sb_Count( fontPackRange.array_of_unicode_codepoints );
}

int findUnusedFontID( void )
{
	int newFont = 0;

	while( fonts[newFont].glyphsBuffer != NULL ) {
		++newFont;
	}
	if( newFont >= MAX_FONTS ) {
		newFont = -1;
	}

	return newFont;
}

// Loads the font at fileName, with a height of pixelHeight.
//  Returns an ID to be used when displaying a string, returns -1 if there was an issue.
int txt_LoadFont( const char* fileName, int pixelHeight )
{
	uint8_t* buffer = NULL;
	unsigned char* bmpBuffer = NULL;
	stbtt_fontinfo font;
	int newFont = 0;
	Vector2* mins = NULL;
	Vector2* maxes = NULL;
	int* retIDs;
	Glyph* glyphStorage = NULL; // temporary storage, used to reduce memory fragmentation

	// find an unused font ID
	newFont = findUnusedFontID( );
	if( newFont < 0 ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to find empty font to use for %s", fileName );
		goto clean_up;
	}

	sb_Add( glyphStorage, fontPackRange.num_chars );
	if( glyphStorage == NULL ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to allocate glyphs for %s", fileName );
		goto clean_up;
	}

	// create storage for all the characters
	fontPackRange.chardata_for_range = mem_Allocate( sizeof( stbtt_packedchar ) * fontPackRange.num_chars );
	fontPackRange.font_size = (float)pixelHeight;
	if( fontPackRange.chardata_for_range == NULL ) {
		llog( LOG_ERROR, "Error allocating range data for %s", fileName );
		newFont = -1;
		goto clean_up;
	}
	
	// now load the file
	//  some temporary memory for loading the file
	size_t bufferSize = 1024 * 1024;
	buffer = mem_Allocate( bufferSize * sizeof( uint8_t ) ); // megabyte sized buffer, should never load a file larger than this
	if( buffer == NULL ) {
		llog( LOG_WARN, "Error allocating font data buffer for %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		llog( LOG_ERROR, "Error opening font file %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	size_t numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( uint8_t ), bufferSize );
	if( numRead >= bufferSize ) {
		llog( LOG_ERROR, "Too much data read in from file %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	// get some of the basic font stuff, need to do this so we can handle multiple lines of text
	int ascent, descent, lineGap;
	float scale;
	stbtt_InitFont( &font, buffer, 0 );
	stbtt_GetFontVMetrics( &font, &ascent, &descent, &lineGap );
	scale = stbtt_ScaleForPixelHeight( &font, (float)pixelHeight );
	fonts[newFont].ascent = (float)ascent * scale;
	fonts[newFont].descent = (float)descent * scale;
	fonts[newFont].lineGap = (float)lineGap * scale;
	fonts[newFont].nextLineDescent = fonts[newFont].ascent - fonts[newFont].descent + fonts[newFont].lineGap;
	fonts[newFont].baseSize = pixelHeight;
	
	// pack and create the 1 channel bitmap
	// TODO: Better estimates of the width and height (find largest glyph we'll use and calculate using that?)
	// TODO: clip the height to what we actually use
	stbtt_pack_context packContext;
	int bmpWidth = 1024;
	int bmpHeight = 1024;
	bmpBuffer = mem_Allocate( sizeof( unsigned char ) * bmpWidth * bmpHeight ); // the 4 allows room for expansion
	if( bmpBuffer == NULL ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to allocate bitmap memory for %s", fileName );
		goto clean_up;
	}
	// TODO: Test oversampling
	if( !stbtt_PackBegin( &packContext, bmpBuffer, bmpWidth, bmpHeight, 0, 1, NULL ) ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to begin packing ranges for %s", fileName );
		goto clean_up;
	}
	int wasPacked = stbtt_PackFontRanges( &packContext, (unsigned char*)buffer, 0, &fontPackRange, 1 );
	stbtt_PackEnd( &packContext );
	if( !wasPacked ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to pack ranges for %s", fileName );
		goto clean_up;
	}

	// create all the glyphs for displaying
	mins = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	maxes = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	retIDs = mem_Allocate( sizeof( int ) * fontPackRange.num_chars );
	if( ( mins == NULL ) || ( maxes == NULL ) || ( retIDs == NULL ) ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to allocate image data for %s", fileName );
		goto clean_up;
	}

	// extract all the rectangles so we can split the image correctly
	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		mins[i].x = fontPackRange.chardata_for_range[i].x0;
		mins[i].y = fontPackRange.chardata_for_range[i].y0;
		maxes[i].x = fontPackRange.chardata_for_range[i].x1;
		maxes[i].y = fontPackRange.chardata_for_range[i].y1;
	}

	// create the images for the glyphs and record everything
	//  clamp to either on or off
	fonts[newFont].packageID = img_SplitAlphaBitmap( bmpBuffer, bmpWidth, bmpHeight, fontPackRange.num_chars, ST_ALPHA_ONLY, mins, maxes, retIDs );
	if( fonts[newFont].packageID < 0 ) {
		llog( LOG_ERROR, "Unable to split images for font %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	fonts[newFont].glyphsBuffer = glyphStorage;

	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		fonts[newFont].glyphsBuffer[i].codepoint = fontPackRange.array_of_unicode_codepoints[i];
		fonts[newFont].glyphsBuffer[i].imageID = retIDs[i];
		fonts[newFont].glyphsBuffer[i].advance = fontPackRange.chardata_for_range[i].xadvance;

		if( fonts[newFont].glyphsBuffer[i].codepoint == missingChar ) {
			fonts[newFont].missingCharGlyphIdx = i;
		}

		// set the offset for each glyph, the x0, y0, x1, and y1 of the quad determines the coordinates of the rectangle to use to render
		//  the glyph as an offset of it's position, so we just set the offset as the middle point of that rectangle
		stbtt_aligned_quad quad;
		float dummyX = 0.0f;
		float dummyY = 0.0f;
		Vector2 offset = { 0.0f, 0.0f };
		stbtt_GetPackedQuad( fontPackRange.chardata_for_range, bmpWidth, bmpHeight, i, &dummyX, &dummyY, &quad, 0 );

		offset.x = ( quad.x0 + quad.x1 ) / 2.0f;
		offset.y = ( quad.y0 + quad.y1 ) / 2.0f;
		img_SetOffset( retIDs[i], offset );
	}

	// TODO: get a way to do this with fewer temporary allocations
clean_up:
	mem_Release( buffer );
	mem_Release( bmpBuffer );
	mem_Release( fontPackRange.chardata_for_range );
	mem_Release( mins );
	mem_Release( maxes );
	fontPackRange.chardata_for_range = NULL;
	fontPackRange.font_size = 0.0f;

	if( newFont < 0 ) {
		// creating font failed, release pre-allocated storage
		sb_Release( glyphStorage );
	}

	return newFont;
}

typedef struct {
	const char* fileName;
	int* outFontID;

	stbtt_pack_range packRange; // in case stuff changes while loading the font

	// these are calculated after the font has been loaded and are saved out when the font is bound
	float descent;
	float lineGap;
	float ascent;
	float nextLineDescent;

	int bmpWidth;
	int bmpHeight;
	unsigned char* bmpBuffer;
} LoadFontData;

static void cleanUpLoadFontTaskData( LoadFontData* data )
{
	if( data == NULL ) return;

	mem_Release( data->packRange.chardata_for_range );
	mem_Release( data->packRange.array_of_unicode_codepoints );
	mem_Release( data->bmpBuffer );

	mem_Release( data );
}

static void bindFontTask( void* data )
{
	int newFont = 0;
	Vector2* mins = NULL;
	Vector2* maxes = NULL;
	int* retIDs = NULL;
	LoadFontData* fontData = NULL;

	if( data == NULL ) {
		llog( LOG_ERROR, "NULL data passed to bindFontTask." );
		goto clean_up;
	}
	fontData = (LoadFontData*)data;

	// find an unused font ID
	while( fonts[newFont].glyphsBuffer != NULL ) {
		++newFont;
	}
	if( newFont >= MAX_FONTS ) {
		llog( LOG_ERROR, "Unable to find empty font to use for %s", fontData->fileName );
		goto clean_up;
	}

	// got the data, bind the images and create the font
	// create all the glyphs for displaying
	mins = mem_Allocate( sizeof( Vector2 ) * fontData->packRange.num_chars );
	maxes = mem_Allocate( sizeof( Vector2 ) * fontData->packRange.num_chars );
	retIDs = mem_Allocate( sizeof( int ) * fontData->packRange.num_chars );
	if( ( mins == NULL ) || ( maxes == NULL ) || ( retIDs == NULL ) ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to allocate image data for %s", fontData->fileName );
		goto clean_up;
	}

	// extract all the rectangles so we can split the image correctly
	for( int i = 0; i < fontData->packRange.num_chars; ++i ) {
		mins[i].x = fontData->packRange.chardata_for_range[i].x0;
		mins[i].y = fontData->packRange.chardata_for_range[i].y0;
		maxes[i].x = fontData->packRange.chardata_for_range[i].x1;
		maxes[i].y = fontData->packRange.chardata_for_range[i].y1;
	}

	// create the images for the glyphs and record everything
	//  clamp to either on or off
	fonts[newFont].packageID = img_SplitAlphaBitmap( fontData->bmpBuffer, fontData->bmpWidth, fontData->bmpHeight,
		fontData->packRange.num_chars, ST_ALPHA_ONLY, mins, maxes, retIDs );
	if( fonts[newFont].packageID < 0 ) {
		llog( LOG_ERROR, "Unable to split images for font %s", fontData->fileName );
		newFont = -1;
		goto clean_up;
	}

	sb_Add( fonts[newFont].glyphsBuffer, fontData->packRange.num_chars );
	if( fonts[newFont].glyphsBuffer == NULL ) {
		newFont = -1;
		llog( LOG_ERROR, "Unable to allocate glyphs for %s", fontData->fileName );
		goto clean_up;
	}

	fonts[newFont].ascent = fontData->ascent;
	fonts[newFont].descent = fontData->descent;
	fonts[newFont].lineGap = fontData->lineGap;
	fonts[newFont].nextLineDescent = fontData->nextLineDescent;

	for( int i = 0; i < fontData->packRange.num_chars; ++i ) {
		fonts[newFont].glyphsBuffer[i].codepoint = fontData->packRange.array_of_unicode_codepoints[i];
		fonts[newFont].glyphsBuffer[i].imageID = retIDs[i];
		fonts[newFont].glyphsBuffer[i].advance = fontData->packRange.chardata_for_range[i].xadvance;

		if( fonts[newFont].glyphsBuffer[i].codepoint == missingChar ) {
			fonts[newFont].missingCharGlyphIdx = i;
		}

		// set the offset for each glyph, the x0, y0, x1, and y1 of the quad determines the coordinates of the rectangle to use to render
		//  the glyph as an offset of it's position, so we just set the offset as the middle point of that rectangle
		stbtt_aligned_quad quad;
		float dummyX = 0.0f;
		float dummyY = 0.0f;
		Vector2 offset = { 0.0f, 0.0f };
		stbtt_GetPackedQuad( fontData->packRange.chardata_for_range, fontData->bmpWidth, fontData->bmpHeight, i, &dummyX, &dummyY, &quad, 0 );

		offset.x = ( quad.x0 + quad.x1 ) / 2.0f;
		offset.y = ( quad.y0 + quad.y1 ) / 2.0f;
		img_SetOffset( retIDs[i], offset );
	}

	// all done, set our new font
	(*(fontData->outFontID)) = newFont;

clean_up:
	mem_Release( mins );
	mem_Release( maxes );
	mem_Release( retIDs );

	cleanUpLoadFontTaskData( fontData );
}

static void loadFontTask( void* data )
{
	uint8_t* buffer = NULL;
	SDL_RWops* rwopsFile = NULL;

	if( data == NULL ) {
		return;
	}

	LoadFontData* fontData = (LoadFontData*)data;

	// load the actual data here
	size_t bufferSize = 1024 * 1024;
	buffer = mem_Allocate( bufferSize * sizeof( uint8_t ) ); // megabyte sized buffer, should never load a file larger than this
	if( buffer == NULL ) {
		llog( LOG_WARN, "Error allocating font data buffer for %s", fontData->fileName );
		goto failure;
	}

	rwopsFile = SDL_RWFromFile( fontData->fileName, "r" );
	if( rwopsFile == NULL ) {
		llog( LOG_ERROR, "Error opening font file %s", fontData->fileName );
		goto failure;
	}

	size_t numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( uint8_t ), bufferSize );
	if( numRead >= bufferSize ) {
		llog( LOG_ERROR, "Too much data read in from file %s", fontData->fileName );
		goto clean_up;
	}

	// get some of the basic font stuff, need to do this so we can handle multiple lines of text
	int ascent, descent, lineGap;
	float scale;
	stbtt_fontinfo font;
	stbtt_InitFont( &font, buffer, 0 );
	stbtt_GetFontVMetrics( &font, &ascent, &descent, &lineGap );
	scale = stbtt_ScaleForPixelHeight( &font, fontData->packRange.font_size );
	fontData->ascent = (float)ascent * scale;
	fontData->descent = (float)descent * scale;
	fontData->lineGap = (float)lineGap * scale;
	fontData->nextLineDescent = fontData->ascent - fontData->descent + fontData->lineGap;
	
	// pack and create the 1 channel bitmap
	// TODO: Better estimates of the width and height (find largest glyph we'll use and calculate using that?)
	// TODO: clip the height to what we actually use
	stbtt_pack_context packContext;
	fontData->bmpWidth = 1024;
	fontData->bmpHeight = 1024;
	fontData->bmpBuffer = mem_Allocate( sizeof( unsigned char ) * fontData->bmpWidth * fontData->bmpHeight ); // the 4 allows room for expansion
	if( fontData->bmpBuffer == NULL ) {
		llog( LOG_ERROR, "Unable to allocate bitmap memory for %s", fontData->fileName );
		goto failure;
	}
	// TODO: Test oversampling
	if( !stbtt_PackBegin( &packContext, fontData->bmpBuffer, fontData->bmpWidth, fontData->bmpHeight, 0, 1, NULL ) ) {
		llog( LOG_ERROR, "Unable to begin packing ranges for %s", fontData->fileName );
		goto failure;
	}

	int wasPacked = stbtt_PackFontRanges( &packContext, (unsigned char*)buffer, 0, &( fontData->packRange ), 1 );
	stbtt_PackEnd( &packContext );
	if( !wasPacked ) {
		llog( LOG_ERROR, "Unable to pack ranges for %s", fontData->fileName );
		goto failure;
	}

	if( !jq_AddMainThreadJob( bindFontTask, data ) ) {
		goto failure;
	} else {
		goto clean_up;
	}

failure:
	cleanUpLoadFontTaskData( fontData );

clean_up:
	mem_Release( buffer );
	SDL_RWclose( rwopsFile );
}

// Loads the font at file name on a seperate thread. Uses a height of pixelHeight.
// This will initialize a bunch of stuff but not do any of the loading.
//  Puts the resulting font ID into outFontID.
void txt_ThreadedLoadFont( const char* fileName, float pixelHeight, int* outFontID )
{
	(*outFontID) = -1;

	LoadFontData* data = mem_Allocate( sizeof( LoadFontData ) );
	if( data == NULL ) {
		llog( LOG_WARN, "Unable to create data for threaded font load for file %s", fileName );
		return;
	}

	// initalize all the data we'll need
	data->fileName = fileName;
	data->outFontID = outFontID;

	data->packRange.font_size = pixelHeight;
	data->packRange.num_chars = fontPackRange.num_chars;
	data->packRange.first_unicode_codepoint_in_range = fontPackRange.first_unicode_codepoint_in_range;

	data->packRange.chardata_for_range = mem_Allocate( sizeof( stbtt_packedchar ) * data->packRange.num_chars );

	size_t codePointsSize = sizeof( data->packRange.array_of_unicode_codepoints[0] ) * sb_Count( fontPackRange.array_of_unicode_codepoints );
	data->packRange.array_of_unicode_codepoints = mem_Allocate( codePointsSize );
	memcpy( data->packRange.array_of_unicode_codepoints, fontPackRange.array_of_unicode_codepoints, codePointsSize );

	data->bmpBuffer = NULL;

	if( !jq_AddJob( loadFontTask, data ) ) {
		cleanUpLoadFontTaskData( data );
	}
}

void txt_UnloadFont( int fontID )
{
	assert( fontID >= 0 );

	sb_Release( fonts[fontID].glyphsBuffer );
	fonts[fontID].glyphsBuffer = NULL;
	img_CleanPackage( fonts[fontID].packageID );
}

Glyph* getCodepointGlyph( int fontID, int codepoint )
{
	Font* font = &( fonts[fontID] );
	Glyph* out = NULL;
	int glyphCount = sb_Count( font->glyphsBuffer );
	// TODO: Make this faster
	for( int i = 0; i < glyphCount; ++i ) {
		if( font->glyphsBuffer[i].codepoint == codepoint ) {
			return &( font->glyphsBuffer[i] );
		}
	}
	return &( font->glyphsBuffer[ font->missingCharGlyphIdx ] );
}

// Gets the code point from a string, will advance the string past the current codepoint to the next.
uint32_t getUTF8CodePoint( const uint8_t** strData )
{
	// TODO: Get this working with actual UTF-8 and not just the ASCII subset
	//  https://tools.ietf.org/html/rfc3629
	uint32_t c = (**strData);
	++(*strData);
	return c;
}

// Gets the total width of the string once it's been rendered
float calcStringRenderWidth( const uint8_t* str, int fontID, float scale )
{
	float width = 0.0f;

	uint32_t codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( ( codepoint == 0 ) || ( codepoint == LINE_FEED ) ) {
			// end of string/line do nothing
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, codepoint );
			width += ( glyph->advance * scale );
		}
	} while( ( codepoint != 0 ) && ( codepoint != LINE_FEED ) );

	return width;
}

float calcCodepointsRenderWidth( const uint32_t* str, int fontID, float scale )
{
	float width = 0.0f;
	for( int i = 0; ( str[i] != 0 ) && ( str[i] != LINE_FEED ); ++i ) {
		width += getCodepointGlyph( fontID, str[i] )->advance;
	}
	return width * scale;
}

void positionStringStartX( const uint8_t* str, int fontID, HorizTextAlignment align, float scale, Vector2* inOutPos )
{
	switch( align ) {
	case HORIZ_ALIGN_RIGHT:
		inOutPos->x -= calcStringRenderWidth( str, fontID, scale );
		break;
	case HORIZ_ALIGN_CENTER:
		inOutPos->x -= ( calcStringRenderWidth( str, fontID, scale ) / 2.0f );
		break;
	/*case HORIZ_ALIGN_LEFT:
	default:
		// nothing to do
		break;*/
	}
}

void positionCodepointsStartX( const uint32_t* codeptStr, int fontID, HorizTextAlignment align, float width, float scale, Vector2* inOutPos )
{
	switch( align ) {
	case HORIZ_ALIGN_RIGHT:
		inOutPos->x += width - calcCodepointsRenderWidth( codeptStr, fontID, scale );
		break;
	case HORIZ_ALIGN_CENTER:
		inOutPos->x += ( width / 2.0f ) - ( calcCodepointsRenderWidth( codeptStr, fontID, scale ) / 2.0f );
		break;
	/*case HORIZ_ALIGN_LEFT:
		// nothing to do
		break;*/
	}
}

float calcRenderHeight( const uint8_t* str, int fontID )
{
	float height = fonts[fontID].nextLineDescent;

	uint32_t codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( codepoint == 0xA ) {
			height += fonts[fontID].nextLineDescent;
		}
	} while( codepoint != 0 );

	return height;
}

void positionStringStartY( const uint8_t* str, int fontID, VertTextAlignment align, float scale, Vector2* inOutPos )
{
	// nothing to do, position is what we want
	if( align == VERT_ALIGN_BASE_LINE ) {
		return;
	}

	switch( align ) {
	case VERT_ALIGN_BOTTOM:
		inOutPos->y += ( fonts[fontID].descent - calcRenderHeight( str, fontID ) + fonts[fontID].nextLineDescent ) * scale;
		break;
	case VERT_ALIGN_TOP:
		inOutPos->y += fonts[fontID].ascent * scale;
		break;
	case VERT_ALIGN_CENTER:
		inOutPos->y += ( fonts[fontID].ascent - ( calcRenderHeight( str, fontID ) / 2.0f ) ) * scale;
		break;
	}
}

// Draws a string on the screen. The base line is determined by pos.
void txt_DisplayString( const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, int camFlags, int8_t depth, float desiredPixelSize )
{
	assert( utf8Str != NULL );

	if( fontID < 0 ) return;

	float scale = desiredPixelSize / fonts[fontID].baseSize;

	// TODO: Alignment options?
	// TODO: Handle multi-line text better (sometimes letters overlap right now)
	const uint8_t* str = (uint8_t*)utf8Str;
	Vector2 currPos = pos;
	positionStringStartX( str, fontID, hAlign, scale, &currPos );
	positionStringStartY( str, fontID, vAlign, scale, &currPos );
	uint32_t codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( codepoint == 0 ) {
			// end of line do nothing
		} else if( codepoint == 0xA ) {
			// new line
			currPos.x = pos.x;
			currPos.y += fonts[fontID].nextLineDescent;
			positionStringStartX( str, fontID, hAlign, scale, &currPos );
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, codepoint );
			if( glyph != NULL ) {
				int drawID = img_CreateDraw( glyph->imageID, camFlags, currPos, currPos, depth );
				img_SetDrawScale( drawID, scale, scale );
				img_SetDrawColor( drawID, clr, clr );
				//img_Draw_s_c( glyph->imageID, camFlags, currPos, currPos, scale, scale, clr, clr, depth );
				//img_Draw_c( glyph->imageID, camFlags, currPos, currPos, clr, clr, depth );
				currPos.x += glyph->advance * scale;
			}
		}
	} while( codepoint != 0 );
}

// returns whether we can break the line at the specified codepoint
//  gotten from here: https://en.wikipedia.org/wiki/Whitespace_character#Unicode
bool isBreakableCodepoint( int codepoint )
{
	return ( ( codepoint == 9 ) ||
			 ( codepoint == 32 ) ||
			 ( codepoint == 5760 ) ||
			 ( codepoint == 8192 ) ||
			 ( codepoint == 8193 ) ||
			 ( codepoint == 8194 ) ||
			 ( codepoint == 8195 ) ||
			 ( codepoint == 8196 ) ||
			 ( codepoint == 8197 ) ||
			 ( codepoint == 8198 ) ||
			 ( codepoint == 8200 ) ||
			 ( codepoint == 8201 ) ||
			 ( codepoint == 8202 ) ||
			 ( codepoint == 8287 ) ||
			 ( codepoint == 12288 ) ||
			 ( codepoint == 6158 ) ||
			 ( codepoint == 8203 ) ||
			 ( codepoint == 8204 ) ||
			 ( codepoint == 8205 ) );
}

void convertOutToBuffer( const uint8_t* utf8Str )
{
	const uint8_t* str = utf8Str;
	uint32_t codepoint;
	sb_Clear( sbStringCodepointBuffer );
	do {
		codepoint = getUTF8CodePoint( &str );
		sb_Push( sbStringCodepointBuffer, codepoint );
	} while( codepoint != 0 );
}

// Draws a string on the screen to an area. Splits up lines and such. If outCharPos is not equal to NULL it will
//  grab the position of the character at storeCharPos and put it in there. Returns if outCharPos is valid.
bool txt_DisplayTextArea( const uint8_t* utf8Str, Vector2 upperLeft, Vector2 size, Color clr,
	HorizTextAlignment hAlign, VertTextAlignment vAlign, int fontID, size_t storeCharPos, Vector2* outCharPos,
	int camFlags, int8_t depth, float desiredPixelSize )
{
	assert( utf8Str != NULL );

	if( fontID < 0 ) {
		return false;
	}

	float scale = desiredPixelSize / fonts[fontID].baseSize;

	const uint8_t* str = (uint8_t*)utf8Str;

	bool posValid = false;
	size_t charBufferPos = SIZE_MAX;

	// don't bother rendering anything if there are no lines to be able to draw
	if( (int)( size.y / ( fonts[fontID].nextLineDescent * scale ) ) <= 0 ) {
		return false;
	}

	// puts the converted string int sbStringCodepointBuffer
	convertOutToBuffer( (uint8_t*)utf8Str );

	float currentLength = 0.0f;
	float currentHeight = 0.0f;
	uint32_t* stringPos = sbStringCodepointBuffer;
	size_t lastBreakPoint = SIZE_MAX;

	// the height and width are dependent on each other, we'll also find the break
	//  points on lines caused by wrapping and insert line breaks where appropriate
	// the main problem will be if the break is in the middle of a word instead of
	//  just replacing a white space character
	Vector2 maxSize = VEC2_ZERO;
	maxSize.y = fonts[fontID].nextLineDescent * scale;
	float lastBreakPointSize = 0.0f;
	uint32_t maxLineCnt = 0;
	for( size_t i = 0; i < sb_Count( sbStringCodepointBuffer ); ++i ) {
		if( isBreakableCodepoint( sbStringCodepointBuffer[i] ) ) {
			// store the position for later use
			lastBreakPoint = i;

			// want to ignore the character since if we're using it that means
			//  it will be replaced
			lastBreakPointSize = currentLength;
		}

		if( storeCharPos == i ) {
			charBufferPos = sb_Count( sbStringCodepointBuffer );
		}

		if( sbStringCodepointBuffer[i] != LINE_FEED ) {
			Glyph* glyph = getCodepointGlyph( fontID, sbStringCodepointBuffer[i] );
			currentLength += ( glyph->advance * scale );
			
			if( currentLength > size.x ) {
				if( lastBreakPoint != SIZE_MAX ) {
					// have a valid breakpoint, replace it with a new line
					sbStringCodepointBuffer[lastBreakPoint] = LINE_FEED;
					i = lastBreakPoint;
				} else {
					// no valid breakpoint, put the breakpoint in the middle
					//  of the word
					--i;
					sb_Insert( sbStringCodepointBuffer, i, LINE_FEED );
					lastBreakPointSize = currentLength - ( glyph->advance * scale );
				}
			}
		}

		// if we're at a line feed, or reached the max width
		if( sbStringCodepointBuffer[i] == LINE_FEED ) {
			maxSize.x = MAX( maxSize.x, lastBreakPointSize );
			currentLength = 0.0f;
			lastBreakPoint = SIZE_MAX;
			lastBreakPointSize = 0.0f;

			if( ( maxSize.y + ( fonts[fontID].nextLineDescent * scale ) ) > size.y ) {
				// past the bottom of the area we want to draw the text in
				//  truncate the string here
				sbStringCodepointBuffer[i] = 0;
				break;
			}
			++maxLineCnt;
			maxSize.y += fonts[fontID].nextLineDescent * scale;
		}
	}

	// now based on the calculated height we can start rendering and positioning stuff
	//  since we've preprocessed the string and know that all the lines will fit in the
	//  maximum width we can just use the standard width calculation we've already made

	Vector2 renderPos = upperLeft;
	switch( vAlign ) {
	case VERT_ALIGN_BASE_LINE:
	case VERT_ALIGN_TOP:
		//renderPos.y += fonts[fontID].descent + fonts[fontID].nextLineDescent;
		renderPos.y = ( upperLeft.y + fonts[fontID].descent + fonts[fontID].nextLineDescent ) * scale;
		break;
	case VERT_ALIGN_CENTER:
		renderPos.y = upperLeft.y + ( size.y / 2.0f ) - ( maxSize.y / 2.0f ) + ( fonts[fontID].ascent * scale );
		break;
	case VERT_ALIGN_BOTTOM:
		renderPos.y = ( upperLeft.y + size.y ) - maxSize.y + ( ( fonts[fontID].nextLineDescent + fonts[fontID].descent ) * scale );
		//renderPos.y += fonts[fontID].ascent - ( maxSize.y / 2.0f );
		break;
	}
	positionCodepointsStartX( sbStringCodepointBuffer, fontID, hAlign, size.x, scale, &renderPos );
	for( size_t i = 0; ( i < sb_Count( sbStringCodepointBuffer ) ) && ( sbStringCodepointBuffer[i] != 0 ); ++i ) {
		if( sbStringCodepointBuffer[i] == LINE_FEED ) {
			// new line
			renderPos.x = upperLeft.x;
			renderPos.y += fonts[fontID].nextLineDescent * scale;
			positionCodepointsStartX( &( sbStringCodepointBuffer[i+1] ), fontID, hAlign, size.x, scale, &renderPos );
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, sbStringCodepointBuffer[i] );
			if( glyph != NULL ) {
				//img_Draw_c( glyph->imageID, camFlags, renderPos, renderPos, clr, clr, depth );
				//img_Draw_s_c( glyph->imageID, camFlags, renderPos, renderPos, scale, scale, clr, clr, depth );
				int drawID = img_CreateDraw( glyph->imageID, camFlags, renderPos, renderPos, depth );
				img_SetDrawScale( drawID, scale, scale );
				img_SetDrawColor( drawID, clr, clr );
				renderPos.x += ( glyph->advance * scale );
			}
		}

		if( ( i == charBufferPos ) && ( outCharPos != NULL ) ) {
			(*outCharPos) = renderPos;
			posValid = true;
		}
	}

	if( !posValid && ( outCharPos != NULL ) ) {
		(*outCharPos) = renderPos;
		posValid = true;
	}

	return posValid;
}

// copied from stb_truetype.h, want to be able to calculate the size without creating the image so we can do the
//  packing without having to render each letter
static void calcSDFCodepointSize( stbtt_fontinfo* font, int codepoint, float scale, int padding, int* outWidth, int* outHeight )
{
	int ix0,iy0,ix1,iy1;
	int glyph = stbtt_FindGlyphIndex( font, codepoint );

	stbtt_GetGlyphBitmapBoxSubpixel( font, glyph, scale, scale, 0.0f,0.0f, &ix0,&iy0,&ix1,&iy1);

	if (ix0 == ix1 || iy0 == iy1) {
		(*outWidth) = 0;
		(*outHeight) = 0;
		return;
	}

	ix0 -= padding;
	iy0 -= padding;
	ix1 += padding;
	iy1 += padding;

	(*outWidth) = (ix1 - ix0);
	(*outHeight) = (iy1 - iy0);
}

/*
For a SDF font the file format will be as follows:
Font:
  int descent
  int lineGap
  int ascent
  int baseSize
  size_t numGlyphs
  stretchy_buffer Glyphs:
    int codepoint
	int advance
	vec2 imgMin   // these will only be integer values, so save them out as that
	vec2 imgMax   // these will only be integer values, so save them out as that
	vec2 offset
  size_t pngImageSize
  byte pngImage

we'll using the extension sdfont

When loading a font we'll want to check to see if the required codepoints and the stored codepoints match, if they don't
 we'll consider the loading a failure

we can recalculate the scale like this: scale = baseSize / ( ascent - descent )
 that we we only have to save everything as integers
*/
static const char* sdfFontExtension = ".sdfont";

static uint8_t* sbSaveImageData = NULL;

// used to write out the data created by stbi_write_png_to_func
void stbiWriteFunc(void *context, void *data, int size)
{
	uint8_t* chunkStart = sb_Add( sbSaveImageData, size );
	memcpy( chunkStart, data, size );
}

// saves out the font to fileName + sdfFontExtension
static bool saveSDFFont( const char* fileName,
	int32_t descent, int32_t linegap, int32_t ascent, int32_t baseSize,
	Glyph* sbGlyphs, Vector2* mins, Vector2* maxes, Vector2* offsets,
	LoadedImage* image )
{
	llog( LOG_INFO, "Writing out sdf font for %s", fileName );
	bool succeeded = false;

	sb_Clear( sbSaveImageData );

#define CHECK_WRITE( w, s, c ) \
	if( ( w ) != ( c )  ) { \
		llog( LOG_ERROR, "Error %s for file %s: %s", ( s ), fileName, SDL_GetError( ) ); \
		goto clean_up; \
	}

	char* fontFileName = mem_Allocate( sizeof( char ) * ( strlen( fileName ) + strlen( sdfFontExtension ) + 1 ) );
	strcpy( fontFileName, fileName );
	strcat( fontFileName, sdfFontExtension );

	SDL_RWops* rwopsFile = SDL_RWFromFile( fontFileName, "w" );
	if( rwopsFile == NULL ) {
		llog( LOG_WARN, "Unable to open sdf font for %s: %s", fileName, SDL_GetError( ) );
		goto clean_up;
	}

	// write out the font data
	CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &descent ) ), "writing out descent", 1 );
	CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &linegap ) ), "writing out line gap", 1 );
	CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &ascent ) ), "writing out ascent", 1 );
	CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &baseSize ) ), "writing out base size", 1 );

	// write out the glyphs
	CHECK_WRITE( SDL_WriteBE64( rwopsFile, (Uint64)sb_Count( sbGlyphs ) ), "writing out glyph count", 1 );
	for( size_t i = 0; i < sb_Count( sbGlyphs ); ++i ) {
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &( sbGlyphs[i].codepoint ) ) ), "writing out glyph codepoint", 1 );

		int32_t advance = (int32_t)sbGlyphs[i].advance;
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &advance ) ), "writing out glyph advance", 1 );

		// the mins and maxes are stored as floats but should still be integer values
		uint32_t minX = (uint32_t)mins[i].x;
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &minX ) ), "writing out glyph min x", 1 );

		uint32_t minY = (uint32_t)mins[i].y;
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &minY ) ), "writing out glyph min y", 1 );

		uint32_t maxX = (uint32_t)maxes[i].x;
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &maxX ) ), "writing out glyph max x", 1 );

		uint32_t maxY = (uint32_t)maxes[i].y;
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &maxY ) ), "writing out glyph max y", 1 );

		// offsets should be either whole numbers or half, so we'll store it as doubled so we only need to store integer values
		int32_t offsetX = (int32_t)( offsets[i].x * 2.0f );
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &offsetX ) ), "writing out glyph offset x", 1 );

		int32_t offsetY = (int32_t)( offsets[i].y * 2.0f );
		CHECK_WRITE( SDL_WriteBE32( rwopsFile, *(Uint32*)( &offsetY ) ), "writing out glyph offset y", 1 );
	}

	// write out the image
	//  encode to png first
	// TODO: Find a way to write out an even more compressed version of the png
	if( stbi_write_png_to_func( stbiWriteFunc, NULL, image->width, image->height, image->comp, image->data, 0 ) == 0 ) {
		llog( LOG_ERROR, "Error creating png for file: %s", fileName );
		goto clean_up;
	}
	CHECK_WRITE( SDL_WriteBE64( rwopsFile, (Uint64)sb_Count( sbSaveImageData ) ), "writing out image size", 1 );
	CHECK_WRITE( SDL_RWwrite( rwopsFile, sbSaveImageData, sizeof( uint8_t ), sb_Count( sbSaveImageData ) ), "writing out image", sb_Count( sbSaveImageData ) );
	
	succeeded = true;

clean_up:
	sb_Release( sbSaveImageData );
	sbSaveImageData = NULL;

	mem_Release( fontFileName );

	if( rwopsFile != NULL ) {
		SDL_RWclose( rwopsFile );
	}

	llog( LOG_INFO, "Done writing out sdf font for %s, %s", fileName, succeeded ? "succeeded" : "failed" );

	return succeeded;

#undef CHECK_WRITE
}

// Attempts to load an sdf font from fileName + sdfFontExtension.
//  Returns -1 if it fails, returns the font id to use otherwise.
static int loadSDFFont( const char* fileName )
{
	int fontID = -1;
	uint8_t* pngBuffer = NULL;
	int* retIDs = NULL;
	Glyph* sbGlyphs = NULL;
	Vector2* mins = NULL;
	Vector2* maxes = NULL;
	Vector2* offsets = NULL;
	Vector2* sbOffsets = NULL;

	// append the extension and see if the file exists
	char* fontFileName = mem_Allocate( sizeof( char ) * ( strlen( fileName ) + strlen( sdfFontExtension ) + 1 ) );
	strcpy( fontFileName, fileName );
	strcat( fontFileName, sdfFontExtension );

	SDL_RWops* rwopsFile = SDL_RWFromFile( fontFileName, "r" );
	if( rwopsFile == NULL ) {
		llog( LOG_WARN, "Unable to open sdf font for %s", fileName );
		return -1;
	}

	// read in all the data for the font
	int32_t descent;
	int32_t lineGap;
	int32_t ascent;
	int32_t baseSize;
	Uint32 readInt;

	readInt = SDL_ReadBE32( rwopsFile );
	descent = *(int32_t*)( &readInt );

	readInt = SDL_ReadBE32( rwopsFile );
	lineGap = *(int32_t*)( &readInt );

	readInt = SDL_ReadBE32( rwopsFile );
	ascent = *(int32_t*)( &readInt );

	readInt = SDL_ReadBE32( rwopsFile );
	baseSize = *(int32_t*)( &readInt );

	float scale = (float)baseSize / ( (float)ascent - (float)descent );

	size_t numGlyphs = (size_t)SDL_ReadBE64( rwopsFile );
	sb_Add( sbGlyphs, numGlyphs );
	mins = mem_Allocate( sizeof( Vector2 ) * numGlyphs );
	maxes = mem_Allocate( sizeof( Vector2 ) * numGlyphs );
	offsets = mem_Allocate( sizeof( Vector2 ) * numGlyphs );

	if( sbGlyphs == NULL ) {
		llog( LOG_ERROR, "Error allocating glyphs for font %s", fileName );
		goto clean_up;
	}

	if( mins == NULL ) {
		llog( LOG_ERROR, "Error allocating mins for font %s", fileName );
		goto clean_up;
	}

	if( maxes == NULL ) {
		llog( LOG_ERROR, "Error allocating maxes for font %s", fileName );
		goto clean_up;
	}

	if( offsets == NULL ) {
		llog( LOG_ERROR, "Error allocating offsets for font %s", fileName );
		goto clean_up;
	}

	for( size_t i = 0; i < numGlyphs; ++i ) {
		readInt = SDL_ReadBE32( rwopsFile );
		sbGlyphs[i].codepoint = *(int32_t*)(&readInt);

		readInt = SDL_ReadBE32( rwopsFile );
		sbGlyphs[i].advance = (float)( *(int32_t*)(&readInt) ) * scale;

		readInt = SDL_ReadBE32( rwopsFile );
		mins[i].x = (float)( *(int32_t*)(&readInt) );

		readInt = SDL_ReadBE32( rwopsFile );
		mins[i].y = (float)( *(int32_t*)(&readInt) );

		readInt = SDL_ReadBE32( rwopsFile );
		maxes[i].x = (float)( *(int32_t*)(&readInt) );

		readInt = SDL_ReadBE32( rwopsFile );
		maxes[i].y = (float)( *(int32_t*)(&readInt) );

		readInt = SDL_ReadBE32( rwopsFile );
		offsets[i].x = (float)( *(int32_t*)(&readInt) ) / 2.0f;

		readInt = SDL_ReadBE32( rwopsFile );
		offsets[i].y = (float)( *(int32_t*)(&readInt) ) / 2.0f;
	}

	size_t pngDataSize = (size_t)SDL_ReadBE64( rwopsFile );
	pngBuffer = mem_Allocate( pngDataSize );
	size_t readTotal = 0;
	size_t amtRead = 1;
	uint8_t* bufferPos = pngBuffer;
	while( ( readTotal < pngDataSize ) && ( amtRead > 0 ) ) {
		amtRead = SDL_RWread( rwopsFile, bufferPos, sizeof( uint8_t ), ( pngDataSize - readTotal ) );
		readTotal += amtRead;
		bufferPos += amtRead;
	}
	SDL_RWclose( rwopsFile );
	rwopsFile = NULL;

	if( readTotal != pngDataSize ) {
		llog( LOG_ERROR, "Unable to read png data for font %s", fileName );
		goto clean_up;
	}

	LoadedImage pngImg;
	gfxUtil_LoadImageFromMemory( pngBuffer, pngDataSize, 1, &pngImg );
	mem_Release( pngBuffer );
	pngBuffer = NULL;

	// now generate the actual font
	//  find an unused font ID
	fontID = findUnusedFontID( );
	if( fontID < 0 ) {
		llog( LOG_ERROR, "Unable to find empty font to use for %s", fileName );
		goto clean_up;
	}

	fonts[fontID].ascent = ascent * scale;
	fonts[fontID].descent = descent * scale;
	fonts[fontID].lineGap = lineGap * scale;
	fonts[fontID].nextLineDescent = fonts[fontID].ascent - fonts[fontID].descent + fonts[fontID].lineGap;
	fonts[fontID].baseSize = baseSize;
	retIDs = mem_Allocate( sizeof( int ) * numGlyphs );
	fonts[fontID].glyphsBuffer = sbGlyphs;

	fonts[fontID].packageID = img_SplitAlphaBitmap( pngImg.data, pngImg.width, pngImg.height, fontPackRange.num_chars, ST_SIMPLE_SDF, mins, maxes, retIDs );
	if( fonts[fontID].packageID < 0 ) {
		llog( LOG_ERROR, "Unable to split image for %s", fileName );
		goto clean_up;
	}

	for( size_t i = 0; i < numGlyphs; ++i ) {
		sbGlyphs[i].imageID = retIDs[i];

		img_SetOffset( retIDs[i], offsets[i] );

		if( sbGlyphs[i].codepoint == missingChar ) {
			fonts[fontID].missingCharGlyphIdx = i;
		}
	}

clean_up:

	mem_Release( retIDs );
	mem_Release( pngBuffer );
	mem_Release( fontFileName );

	if( rwopsFile != NULL ) {
		SDL_RWclose( rwopsFile );
	}

	mem_Release( mins );
	mem_Release( maxes );
	mem_Release( offsets );
	if( fontID == -1 ) {
		sb_Release( sbGlyphs );
	}

	return fontID;
}

// Creates a font that's rendered out as a signed distance field. Will also attempt to save a version of this font that
//  can be loaded later much quicker.
int txt_CreateSDFFont( const char* fileName )
{
	/*
	int font = loadSDFFont( fileName );
	if( font == -1 ) {
		font = generateSDFFont( fileName );
		// have save inside generate, as we may have to do some processing after it's saved
		if( !saveSDFFont( filename, font ) ) {
			llog( LOG_WARN, "Unable to save generated sdf font: %s", fileName );
		}
	}
	return font;
	*/

	// first try to load an existing sdf font, if it doesn't exist then create it
	int newFont = loadSDFFont( fileName );
	if( newFont != -1 ) {
		return newFont;
	}

	int pixelHeight = 64;
	int padding = 4; //6; //4; // 6 instead of 4 to solve flickering issues with some fonts
	unsigned char onEdgeValue = 128;
	float pixelDistScale = (float)onEdgeValue/(float)padding;

	// first we'd like to generate the borders so we can pack everything tightly without having
	//  to store every image
	unsigned char* baseImage = NULL;
	stbrp_rect* rects = NULL;
	stbrp_node* nodes = NULL;
	uint8_t* buffer = NULL;
	stbtt_fontinfo font;
	Glyph* sbGlyphStorage = NULL;
	Vector2* mins = NULL;
	Vector2* maxes = NULL;
	Vector2* offsets = NULL;
	int* retIDs = NULL;

#define OUT_ERROR( s ) \
	{ \
		newFont = -1; \
		llog( LOG_ERROR, "%s for font %s", (s), fileName ); \
		goto clean_up; \
	}

#define CHECK_POINTER( p, s ) \
	{ if( (p) == NULL ) OUT_ERROR( s ); }

	// find an unused font ID
	newFont = findUnusedFontID( );
	if( newFont < 0 ) {
		OUT_ERROR( "Unable to find empty font to use" );
	}

	//  some temporary memory for loading the file
	size_t bufferSize = 1024 * 1024;
	buffer = mem_Allocate( bufferSize * sizeof( uint8_t ) ); // megabyte sized buffer, should never load a file larger than this
	CHECK_POINTER( buffer, "Error allocating font data buffer" );

	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	CHECK_POINTER( rwopsFile, "Error opening file" );

	size_t numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( uint8_t ), bufferSize );
	if( numRead >= bufferSize ) {
		OUT_ERROR( "Too much data read in from file" );
	}
	
	// get some of the basic font stuff, need to do this so we can handle multiple lines of text
	int ascent, descent, lineGap;
	float scale;
	stbtt_InitFont( &font, buffer, 0 );
	stbtt_GetFontVMetrics( &font, &ascent, &descent, &lineGap );
	scale = stbtt_ScaleForPixelHeight( &font, (float)pixelHeight );
	fonts[newFont].ascent = (float)ascent * scale;
	fonts[newFont].descent = (float)descent * scale;
	fonts[newFont].lineGap = (float)lineGap * scale;
	fonts[newFont].nextLineDescent = fonts[newFont].ascent - fonts[newFont].descent + fonts[newFont].lineGap;
	fonts[newFont].baseSize = pixelHeight;

	float test = (float)pixelHeight / ( ascent - descent );

	const int WIDTH = 1024;
	const int HEIGHT = 1024;
	// generate the boxes for all the glyphs we want to pack and then pack them
	rects = mem_Allocate( sizeof( stbrp_rect ) * fontPackRange.num_chars );
	CHECK_POINTER( rects, "Error packing rects" );

	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		int width, height;
		calcSDFCodepointSize( &font, fontPackRange.array_of_unicode_codepoints[i], scale, padding, &width, &height );
		rects[i].w = (stbrp_coord)width;
		rects[i].h = (stbrp_coord)height;
		rects[i].id = fontPackRange.array_of_unicode_codepoints[i];
	}
	// TODO: find a way to pack to approximate a minimum size image
	nodes = mem_Allocate( sizeof( stbrp_node ) * WIDTH );
	CHECK_POINTER( nodes, "Error allocating packing nodes" );

	stbrp_context packContext;
	stbrp_init_target( &packContext, WIDTH, HEIGHT, nodes, WIDTH );
	stbrp_pack_rects( &packContext, rects, fontPackRange.num_chars );
	mem_Release( nodes );
	nodes = NULL;

	// generate the images for each glyphs and place it in the appropriate spot in the master image
	baseImage = mem_Allocate( sizeof( unsigned char ) * WIDTH * HEIGHT );
	CHECK_POINTER( baseImage, "Error allocating full image" );

	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		if( !rects[i].was_packed ) {
			llog( LOG_WARN, "Unable to pack codepoint %i for font %s.", rects[i].id, fileName );
			continue;
		}

		int sdfWidth, sdfHeight, sdfXOff, sdfYOff;
		unsigned char* charSDF = stbtt_GetCodepointSDF( &font, scale, rects[i].id, padding, onEdgeValue, pixelDistScale, &sdfWidth, &sdfHeight, &sdfXOff, &sdfYOff );

		//llog( LOG_INFO, "Packing codepoint %i - x: %i  y: %i  w: %i  h: %i  xo: %i  yo: %i", rects[i].id, rects[i].x, rects[i].y, sdfWidth, sdfHeight, sdfXOff, sdfYOff );
		//if( rects[i].w != sdfWidth ) llog( LOG_WARN, "  - widths don't match!" );
		//if( rects[i].h != sdfHeight ) llog( LOG_WARN, "  - heights don't match!" );

		// pack into image
		//  would really like a version of memcpy with stride
		int x = rects[i].x;
		for( int yo = 0; yo < rects[i].h; ++yo ) {
			unsigned char* rowDest = baseImage + ( ( rects[i].x ) + ( ( rects[i].y + yo ) * WIDTH ) );//( ( y * WIDTH ) + x );
			unsigned char* rowSrc = charSDF + ( yo * rects[i].w );
			memcpy( rowDest, rowSrc, sizeof( unsigned char ) * rects[i].w );
		}
		STBTT_free( charSDF, 0 );
	}

	// split the bitmap
	mins = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	CHECK_POINTER( mins, "Unable to allocate mins list" );

	maxes = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	CHECK_POINTER( maxes, "Unable to allocate maxes list" );

	offsets = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	CHECK_POINTER( offsets, "Unable to allocate offsets list" );

	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		mins[i].x = rects[i].x;
		mins[i].y = rects[i].y;
		maxes[i].x = (float)( rects[i].x + rects[i].w );
		maxes[i].y = (float)( rects[i].y + rects[i].h );
	}
	retIDs = mem_Allocate( sizeof( int ) * fontPackRange.num_chars );

	fonts[newFont].packageID = img_SplitAlphaBitmap( baseImage, WIDTH, HEIGHT, fontPackRange.num_chars, ST_SIMPLE_SDF, mins, maxes, retIDs );
	if( fonts[newFont].packageID < 0 ) {
		OUT_ERROR( "Unable to split images" );
	}

	// create the glyph storage then go through each glyph image and adjust the offset
	sb_Add( sbGlyphStorage, fontPackRange.num_chars );
	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		if( !rects[i].was_packed ) {
			// wasn't packed so we won't create a glyph for it
			llog( LOG_WARN, "Glyph not packed. Not creating glyph for codepoint %i for font %s.", rects[i].id, fileName );
			continue;
		}

		int advance, lsb;
		stbtt_GetCodepointHMetrics( &font, rects[i].id, &advance, &lsb);

		// because stbrp_pack_rects( ) conserves order we can treat these as parallel arrays
		sbGlyphStorage[i].codepoint = rects[i].id;
		sbGlyphStorage[i].advance = (float)advance; // * scale;//fontPackRange.chardata_for_range[i].xadvance; // do the scaling afterwards
		sbGlyphStorage[i].imageID = retIDs[i];

		int ix0, iy0, ix1, iy1;
		stbtt_GetCodepointBitmapBox( &font, rects[i].id, scale, scale, &ix0, &iy0, &ix1, &iy1 );

		Vector2 offset;
		offset.x = ( rects[i].w / 2.0f ) - padding;
		offset.y = ( -rects[i].h / 2.0f ) + padding + iy1;
		img_SetOffset( retIDs[i], offset );
		offsets[i] = offset; // used for saving

		if( sbGlyphStorage[i].codepoint == missingChar ) {
			fonts[newFont].missingCharGlyphIdx = i;
		}
	}
	fonts[newFont].glyphsBuffer = sbGlyphStorage;

	// save out the font so next time we can load it faster
	LoadedImage fontImg;
	fontImg.width = WIDTH;
	fontImg.height = HEIGHT;
	fontImg.comp = 1;
	fontImg.data = baseImage;
	saveSDFFont( fileName, descent, lineGap, ascent, pixelHeight, sbGlyphStorage, mins, maxes, offsets, &fontImg );

	// now go through and scale everything
	for( size_t i = 0; i < sb_Count( sbGlyphStorage ); ++i ) {
		sbGlyphStorage[i].advance *= scale;
	}

clean_up:
	mem_Release( offsets );
	mem_Release( mins );
	mem_Release( maxes );
	mem_Release( retIDs );
	mem_Release( baseImage );
	mem_Release( buffer );
	mem_Release( fontPackRange.chardata_for_range );
	mem_Release( rects );
	mem_Release( nodes );
	fontPackRange.chardata_for_range = NULL;
	fontPackRange.font_size = 0.0f;

	if( newFont < 0 ) {
		// creating font failed, release pre-allocated storage
		sb_Release( sbGlyphStorage );
	}

	return newFont;

#undef CHECK_POINTER
#undef OUT_ERROR
}

int txt_GetBaseSize( int fontID )
{
	assert( fontID >= 0 );
	return fonts[fontID].baseSize;
}

int txt_GetCharacterImage( int fontID, int c )
{
	assert( fontID >= 0 );
	Glyph* glyph = getCodepointGlyph( fontID, c );
	return glyph->imageID;
}
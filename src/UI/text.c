#include "text.h"

// TODO: Ability to cache string images

#include <SDL_rwops.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../System/memory.h"

#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)	((void)(u),mem_Allocate(x))
#define STBTT_free(x,u)		((void)(u),mem_Release(x))
#include <stb_truetype.h>

#include "../Utils/stretchyBuffer.h"
#include "../Graphics/images.h"
#include "../Math/mathUtil.h"

#include "../System/platformLog.h"

#include "../System/jobQueue.h"

typedef struct {
	int codepoint;
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
} Font;

static int missingChar = 0x3F; // '?'

static Font fonts[MAX_FONTS] = { 0 };
// TODO: for localization we can define stbtt_pack_range for each language and link them together to be loaded
stbtt_pack_range fontPackRange = { 0 };

/*
Sets up the default codepoints to load and clears out any currently loaded fonts.
*/
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

/*
Adds a codepoint to the set of codepoints to get glyphs for when loading a font. This does
 not currently cause any currently loaded fonts to be reloaded, so they will not be able to
 display this codepoint if it had not previously been added.
*/
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

/*
Loads the font at fileName, with a height of pixelHeight.
 Returns an ID to be used when displaying a string, returns -1 if there was an issue.
*/
int txt_LoadFont( const char* fileName, float pixelHeight )
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
	while( fonts[newFont].glyphsBuffer != NULL ) {
		++newFont;
	}
	if( newFont >= MAX_FONTS ) {
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
	fontPackRange.font_size = pixelHeight;
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
	scale = stbtt_ScaleForPixelHeight( &font, pixelHeight );
	fonts[newFont].ascent = (float)ascent * scale;
	fonts[newFont].descent = (float)descent * scale;
	fonts[newFont].lineGap = (float)lineGap * scale;
	fonts[newFont].nextLineDescent = fonts[newFont].ascent - fonts[newFont].descent + fonts[newFont].lineGap;
	
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
//#error this looks to be where the issue is happening, something is wrong with the data that is loaded or how it's parsed
	// so, how do we debug this?
	// the data we're getting from PackFontRanges seems to be invalid, whatever we're getting in is invalid
	// stbtt_PackFontRangesRenderIntoRects
	// the code points are completely wrong, but only here
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

/*
Loads the font at file name on a seperate thread. Uses a height of pixelHeight.
This will initialize a bunch of stuff but not do any of the loading.
 Puts the resulting font ID into outFontID.
*/
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
	for( int i = 0; i < glyphCount; ++i ) {
		if( font->glyphsBuffer[i].codepoint == codepoint ) {
			return &( font->glyphsBuffer[i] );
		}
	}
	return &( font->glyphsBuffer[ font->missingCharGlyphIdx ] );
}

/*
Gets the code point from a string, will advance the string past the current codepoint to the next.
*/
uint32_t getUTF8CodePoint( const uint8_t** strData )
{
	// TODO: Get this working with actual UTF-8 and not just the ASCII subset
	//  https://tools.ietf.org/html/rfc3629
	uint32_t c = (**strData);
	++(*strData);
	return c;
}

/*
Gets the total width of the string once it's been rendered
*/
float calcStringRenderWidth( const uint8_t* str, int fontID )
{
	float width = 0.0f;

	uint32_t codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( ( codepoint == 0 ) || ( codepoint == LINE_FEED ) ) {
			// end of string/line do nothing
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, codepoint );
			width += glyph->advance;
		}
	} while( ( codepoint != 0 ) && ( codepoint != LINE_FEED ) );

	return width;
}

float calcCodepointsRenderWidth( const uint32_t* str, int fontID )
{
	float width = 0.0f;
	for( int i = 0; ( str[i] != 0 ) && ( str[i] != LINE_FEED ); ++i ) {
		width += getCodepointGlyph( fontID, str[i] )->advance;
	}
	return width;
}

void positionStringStartX( const uint8_t* str, int fontID, HorizTextAlignment align, Vector2* inOutPos )
{
	switch( align ) {
	case HORIZ_ALIGN_RIGHT:
		inOutPos->x -= calcStringRenderWidth( str, fontID );
		break;
	case HORIZ_ALIGN_CENTER:
		inOutPos->x -= ( calcStringRenderWidth( str, fontID ) / 2.0f );
		break;
	/*case TEXT_ALIGN_LEFT:
	default:
		// nothing to do
		break;*/
	}
}

void positionCodepointsStartX( const uint32_t* codeptStr, int fontID, HorizTextAlignment align, float width, Vector2* inOutPos )
{
	switch( align ) {
	case HORIZ_ALIGN_RIGHT:
		inOutPos->x += width - calcCodepointsRenderWidth( codeptStr, fontID );
		break;
	case HORIZ_ALIGN_CENTER:
		inOutPos->x += ( width / 2.0f ) - ( calcCodepointsRenderWidth( codeptStr, fontID ) / 2.0f );
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

void positionStringStartY( const uint8_t* str, int fontID, VertTextAlignment align, Vector2* inOutPos )
{
	// nothing to do, position is what we want
	if( align == VERT_ALIGN_BASE_LINE ) {
		return;
	}

	switch( align ) {
	case VERT_ALIGN_BOTTOM:
		inOutPos->y += fonts[fontID].descent - calcRenderHeight( str, fontID ) + fonts[fontID].nextLineDescent;
		break;
	case VERT_ALIGN_TOP:
		inOutPos->y += fonts[fontID].ascent;
		break;
	case VERT_ALIGN_CENTER:
		inOutPos->y += fonts[fontID].ascent - ( calcRenderHeight( str, fontID ) / 2.0f );
		break;
	}
}

/*
Draws a string on the screen. The base line is determined by pos.
*/
void txt_DisplayString( const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, int camFlags, int8_t depth )
{
	assert( utf8Str != NULL );

	if( fontID < 0 ) return;

	// TODO: Alignment options?
	// TODO: Handle multi-line text better (sometimes letters overlap right now)
	const uint8_t* str = (uint8_t*)utf8Str;
	Vector2 currPos = pos;
	positionStringStartX( str, fontID, hAlign, &currPos );
	positionStringStartY( str, fontID, vAlign, &currPos );
	uint32_t codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( codepoint == 0 ) {
			// end of line do nothing
		} else if( codepoint == 0xA ) {
			// new line
			currPos.x = pos.x;
			currPos.y += fonts[fontID].nextLineDescent;
			positionStringStartX( str, fontID, hAlign, &currPos );
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, codepoint );
			if( glyph != NULL ) {
				img_Draw_c( glyph->imageID, camFlags, currPos, currPos, clr, clr, depth );
				currPos.x += glyph->advance;
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

#include <string.h>
/*
Draws a string on the screen to an area. Splits up lines and such. If outCharPos is not equal to NULL it will
 grab the position of the character at storeCharPos and put it in there. Returns if outCharPos is valid.
*/
bool txt_DisplayTextArea( const uint8_t* utf8Str, Vector2 upperLeft, Vector2 size, Color clr,
	HorizTextAlignment hAlign, VertTextAlignment vAlign, int fontID, size_t storeCharPos, Vector2* outCharPos,
	int camFlags, int8_t depth )
{
	assert( utf8Str != NULL );

	if( fontID < 0 ) {
		return false;
	}

	const uint8_t* str = (uint8_t*)utf8Str;

	bool posValid = false;
	size_t charBufferPos = SIZE_MAX;

	// don't bother rendering anything if there are no lines to be able to draw
	if( (int)( size.y / fonts[fontID].nextLineDescent ) <= 0 ) {
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
	maxSize.y = fonts[fontID].nextLineDescent;
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
			currentLength += glyph->advance;
			
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
					lastBreakPointSize = currentLength - glyph->advance;
				}
			}
		}

		// if we're at a line feed, or reached the max width
		if( sbStringCodepointBuffer[i] == LINE_FEED ) {
			maxSize.x = MAX( maxSize.x, lastBreakPointSize );
			currentLength = 0.0f;
			lastBreakPoint = SIZE_MAX;
			lastBreakPointSize = 0.0f;

			if( ( maxSize.y + fonts[fontID].nextLineDescent ) > size.y ) {
				// past the bottom of the area we want to draw the text in
				//  truncate the string here
				sbStringCodepointBuffer[i] = 0;
				break;
			}
			++maxLineCnt;
			maxSize.y += fonts[fontID].nextLineDescent;
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
		renderPos.y = upperLeft.y + fonts[fontID].descent + fonts[fontID].nextLineDescent;
		break;
	case VERT_ALIGN_CENTER:
		renderPos.y = upperLeft.y + ( size.y / 2.0f ) - ( maxSize.y / 2.0f ) + fonts[fontID].ascent;
		break;
	case VERT_ALIGN_BOTTOM:
		renderPos.y = ( upperLeft.y + size.y ) - maxSize.y + fonts[fontID].nextLineDescent + fonts[fontID].descent;
		//renderPos.y += fonts[fontID].ascent - ( maxSize.y / 2.0f );
		break;
	}
	positionCodepointsStartX( sbStringCodepointBuffer, fontID, hAlign, size.x, &renderPos );
	for( size_t i = 0; ( i < sb_Count( sbStringCodepointBuffer ) ) && ( sbStringCodepointBuffer[i] != 0 ); ++i ) {
		if( sbStringCodepointBuffer[i] == LINE_FEED ) {
			// new line
			renderPos.x = upperLeft.x;
			renderPos.y += fonts[fontID].nextLineDescent;
			positionCodepointsStartX( &( sbStringCodepointBuffer[i+1] ), fontID, hAlign, size.x, &renderPos );
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, sbStringCodepointBuffer[i] );
			if( glyph != NULL ) {
				img_Draw_c( glyph->imageID, camFlags, renderPos, renderPos, clr, clr, depth );
				renderPos.x += glyph->advance;
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
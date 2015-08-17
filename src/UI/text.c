#include "text.h"

// TODO: Ability to cache string images

#include <SDL_log.h>
#include <SDL_rwops.h>
#include <assert.h>

#include "../System/memory.h"

#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)	((void)(u),mem_Allocate(x))
#define STBTT_free(x,u)		((void)(u),mem_Release(x))
#include <stb_truetype.h>

#include "../Utils/stretchyBuffer.h"
#include "../Graphics/images.h"

typedef struct {
	int codepoint;
	int imageID;
	float advance;
} Glyph;

#define MAX_FONTS 32
typedef struct {
	// this will be sorted by the codepoint entry in all the structs, make it easier to search
	//  could also preprocess strings to just be a list of indices into the buffer
	// will be a stretchy buffer
	Glyph* glyphsBuffer;
	int packageID;

	float descent;
	float lineGap;
	float ascent;

	float nextLineDescent;
} Font;

static int missingChar = 0x3F; // '?'

static Font fonts[MAX_FONTS] = { 0 };
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
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to find empty font to use for %s", fileName );
		goto clean_up;
	}

	sb_Add( glyphStorage, fontPackRange.num_chars );
	if( glyphStorage == NULL ) {
		newFont = -1;
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to allocate glyphs for %s", fileName );
		goto clean_up;
	}

	// create storage for all the characters
	fontPackRange.chardata_for_range = mem_Allocate( sizeof( stbtt_packedchar ) * fontPackRange.num_chars );
	fontPackRange.font_size = pixelHeight;
	if( fontPackRange.chardata_for_range == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error allocating range data for %s", fileName );
		newFont = -1;
		goto clean_up;
	}
	
	// now load the file
	//  some temporary memory for loading the file
	size_t bufferSize = 1024 * 1024;
	buffer = mem_Allocate( bufferSize * sizeof( uint8_t ) ); // megabyte sized buffer, should never load a file larger than this
	if( buffer == NULL ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_APPLICATION, "Error allocating font data buffer for %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Error opening font file %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	size_t numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( uint8_t ), bufferSize );
	if( numRead >= bufferSize ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Too much data read in from file %s", fileName );
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
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to allocate bitmap memory for %s", fileName );
		goto clean_up;
	}
	// TODO: Test oversampling
	if( !stbtt_PackBegin( &packContext, bmpBuffer, bmpWidth, bmpHeight, 0, 1, NULL ) ) {
		newFont = -1;
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to begin packing ranges for %s", fileName );
		goto clean_up;
	}
	int wasPacked = stbtt_PackFontRanges( &packContext, (unsigned char*)buffer, 0, &fontPackRange, 1 );
	stbtt_PackEnd( &packContext );
	if( !wasPacked ) {
		newFont = -1;
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to pack ranges for %s", fileName );
		goto clean_up;
	}

	// create all the glyphs for displaying
	mins = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	maxes = mem_Allocate( sizeof( Vector2 ) * fontPackRange.num_chars );
	retIDs = mem_Allocate( sizeof( int ) * fontPackRange.num_chars );
	if( ( mins == NULL ) || ( maxes == NULL ) || ( retIDs == NULL ) ) {
		newFont = -1;
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to allocate image data for %s", fileName );
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
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Unable to split images for font %s", fileName );
		newFont = -1;
		goto clean_up;
	}

	fonts[newFont].glyphsBuffer = glyphStorage;

	for( int i = 0; i < fontPackRange.num_chars; ++i ) {
		fonts[newFont].glyphsBuffer[i].codepoint = fontPackRange.array_of_unicode_codepoints[i];
		fonts[newFont].glyphsBuffer[i].imageID = retIDs[i];
		fonts[newFont].glyphsBuffer[i].advance = fontPackRange.chardata_for_range[i].xadvance;

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

void txt_UnloadFont( int fontID )
{
	assert( fontID >= 0 );

	sb_Release( fonts[fontID].glyphsBuffer );
	fonts[fontID].glyphsBuffer = NULL;
	img_CleanPackage( fonts[fontID].packageID );
}

Glyph* getCodepointGlyph( int fontID, int codepoint )
{
	Glyph* out = NULL;
	int glyphCount = sb_Count( fonts[fontID].glyphsBuffer );
	for( int i = 0; i < glyphCount; ++i ) {
		if( fonts[fontID].glyphsBuffer[i].codepoint == codepoint ) {
			return &( fonts[fontID].glyphsBuffer[i] );
		}
	}

	return NULL;
}

/*
Gets the code point from a string, will advance the string past the current codepoint to the next.
*/
int getUTF8CodePoint( const uint8_t** strData )
{
	// TODO: Get this working with actual UTF-8 and not just the ASCII subset
	//  https://tools.ietf.org/html/rfc3629
	int c = (**strData);
	++(*strData);
	return c;
}

/*
Gets the total width of the string once it's been rendered
*/
float calcStringRenderWidth( const uint8_t* str, int fontID )
{
	float width = 0.0f;

	int codepoint = 0;
	do {
		codepoint = getUTF8CodePoint( &str );
		if( ( codepoint == 0 ) || ( codepoint == 0xA )) {
			// end of string/line do nothing
		} else {
			Glyph* glyph = getCodepointGlyph( fontID, codepoint );
			width += glyph->advance;
		}
	} while( ( codepoint != 0 ) && ( codepoint != 0xA ) );

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

float calcRenderHeight( const uint8_t* str, int fontID )
{
	float height = fonts[fontID].nextLineDescent;

	int codepoint = 0;
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
	int fontID, int camFlags, char depth )
{
	assert( utf8Str != NULL );

	// TODO: Alignment options?
	// TODO: Handle missing codepoint correctly
	// TODO: Handle multi-line text better (sometimes letters overlap right now)
	const uint8_t* str = (uint8_t*)utf8Str;
	Vector2 currPos = pos;
	positionStringStartX( str, fontID, hAlign, &currPos );
	positionStringStartY( str, fontID, vAlign, &currPos );
	int codepoint = 0;
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
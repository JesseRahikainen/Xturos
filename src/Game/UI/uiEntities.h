#ifndef UI_ENTITIES_H
#define UI_ENTITIES_H

#include "Math/vector2.h"
#include "Graphics/color.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "UI/text.h"

// Labels
EntityID createLabel( ECPS* ecps, const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, float fontSize, int camFlags, int8_t depth );

// Buttons
EntityID button_CreateEmpty( ECPS* ecps, Vector2 position, Vector2 size, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse );

EntityID button_CreateImageButton( ECPS* ecps, Vector2 position, Vector2 normalSize, Vector2 clickedSize,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int imgID, Color imgColor, unsigned int camFlags, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse );

EntityID button_Create3x3Button( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int* slicedBorder, Color imgColor, unsigned int camFlags, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse );

EntityID button_Create3x3ScriptButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int* slicedBorder, Color imgColor, unsigned int camFlags, int8_t depth, const char* pressResponse, const char* releaseResponse );

EntityID button_CreateTextButton( ECPS* ecps, Vector2 position, Vector2 size,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	unsigned int camFlags, int8_t depth, TrackedCallback pressResponse, TrackedCallback releaseResponse );

#endif // inclusion guard
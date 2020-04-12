#include "scissor.h"

typedef struct {
	Vector2 upperLeft;
	Vector2 size;
} ScissorArea;

// spot 0 is always reserved for the full screen area
#define MAX_SCISSOR_STACK_SIZE 32
static int stack[MAX_SCISSOR_STACK_SIZE];
static int stackTop;

#define MAX_SCISSOR_AREAS 64
static ScissorArea areas[MAX_SCISSOR_AREAS];
static int lastAreaIdx;

int scissor_Init( int renderWidth, int renderHeight )
{
	areas[0].upperLeft = VEC2_ZERO;
	areas[0].size.w = (float)renderWidth;
	areas[0].size.h = (float)renderHeight;

	stack[0] = 0;

	lastAreaIdx = 0;
	stackTop = 0;

	return 0;
}

void scissor_Clear( void )
{
	lastAreaIdx = 0;
	stackTop = 0;
}

int scissor_Push( const Vector2* upperLeft, const Vector2* size )
{
	if( lastAreaIdx >= ( MAX_SCISSOR_AREAS - 1 ) ) {
		return -1;
	}

	if( stackTop >= ( MAX_SCISSOR_STACK_SIZE - 1 ) ) {
		return -1;
	}

	++lastAreaIdx;
	++stackTop;

	areas[lastAreaIdx].upperLeft = (*upperLeft);
	areas[lastAreaIdx].size = (*size);
	stack[stackTop] = lastAreaIdx;

	return 0;
}

int scissor_Pop( void )
{
	if( stackTop == 0 ) {
		return 1;
	}

	--stackTop;
	return 0;
}

int scissor_GetTopID( void )
{
	return stack[stackTop];
}

int scissor_GetScissorArea( int id, Vector2* outUpperLeft, Vector2* outSize )
{
	int ret = 0;
	if( id > lastAreaIdx ) {
		id = 0;
		ret = -1;
	}

	(*outUpperLeft) = areas[id].upperLeft;
	(*outSize) = areas[id].size;

	return ret;
}

int scissor_GetScissorAreaGL( int id, GLint* outX, GLint* outY, GLsizei* outW, GLsizei* outH )
{
	Vector2 upperLeft;
	Vector2 size;
	int ret = 0;

	ret = scissor_GetScissorArea( id, &upperLeft, &size );

	(*outX) = (GLint)upperLeft.x;
	(*outY) = (GLint)( areas[0].size.h - ( upperLeft.y + size.h ) );
	(*outW) = (GLint)size.x;
	(*outH) = (GLint)size.y;

	return ret;
}
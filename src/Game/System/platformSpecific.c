#include "platformSpecific.h"

#include "System/platformLog.h"

#ifdef __ANDROID__

#include <jni.h>

static int safeLeftInset = 0;
static int safeRightInset = 0;
static int safeTopInset = 0;
static int safeBottomInset = 0;

// remember to change this if you change the name of the base class
JNIEXPORT void JNICALL
Java_com_Xturos_Game_Game_safeAreaChanged( JNIEnv* env, jclass clazz,
	jint leftInset, jint topInset,
	jint rightInset, jint bottomInset )
{
	//llog( LOG_DEBUG, "Setting new safe area: %i, %i, %i, %i", leftInset, topInset, rightInset, bottomInset );
	safeLeftInset = leftInset;
	safeTopInset = topInset;
	safeRightInset = rightInset;
	safeBottomInset = bottomInset;
}

void getSafeArea( int* outLeft, int* outRight, int* outTop, int* outBottom )
{
	( *outLeft ) = safeLeftInset;
	( *outTop ) = safeTopInset;
	gfx_GetWindowSize( outRight, outBottom );
	( *outRight ) -= safeRightInset;
	( *outBottom ) -= safeBottomInset;
}

#elif __IPHONEOS__

#else

// just use the window, can use this for quickly debugging layout stuff as well
#include "Graphics/graphics.h"
void getSafeArea( int* outLeft, int* outRight, int* outTop, int* outBottom )
{
	// just use the default window size as the safe area
	( *outLeft ) = 0;
	( *outTop ) = 0;
	gfx_GetWindowSize( outRight, outBottom );

	/*( *outLeft ) += 50;
	( *outRight ) -= 50;
	( *outTop ) += 50;
	( *outBottom ) -= 50;//*/
}
#endif
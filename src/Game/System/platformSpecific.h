#ifndef PLATFORM_SPECIFIC_H
#define PLATFORM_SPECIFIC_H

// store any thing that will require communication with the platform libraries here
void platform_GetSafeArea( int* outLeftInset, int* outRightInset, int* outTopInset, int* outBottomInset );

#endif
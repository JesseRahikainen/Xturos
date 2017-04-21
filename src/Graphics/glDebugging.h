#ifndef GL_DEBUGGING_H
#define GL_DEBUGGING_H

#include "../Graphics/glPlatform.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

//#define DEBUG_GL

// all the different combinations D: Description, R: GL function return, C: Check error return
#ifdef DEBUG_GL
#define GL(x) do { (x); checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLD(d,x) do { (x); checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLR(r,x) do { (r)=(x); checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLC(c,x) do { (x); (c)=checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLDR(d,r,x) do { (r)=(x); checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLDC(d,c,x) do { (x); (c)=checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLRC(r,c,x) do { (r)=(x); (c)=checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLDRC(d,r,c,x) do { (r)=(x); (c)=checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#else
#define GL(x) do { (x); } while(0,0)
#define GLD(d,x) do { (x); } while(0,0)
#define GLR(r,x) do { (r)=(x); } while(0,0)
#define GLC(c,x) do { (x); (c)=checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLDR(d,r,x) do { (r)=(x); } while(0,0)
#define GLDC(d,c,x) do { (x); (c)=checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLRC(r,c,x) do { (r)=(x); (c)=checkAndLogErrors( __FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#define GLDRC(d,r,c,x) do { (r)=(x); (c)=checkAndLogErrors( d" "__FILE__":"TOSTRING(__LINE__) ); } while(0,0)
#endif

int checkAndLogErrors( const char* extraInfo );
int checkAndLogFrameBufferCompleteness( GLenum target, const char* extraInfo );

#endif /* inclusion guard */
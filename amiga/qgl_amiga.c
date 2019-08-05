#include "../ref_gl/gl_local.h"
#include "dll.h"

void ( APIENTRY * qglColorTableEXT)( int, int, int, int, int, const void * );
void ( APIENTRY * qglMTexCoord2fSGIS)( GLenum, GLfloat, GLfloat );
void ( APIENTRY * qglLockArraysEXT) (int , int);
void ( APIENTRY * qglUnlockArraysEXT) (void);
void ( APIENTRY * qglPointParameterfEXT)( GLenum param, GLfloat value );
void ( APIENTRY * qglPointParameterfvEXT)( GLenum param, const GLfloat *value );
void ( APIENTRY * qglSelectTextureSGIS)( GLenum );
void ( APIENTRY * qglActiveTextureARB)( GLenum );
void ( APIENTRY * qglClientActiveTextureARB)( GLenum );

void *qwglGetProcAddress(char *x)
{
    	return 0;
}

void QGL_Shutdown(void)
{
}

qboolean QGL_Init(const char *dllname)
{
    	qglColorTableEXT        = 0; //glColorTable;
    	qglMTexCoord2fSGIS      = 0;
    	qglLockArraysEXT        = 0;
    	qglUnlockArraysEXT      = 0;
    	qglPointParameterfEXT   = 0; 
    	qglPointParameterfvEXT  = 0;
    	qglSelectTextureSGIS    = 0;
 
    	return true;
}

void GLimp_EnableLogging( qboolean enable )
{
}

void GLimp_LogNewFrame( void )
{
}

#if 0 // now in each gla_imp*.c dll

#if 0
DLLFUNC void* dllFindResource(int id, char *pType)
{
	return NULL;
}

DLLFUNC void* dllLoadResource(void *pHandle)
{
	return NULL;
}

DLLFUNC void dllFreeResource(void *pHandle)
{
	return;
}
#endif

extern struct Window *GetWindowHandle(void);

extern refexport_t GetRefAPI (refimport_t rimp );

dll_tExportSymbol DLL_ExportSymbols[]=
{
	//{(void *)dllFindResource,"dllFindResource"},
	//{(void *)dllLoadResource,"dllLoadResource"},
	//{(void *)dllFreeResource,"dllFreeResource"},
	{(void *)GetRefAPI,"GetRefAPI"},
	{(void *)GetWindowHandle, "GetWindowHandle"},
	{0, 0}
};

dll_tImportSymbol DLL_ImportSymbols[] =
{
	{0, 0, 0, 0}
};

int DLL_Init()
{
	return 1L;
}

void DLL_DeInit()
{
}

#if defined (__GNUC__)
extern int main(int, char **); // new Cowcat
#endif

#endif

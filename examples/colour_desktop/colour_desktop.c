/**
 *  @file     colour_desktop.c
 *
 *  @brief    a compiz desktop colour management plug-in
 *
 *  @author   Kai-Uwe Behrmann, based on Tomas' color filter, based on Gerhard
 *            Fürnkranz' GLSL ppm_viewer
 *  @par Copyright:
 *            2008 (C) Gerhard Fürnkranz, 2008 (C) Tomas Carnecky,
              2009 (C) Kai-Uwe Behrmann
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2009/02/23
 */


#include <assert.h>

#define GL_GLEXT_PROTOTYPES
#define _BSD_SOURCE

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <X11/extensions/Xfixes.h>

#define HAVE_XRANDR
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#include <compiz-core.h>

#include <assert.h>
#include <math.h>     // floor()
#include <string.h>   // strdup()
#include <sys/time.h>
#include <time.h>
#include <unistd.h>   // getpid()

#include <stdarg.h>
#include <icc34.h>
#include <oyranos_alpha.h>
#include <oyranos_cmm.h> // oyCMMptr_New
#include "oyranos_definitions.h" /* ICC Profile in X */

#include <X11/Xcm.h>

//#define OY_CACHE 1 /* aching in Oyranos is slower */

#define OY_COMPIZ_VERSION (COMPIZ_VERSION_MAJOR * 10000 + COMPIZ_VERSION_MINOR * 100 + COMPIZ_VERSION_MICRO)
#if OY_COMPIZ_VERSION < 708
#define oyCompLogMessage(disp_, plug_in_name, debug_level, format_, ... ) \
        compLogMessage( disp_, plug_in_name, debug_level, format_, __VA_ARGS__ )
#else
#define oyCompLogMessage(disp_, plug_in_name, debug_level, format_, ... ) \
        compLogMessage( plug_in_name, debug_level, format_, __VA_ARGS__ )
#endif

/* Uncomment the following line if you want to enable debugging output */
#define PLUGIN_DEBUG 1

/**
 * The 3D lookup texture has 64 points in each dimension, using 16 bit integers.
 * That means each active region will use 1.5MiB of texture memory.
 */
#define GRIDPOINTS 64

#define STENCIL_ID (pw->stencil_id*ps->nCcontexts + i + 1)


#define DBG_STRING "\n  %s:%d %s() %.02f "
#define DBG_ARGS (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__),__LINE__,__func__,(double)clock()/CLOCKS_PER_SEC
#if defined(PLUGIN_DEBUG_)
#define START_CLOCK(text) fprintf( stderr, DBG_STRING text " - ", DBG_ARGS );
#define END_CLOCK         fprintf( stderr, "%.02f\n", (double)clock()/CLOCKS_PER_SEC );
#else
#define START_CLOCK(text)
#define END_CLOCK
#endif



typedef CompBool (*dispatchObjectProc) (CompPlugin *plugin, CompObject *object, void *privateData);

/** Be active once and then not again. */
static int colour_desktop_can = 1;
/** to be ignored profiles */
static oyProfiles_s * ignore = 0;


/**
 * When a profile is uploaded into the root window, the plugin fetches the 
 * property and creates a lcms profile object. Each profile has a reference
 * count to allow clients to share profiles. When the ref-count drops to zero,
 * the profile is released.
 */
typedef struct {
  uint8_t md5[16];
  oyProfile_s * oy_profile;

  unsigned long refCount;
} PrivColorProfile;

/**
 * The XserverRegion is dereferenced only when the client sends a
 * _NET_COLOR_MANAGEMENT ClientMessage to its window. This allows clients to
 * change the region as the window is resized and later send _N_C_M to tell the
 * plugin to re-fetch the region from the server.
 * The profile is resolved as soon as the client uploads the regions into the
 * window. That means clients need to upload profiles first and then the 
 * regions. Otherwise the plugin won't be able to find the profile and no color
 * transformation will be done.
 */
typedef struct {
  /* These members are only valid when this region is part of the
   * active stack range. */
  Region xRegion;
} PrivColorRegion;

/**
 * Output profiles are currently only fetched using XRandR. For backwards 
 * compatibility the code should fall back to root window properties 
 * (OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE).
 */
typedef struct {
  char name[32];
  oyProfile_s * oy_profile;
  GLushort clut[GRIDPOINTS][GRIDPOINTS][GRIDPOINTS][3];
  GLuint glTexture;
  GLfloat scale, offset;
  XRectangle xRect;
} PrivColorOutput;


static CompMetadata pluginMetadata;

static int core_priv_index = -1;
static int display_priv_index = -1;
static int screen_priv_index = -1;
static int window_priv_index = -1;

typedef struct {
  int childPrivateIndex;

  ObjectAddProc objectAdd;
} PrivCore;

typedef struct {
  int childPrivateIndex;

  HandleEventProc handleEvent;

  /* ClientMessage sent by the application */
  Atom netColorManagement;

  /* Window properties */
  Atom netColorProfiles;
  Atom netColorRegions;
  Atom netColorTarget;
  Atom netColorDesktop;
} PrivDisplay;

typedef struct {
  int childPrivateIndex;

  /* hooked functions */
  DrawWindowProc drawWindow;
  DrawWindowTextureProc drawWindowTexture;

  /* profiles attached to the screen */
  unsigned long nProfiles;
  PrivColorProfile *profile;

  /* compiz fragement function */
  int function, param, unit;
  int function_2, param_2, unit_2;

  /* XRandR outputs and the associated profiles */
  unsigned long nCcontexts;
  PrivColorOutput *ccontexts;
} PrivScreen;

typedef struct {
  /* stencil buffer id */
  unsigned long stencil_id;

  /* regions attached to the window */
  unsigned long nRegions;
  PrivColorRegion *pRegion;

  /* old absolute region */
  oyRectangle_s * absoluteWindowRectangleOld;

  /* active stack range */
  unsigned long active;

  /* active XRandR output */
  char *output;
} PrivWindow;

static Region absoluteRegion(CompWindow *w, Region region);
static void damageWindow(CompWindow *w, void *closure);
oyPointer  pluginGetPrivatePointer   ( CompObject        * o );
static int updateNetColorDesktopAtom ( CompScreen        * s,
                                       PrivScreen        * ps,
                                       int                 request );
static int     hasScreenProfile      ( CompScreen        * s,
                                       int                 screen,
                                       int                 server );
static void    setupICCprofileAtoms  ( CompScreen        * s,
                                       int                 screen,
                                       int                 init );
static int     getDeviceProfile      ( CompScreen        * s,
                                       PrivScreen        * ps,
                                       oyConfig_s        * device,
                                       int                 screen );
static void    setupColourTables     ( CompScreen        * s,
                                       oyConfig_s        * device,
                                       int                 screen );

/**
 *    Private Data Allocation
 *
 * These are helper functions that really should be part of compiz. The private
 * data setup and handling currently requires macros and duplicates code all 
 * over the place. These functions, along with the object setup code (at the 
 * very bottom of this source file) make it much simpler.
 */
#if 0
static void *compObjectGetPrivateIndex(CompObject *o)
{
  if (o == NULL)
    return &corePrivateIndex;

  return compObjectGetPrivateIndex(o->parent);
}

static CompObject * compObjectGetTopLeave( CompObject * o )
{
  while(o->parent)
    o = o->parent;
  return o;
}
#endif

static void *compObjectGetPrivate(CompObject *o)
{
  /*
  int *privateIndex = 0;

  if(!o)
    return NULL;

  privateIndex = compObjectGetPrivateIndex( compObjectGetTopLeave( o ));
  if (privateIndex == NULL)
    return NULL;

  return o->privates[*privateIndex].ptr;
  */

  oyPointer private_data = pluginGetPrivatePointer( o );
  return private_data;
}

#if 0
static void *compObjectAllocPrivate(CompObject *parent, CompObject *object, int size)
{
  int *privateData = 0;
  int *privateIndex = compObjectGetPrivateIndex(parent);
  if (privateIndex == NULL)
    return NULL;

  privateData = malloc(size);
  if (privateData == NULL)
    return NULL;

  memset( privateData, 0, size );

  /* allocate an index for child objects */
  if (object->type < 3) {
    *privateData = compObjectAllocatePrivateIndex(object, object->type + 1);
    if (*privateData == -1) {
      free(privateData);
      return NULL;
    }
  }


  object->privates[*privateIndex].ptr = privateData;

  return privateData;
}
#endif

static void compObjectFreePrivate(CompObject *o)
{
  int index = -1;
  switch(o->type)
  {
    case COMP_OBJECT_TYPE_CORE:
           index = core_priv_index;
         break;
    case COMP_OBJECT_TYPE_DISPLAY:
           index = display_priv_index;
         break;
    case COMP_OBJECT_TYPE_SCREEN:
           index = screen_priv_index;
         break;
    case COMP_OBJECT_TYPE_WINDOW:
           index = window_priv_index;
         break;
  }

  if(index < 0)
    return;

  oyPointer ptr = o->privates[index].ptr;
  o->privates[index].ptr = NULL;
  if(ptr)
    free(ptr);
}

/**
 * Xcolor helper functions. I didn't really want to put them into the Xcolor
 * library.
 * Other window managers are free to copy those when needed.
 */

static inline XcolorRegion *XcolorRegionNext(XcolorRegion *region)
{
  unsigned char *ptr = (unsigned char *) region;
  return (XcolorRegion *) (ptr + sizeof(XcolorRegion));
}

static inline unsigned long XcolorRegionCount(void *data, unsigned long nBytes)
{
  return nBytes / sizeof(XcolorRegion);
}


/**
 * Here begins the real code
 */

static int getFetchTarget(CompTexture *texture)
{
  if (texture->target == GL_TEXTURE_2D) {
    return COMP_FETCH_TARGET_2D;
  } else {
    return COMP_FETCH_TARGET_RECT;
  }
}

/**
 * The shader is the same for all windows and profiles. It only depends on the
 * 3D texture and two environment variables.
 */
static int getProfileShader(CompScreen *s, CompTexture *texture, int param, int unit)
{
  PrivScreen *ps = compObjectGetPrivate((CompObject *) s);
  int function = -1;

#if defined(PLUGIN_DEBUG_)
  oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                  DBG_STRING "Shader request: %d/%d %d/%d/%d %d/%d/%d",DBG_ARGS,
                  ps->function, ps->function_2,
                  param, ps->param, ps->param_2,
                  unit, ps->unit, ps->unit_2);
#endif

  if (ps->function && ps->param == param && ps->unit == unit)
    return ps->function;

  if(ps->function_2 && ps->param_2 == param && ps->unit_2 == unit)
    return ps->function_2;

  if( ps->function_2)
    destroyFragmentFunction(s, ps->function_2);
  /*else if(ps->function)  should not happen as funcition is statical cached
    destroyFragmentFunction(s, ps->function); */

  CompFunctionData *data = createFunctionData();

  addFetchOpToFunctionData(data, "output", NULL, getFetchTarget(texture));

  /* needed? */
  addDataOpToFunctionData(data, "MAD output, output, program.env[%d], program.env[%d];", param, param + 1);
  /* output.rgb means: do not touch alpha */
  addDataOpToFunctionData(data, "TEX output.rgb, output, texture[%d], 3D;", unit);
  /* needless? */
  //addColorOpToFunctionData (data, "output", "output");

  function = createFragmentFunction(s, "colour_desktop", data);

#if defined(PLUGIN_DEBUG)
  oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                  DBG_STRING "Shader compiled: %d/%d/%d", DBG_ARGS,
                  function, param, unit);
#endif


  if(ps->param == -1)
  {
    ps->function = function;
    ps->param = param;
    ps->unit = unit;
    return ps->function;
  } else
  {
    ps->function_2 = function;
    ps->param_2 = param;
    ps->unit_2 = unit;
    return ps->function_2;
  }
}

/**
 * Converts a server-side region to a client-side region.
 */
static Region convertRegion(Display *dpy, XserverRegion src)
{
  Region ret = XCreateRegion();

  int nRects = 0;
  XRectangle *rect = XFixesFetchRegion(dpy, src, &nRects);

  for (int i = 0; i < nRects; ++i) {
    XUnionRectWithRegion(&rect[i], ret, ret);
  }

  XFree(rect);

  return ret;
}

static Region windowRegion( CompWindow * w )
{
  Region r = XCreateRegion();
  XRectangle rect = {0,0,w->serverWidth, w->serverHeight};
  XUnionRectWithRegion( &rect, r, r );
 return r;
}

/**
 * Generic function to fetch a window property.
 */
static void *fetchProperty(Display *dpy, Window w, Atom prop, Atom type, unsigned long *n, Bool delete)
{
  Atom actual;
  int format;
  unsigned long left;
  unsigned char *data;

  XFlush( dpy );

  int result = XGetWindowProperty(dpy, w, prop, 0, ~0, delete, type, &actual, &format, n, &left, &data);
#if defined(PLUGIN_DEBUG_)
  fprintf( stderr, DBG_STRING "%s delete=%d %s %lu\n", DBG_ARGS,
                XGetAtomName( dpy, prop ), delete,
                (result == Success) ? "fine" : "err", *n );
#endif
  if (result == Success)
    return (void *) data;

  return NULL;
}

static unsigned long colour_desktop_stencil_id_pool = 0;

/**
 * Called when new regions have been attached to a window. Fetches these and
 * saves them in the local list.
 */
static void updateWindowRegions(CompWindow *w)
{
  PrivWindow *pw = compObjectGetPrivate((CompObject *) w);

  CompDisplay *d = w->screen->display;
  PrivDisplay *pd = compObjectGetPrivate((CompObject *) d);

  /* free existing data structures */
  for (unsigned long i = 0; i < pw->nRegions; ++i) {
    if (pw->pRegion[i].xRegion != 0) {
      XDestroyRegion(pw->pRegion[i].xRegion);
    }
  }
  if (pw->nRegions)
    free(pw->pRegion);
  pw->nRegions = 0;
  oyRectangle_Release( &pw->absoluteWindowRectangleOld );


  /* fetch the regions */
  unsigned long nBytes;
  void *data = fetchProperty(d->display, w->id, pd->netColorRegions, XA_CARDINAL, &nBytes, False);

  /* allocate the list */
  unsigned long count = 1;
  if(data)
    count += XcolorRegionCount(data, nBytes + 1);

  pw->pRegion = malloc(count * sizeof(PrivColorRegion));
  if (pw->pRegion == NULL)
    goto out;

  memset(pw->pRegion, 0, count * sizeof(PrivColorRegion));

  /* get the complete windows region and put it at the end */
  pw->pRegion[count-1].xRegion = windowRegion( w );


  /* fill in the possible application region(s) */
  XcolorRegion *region = data;
  Region wRegion = pw->pRegion[count-1].xRegion;
  for (unsigned long i = 0; i < (count - 1); ++i)
  {
    pw->pRegion[i].xRegion = convertRegion( d->display, ntohl(region->region) );

#if defined(PLUGIN_DEBUG_)
    BOX * b = &pw->pRegion[i].xRegion->extents;
    if(b)
    printf( DBG_STRING "\n  substract region[%d] %dx%d+%d+%d\n",DBG_ARGS,(int)i,
            b->x2 - b->x1, b->y2 - b->y1, b->x1, b->y1 );
#endif

    /* substract a application region from the window region */
    XSubtractRegion( wRegion, pw->pRegion[i].xRegion, wRegion );

    region = XcolorRegionNext(region);
  }

  pw->nRegions = count;
  pw->active = 1;
  if(pw->nRegions > 1)
    pw->stencil_id = colour_desktop_stencil_id_pool++;
  else
    pw->stencil_id = 0;

#if defined(PLUGIN_DEBUG_)
  oyCompLogMessage(d, "colour_desktop", CompLogLevelDebug, "\n  Updated window regions, %d total now; id:%d %dx%d", count, pw->stencil_id, w->serverWidth,w->serverHeight);
#endif

  pw->absoluteWindowRectangleOld = oyRectangle_NewWith( 0,0, w->serverWidth, w->serverHeight, 0 );

  addWindowDamage(w);

out:
  XFree(data);
}


/**
 * Called when the window target (_NET_COLOR_TARGET) has been changed.
 */
static void updateWindowOutput(CompWindow *w)
{
  PrivWindow *pw = compObjectGetPrivate((CompObject *) w);

  CompDisplay *d = w->screen->display;
  PrivDisplay *pd = compObjectGetPrivate((CompObject *) d);

  if (pw->output)
    XFree(pw->output);

  unsigned long nBytes;
  pw->output = fetchProperty(d->display, w->id, pd->netColorTarget, XA_STRING, &nBytes, False);

#if defined(_NET_COLOR_DEBUG)
  oyCompLogMessage(d, "colour_desktop", CompLogLevelDebug, "Updated window output, target is %s", pw->output);
#endif

  if(!pw->nRegions)
    addWindowDamage(w);
}

static void cdCreateTexture( PrivColorOutput *ccontext )
{
    glBindTexture(GL_TEXTURE_3D, ccontext->glTexture);

    ccontext->scale = (GLfloat) (GRIDPOINTS - 1) / GRIDPOINTS;
    ccontext->offset = (GLfloat) 1.0 / (2 * GRIDPOINTS);

#if defined(PLUGIN_DEBUG_)
    printf( DBG_STRING "\n", DBG_ARGS );
#endif

    glGenTextures(1, &ccontext->glTexture);
    glBindTexture(GL_TEXTURE_3D, ccontext->glTexture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage3D( GL_TEXTURE_3D, 0, GL_RGB16, GRIDPOINTS,GRIDPOINTS,GRIDPOINTS,
                  0, GL_RGB, GL_UNSIGNED_SHORT, ccontext->clut);
}

static int     hasScreenProfile      ( CompScreen        * s,
                                       int                 screen,
                                       int                 server )
{
  char num[12];
  Window root = RootWindow( s->display->display, 0 );
  char * icc_profile_atom = calloc( 1024, sizeof(char) );
  Atom a;
  oyPointer data;
  unsigned long n = 0;

  if(!icc_profile_atom) return 0;

  snprintf( num, 12, "%d", (int)screen );
  if(server)
  snprintf( icc_profile_atom, 1024, OY_ICC_COLOUR_SERVER_TARGET_PROFILE_IN_X_BASE"%s%s", 
            screen ? "_" : "", screen ? num : "" );
  else
  snprintf( icc_profile_atom, 1024, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE"%s%s", 
            screen ? "_" : "", screen ? num : "" );


  a = XInternAtom(s->display->display, icc_profile_atom, False);

  data = fetchProperty( s->display->display, root, a, XA_CARDINAL,
                          &n, False);
  if(data) XFree(data);
  free(icc_profile_atom);
  return (int)n;
}

static void changeProperty           ( Display           * display,
                                       Atom                target_atom,
                                       int                 type,
                                       void              * data,
                                       unsigned long       size )
{
#ifdef DEBUG
  fprintf( stderr, DBG_STRING"set %s size %d\n", DBG_ARGS,
          XGetAtomName( display, target_atom ), size );
#endif
    XChangeProperty( display, RootWindow( display, 0 ),
                     target_atom, type, 8, PropModeReplace,
                     data, size );
}

static void    setupICCprofileAtoms  ( CompScreen        * s,
                                       int                 screen,
                                       int                 init )
{
  char num[12];
  Window root = RootWindow( s->display->display, 0 );
  char * icc_profile_atom = calloc( 1024, sizeof(char) ),
       * icc_colour_server_profile_atom = calloc( 1024, sizeof(char) );
  Atom a,da, source_atom, target_atom;

  oyPointer source;
  oyPointer target;
  unsigned long source_n = 0, target_n = 0;

  snprintf( num, 12, "%d", (int)screen );
  snprintf( icc_profile_atom, 1024, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE"%s%s", 
            screen ? "_" : "", screen ? num : "" );
  snprintf( icc_colour_server_profile_atom, 1024, OY_ICC_COLOUR_SERVER_TARGET_PROFILE_IN_X_BASE"%s%s", 
            screen ? "_" : "", screen ? num : "" );


  a = XInternAtom(s->display->display, icc_profile_atom, False);
  da = XInternAtom(s->display->display, icc_colour_server_profile_atom, False);

  /* select the atoms */
  if(init)
  {
    source_atom = a;
    target_atom = da;
  } else
  {
    source_atom = da;
    target_atom = a;
  }

  target = fetchProperty( s->display->display, root, target_atom, XA_CARDINAL,
                          &target_n, False);

  if( !target_n ||
      (target_n && !init) )
  {
    /* copy the real device atom */
    source = fetchProperty( s->display->display, root, source_atom, XA_CARDINAL,
                            &source_n, False);
    changeProperty ( s->display->display,
                     target_atom, XA_CARDINAL,
                     source, source_n );
#if defined(PLUGIN_DEBUG)
    if(init)
      fprintf( stderr, DBG_STRING "copy from %s to %s (%d)\n", DBG_ARGS,
              icc_profile_atom,
              icc_colour_server_profile_atom, (int)source_n );
    else
      fprintf( stderr, DBG_STRING "copy from %s to %s (%d)\n", DBG_ARGS,
              icc_colour_server_profile_atom,
              icc_profile_atom, (int)source_n );
#endif
    XFree( source );
    source = 0; source_n = 0;

    if(init)
    {
      /* setup the OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE(_xxx) atom as document colour space */
      size_t size = 0;
      oyProfile_s * screen_document_profile = oyProfile_FromStd( oyASSUMED_WEB,
                                                                 0 );

      if(!screen_document_profile)
        oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                          DBG_STRING"Could not get oyASSUMED_WEB", DBG_ARGS);

      if(!ignore) ignore = oyProfiles_New(0);
      oyProfile_s * p = oyProfile_Copy( screen_document_profile, 0 );
      /* make shure the profile is ignored */
      oyProfiles_MoveIn( ignore, &p, -1 );

      source = oyProfile_GetMem( screen_document_profile, &size, 0, malloc );
      source_n = size;
      changeProperty ( s->display->display,
                       source_atom, XA_CARDINAL,
                       source, source_n );
#if defined(PLUGIN_DEBUG)
      printf( DBG_STRING "set %s %d and mark (%d)\n", DBG_ARGS,
              icc_profile_atom, (int)source_n, oyProfiles_Count( ignore ) );
#endif

      oyProfile_Release( &screen_document_profile );
      if(source) free( source ); source = 0;
    } else
    {
      /* clear/erase the _ICC_DEVICE_PROFILE(_xxx) atom */
      XDeleteProperty( s->display->display,root, source_atom );
    }

  } else
    if(target_atom && init)
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                        DBG_STRING"icc_colour_server_profile_atom already present %d size:%lu",
                        DBG_ARGS, target_atom, target_n );

  if(icc_profile_atom) free(icc_profile_atom);
  if(icc_colour_server_profile_atom) free(icc_colour_server_profile_atom);
}

static int     getDeviceProfile      ( CompScreen        * s,
                                       PrivScreen        * ps,
                                       oyConfig_s        * device,
                                       int                 screen )
{
  PrivColorOutput * output = &ps->ccontexts[screen];
  oyOption_s * o = 0;
  oyRectangle_s * r = 0;
  const char * device_name = 0;
  char num[12];
  int error = 0, t_err = 0;

  int size = hasScreenProfile( s, screen, 0 );

  int test = 1;

  snprintf( num, 12, "%d", (int)screen );

#if defined(PLUGIN_DEBUG)
    printf(DBG_STRING"HuHu screen %d\n", DBG_ARGS, screen);
#endif
    o = oyConfig_Find( device, "device_rectangle" );
    if( !o )
    {
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                      DBG_STRING"monitor rectangle request failed", DBG_ARGS);
      return 1;
    }
    r = (oyRectangle_s*) oyOption_StructGet( o, oyOBJECT_RECTANGLE_S );
    if( !r )
    {
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                      DBG_STRING"monitor rectangle request failed", DBG_ARGS);
      return 1;
    }
    oyOption_Release( &o );

    output->xRect.x = r->x;
    output->xRect.y = r->y;
    output->xRect.width = r->width;
    output->xRect.height = r->height;

    device_name = oyConfig_FindString( device, "device_name", 0 );
    if(device_name && device_name[0])
    {
      strcpy( output->name, device_name );

#if defined(PLUGIN_DEBUG)
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                      DBG_STRING "  screen output found %s %s",
                      DBG_ARGS, output->name, oyRectangle_Show(r) );
#endif

    } else
    {
       oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
       DBG_STRING "oyDevicesGet list answere included no device_name",DBG_ARGS);

       strcpy( output->name, num );
    }

    o = oyConfig_Find( device, "icc_profile" );

    output->oy_profile = (oyProfile_s*) 
                                  oyOption_StructGet( o, oyOBJECT_PROFILE_S );

#if defined(PLUGIN_DEBUG)
    printf(DBG_STRING"found device icc_profile %d\n", DBG_ARGS, o?1:0);
#endif
    if(!output->oy_profile)
    {
      oyOptions_s * options = 0;
      oyOptions_SetFromText( &options,
                   "//"OY_TYPE_STD"/config/command",
                                       "list", OY_CREATE_NEW );
      oyOptions_SetFromText( &options,
                   "//"OY_TYPE_STD"/config/icc_profile.net_color_region_target",
                                       "yes", OY_CREATE_NEW );
      t_err = oyDeviceGetProfile( device, options, &output->oy_profile );
      oyOptions_Release( &options );
#if defined(PLUGIN_DEBUG)
      printf( DBG_STRING"found net icc_profile 0x%lx %s %d 0x%lx\n", DBG_ARGS,
              (intptr_t)output->oy_profile,
              oyProfile_GetFileName(output->oy_profile, -1),
              t_err, (intptr_t)output);
#endif

      /* oyDeviceGetProfile() might setup _ICC_PROFILE. This causes a according
       * property event. _ICC_PROFILE will be set to oyASSUMED_WEB.
       * We ignore this event. */
      if(!size)
      {
        oyProfile_s * p = oyProfile_FromStd( oyASSUMED_WEB, 0 );

        if(!ignore) ignore = oyProfiles_New(0);
        /* make shure the profile is ignored */
        oyProfiles_MoveIn( ignore, &p, -1 );
      }
    }

    if(output->oy_profile)
    {
      /* check that no sRGB is delivered */
      if(t_err)
      {
        oyProfile_s * web = oyProfile_FromStd( oyASSUMED_WEB, 0 );
        if(oyProfile_Equal( web, output->oy_profile ))
        {
          oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                      DBG_STRING "Output %s ignoring fallback %d",
                      DBG_ARGS, output->name, error);
          oyProfile_Release( &output->oy_profile );
          error = 1;
        }
        oyProfile_Release( &web );
      }
    } else
    {
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                      DBG_STRING "Output %s: no ICC profile found %d",
                      DBG_ARGS, output->name, error);
      error = 1;
    }

#if defined(PLUGIN_DEBUG)
    printf( DBG_STRING"found icc_profile 0x%lx %d 0x%lx\n", DBG_ARGS,
             (intptr_t)output->oy_profile, t_err,  (intptr_t)output);
#endif
  return error;
}


static void    setupColourTables     ( CompScreen        * s,
                                       oyConfig_s        * device,
                                       int                 screen )
{
  PrivScreen *ps = compObjectGetPrivate((CompObject *) s);
  PrivColorOutput * output = &ps->ccontexts[screen];
  int error = 0;
  oyConversion_s * cc;

  if(!colour_desktop_can)
    return;

    if (output->oy_profile)
    {
#if defined(PLUGIN_DEBUG_)  /* expensive lookup */
      const char * tmp = oyProfile_GetFileName( output->oy_profile, 0 );
      
      oyCompLogMessage(s->display, "colour_desktop", CompLogLevelInfo,
             DBG_STRING "Output %s: extracted profile from Oyranos: %s",
             DBG_ARGS, output->name,
             (strrchr(tmp, OY_SLASH_C)) ? strrchr(tmp, OY_SLASH_C) + 1 : tmp );
#endif

      START_CLOCK("create images")
      oyProfile_s * src_profile = oyProfile_FromStd( oyASSUMED_WEB, 0 );
      oyProfile_s * dst_profile = output->oy_profile;

      oyPixel_t pixel_layout = OY_TYPE_123_16;

      oyImage_s * image_in = oyImage_Create( GRIDPOINTS,GRIDPOINTS*GRIDPOINTS,
                                             output->clut, 
                                             pixel_layout, src_profile, 0 );
      oyProfile_Release( &src_profile );
      oyImage_s * image_out= oyImage_Create( GRIDPOINTS,GRIDPOINTS*GRIDPOINTS,
                                             output->clut,
                                             pixel_layout, dst_profile, 0 );
      oyOptions_s * options = 0;
      /* rendering_high_precission maps to lcms' cmsFLAGS_NOTPRECALC */
      error = oyOptions_SetFromText( &options, "//" OY_TYPE_STD "/icc/rendering_high_precission",
                                     "1", OY_CREATE_NEW );
      END_CLOCK

      START_CLOCK("oyConversion_CreateBasicPixels: ")
      cc = oyConversion_CreateBasicPixels( image_in, image_out,
                                                      options, 0 ); END_CLOCK

      if (cc == NULL)
      {
        oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                      DBG_STRING "no conversion created for %s",
                      DBG_ARGS, output->name);
        return;
      }

      START_CLOCK("fill array: ")
      uint16_t in[3];
      for (int r = 0; r < GRIDPOINTS; ++r)
      {
        in[0] = floor((double) r / (GRIDPOINTS - 1) * 65535.0 + 0.5);
        for (int g = 0; g < GRIDPOINTS; ++g) {
          in[1] = floor((double) g / (GRIDPOINTS - 1) * 65535.0 + 0.5);
          for (int b = 0; b < GRIDPOINTS; ++b)
          {
            in[2] = floor((double) b / (GRIDPOINTS - 1) * 65535.0 + 0.5);
            for(int j = 0; j < 3; ++j)
              /* BGR */
              output->clut[b][g][r][j] = in[j];
          }
        }
      } END_CLOCK

      START_CLOCK("oyConversion_RunPixels: ")
      oyConversion_RunPixels( cc, 0 ); END_CLOCK

      START_CLOCK("cdCreateTexture: ")
      cdCreateTexture( output ); END_CLOCK

      oyOptions_Release( &options );
      oyImage_Release( &image_in );
      oyImage_Release( &image_out );
      oyConversion_Release( &cc );

    } else {
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelInfo,
                      DBG_STRING "Output %s: no profile",
                      DBG_ARGS, output->name);
    }

}

static void freeOutput( PrivScreen *ps )
{
  if (ps->nCcontexts > 0)
  {
    for (unsigned long i = 0; i < ps->nCcontexts; ++i)
    {
      if(ps->ccontexts[i].oy_profile)
        oyProfile_Release( &ps->ccontexts[i].oy_profile );
      if(ps->ccontexts[i].glTexture)
        glDeleteTextures( 1, &ps->ccontexts[i].glTexture );
      ps->ccontexts[i].glTexture = 0;
    }
    free(ps->ccontexts);
  }
}

/**
 * Called when XRandR output configuration (or properties) change. Fetch
 * output profiles (if available) or fall back to sRGB.
 * Device profiles are obtained from Oyranos only once at beginning.
 */
static void updateOutputConfiguration(CompScreen *s, CompBool init)
{
  PrivScreen *ps = compObjectGetPrivate((CompObject *) s);
  int error = 0,
      n,
      set = 1;
  oyOptions_s * options = 0;
  oyConfigs_s * devices = 0;
  oyConfig_s * device = 0;

  /* clean memory */
  if(init)
  {
    START_CLOCK("freeOutput:")
    freeOutput(ps); END_CLOCK
  }

  /* obtain device informations, including geometry and ICC profiles
     from the according Oyranos module */
  error = oyOptions_SetFromText( &options, "//" OY_TYPE_STD "/config/command",
                                 "list", OY_CREATE_NEW );
  error = oyOptions_SetFromText( &options, "//" OY_TYPE_STD "/config/device_rectangle",
                                 "true", OY_CREATE_NEW );
  /*error = oyOptions_SetFromText( &options,
                                 "//" OY_TYPE_STD "/config/display_name",
                                 DisplayString( s->display->display ),
                                 OY_CREATE_NEW );*/
  error = oyDevicesGet( OY_TYPE_STD, "monitor", options, &devices );
  n = oyOptions_Count( options );
  //printf( DBG_STRING "options: %d devices: %d %s\n", DBG_ARGS, n, oyConfigs_Count( devices ) );
  oyOptions_Release( &options );

  n = oyConfigs_Count( devices );
#if defined(PLUGIN_DEBUG)
  oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
               DBG_STRING "Oyranos monitor \"%s\" devices found: %d init: %d",
                    DBG_ARGS, DisplayString( s->display->display ), n, init);
#endif

  if(init)
  {
    ps->nCcontexts = n;
    ps->ccontexts = malloc(ps->nCcontexts * sizeof(PrivColorOutput));
    memset( ps->ccontexts, 0, ps->nCcontexts * sizeof(PrivColorOutput));
  }

  if(colour_desktop_can)
  for (unsigned long i = 0; i < ps->nCcontexts; ++i)
  {
    device = oyConfigs_Get( devices, i );

    if(init)
      error = getDeviceProfile( s, ps, device, i );

    if(ps->ccontexts[i].oy_profile)
    {
      setupICCprofileAtoms( s, i, set );
      setupColourTables ( s, device, i );
    } else
    {
      oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                  DBG_STRING "No profile found on desktops %d/%d 0x%lx 0x%lx",
                  DBG_ARGS, i, ps->nCcontexts, &ps->ccontexts[i], ps->ccontexts[i].oy_profile);
    }

    oyConfig_Release( &device );
  }
  oyConfigs_Release( &devices );

#if defined(PLUGIN_DEBUG)
  oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                  DBG_STRING "Updated screen outputs, %d total  (%d)",
                  DBG_ARGS, ps->nCcontexts, init);
#endif
  START_CLOCK("damageWindow(s)")
  {
    int all = 1;
    forEachWindowOnScreen( s, damageWindow, &all );
  } END_CLOCK

  if(init)
    updateNetColorDesktopAtom( s, ps, 2 );
}

/**
 * CompDisplay::handleEvent
 */
static void pluginHandleEvent(CompDisplay *d, XEvent *event)
{
  PrivDisplay *pd = compObjectGetPrivate((CompObject *) d);
  const char * atom_name = 0;

  UNWRAP(pd, d, handleEvent);
  (*d->handleEvent) (d, event);
  WRAP(pd, d, handleEvent, pluginHandleEvent);

  if(!colour_desktop_can)
    return;

  CompScreen * s = findScreenAtDisplay(d, event->xany.window);
  PrivScreen * ps = compObjectGetPrivate((CompObject *) s);

  /* initialise */
  if(s && ps && s->nOutputDev != ps->nCcontexts)
  {
#if defined(PLUGIN_DEBUG)
    printf( DBG_STRING "s->nOutputDev %d != ps->nCcontexts %d\n", DBG_ARGS,
            (int)s->nOutputDev, (int)ps->nCcontexts );
#endif
    updateOutputConfiguration( s, TRUE);
  }
 

  switch (event->type)
  {
  case PropertyNotify:
    atom_name = XGetAtomName( event->xany.display, event->xproperty.atom );
#if defined(PLUGIN_DEBUG)
    if (event->xproperty.atom == pd->netColorProfiles ||
        event->xproperty.atom == pd->netColorRegions ||
        event->xproperty.atom == pd->netColorTarget ||
        event->xproperty.atom == pd->netColorDesktop ||
           strstr( atom_name, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE) != 0 ||
           strstr( atom_name, "EDID") != 0)
      printf( DBG_STRING "PropertyNotify: %s\n", DBG_ARGS, atom_name );
#endif
    if (event->xproperty.atom == pd->netColorRegions) {
      CompWindow *w = findWindowAtDisplay(d, event->xproperty.window);
      updateWindowRegions(w);
    } else if (event->xproperty.atom == pd->netColorTarget) {
      CompWindow *w = findWindowAtDisplay(d, event->xproperty.window);
      updateWindowOutput(w);

    /* let possibly others take over the colour server */
    } else if( event->xproperty.atom == pd->netColorDesktop && atom_name )
    {

      updateNetColorDesktopAtom( s, ps, 0 );

    /* update for a changing monitor profile */
    } else if(
           strstr( atom_name, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE) != 0 &&
           strstr( atom_name, "ICC_PROFILE_IN_X") == 0 )
    {
      if(colour_desktop_can)
      {
        int screen = 0;
        int ignore_profile = 0;
        char * icc_colour_server_profile_atom = malloc(1024);
        char num[12];
        Atom da;

        if(strlen(atom_name) > strlen(OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE"_"))
        sscanf( (const char*)atom_name,
                OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE "_%d", &screen );
        snprintf( num, 12, "%d", (int)screen );

        snprintf( icc_colour_server_profile_atom, 1024,
                  OY_ICC_COLOUR_SERVER_TARGET_PROFILE_IN_X_BASE"%s%s",
                  screen ? "_" : "", screen ? num : "" );

        da = XInternAtom( d->display, icc_colour_server_profile_atom, False);

        if(da)
        {
          unsigned long n = 0;
          char * data = fetchProperty( d->display, RootWindow(d->display,0),
                                       event->xproperty.atom, XA_CARDINAL,
                                       &n, False);
          if(data && n)
          {
            const char * tmp = 0;
            int pn = oyProfiles_Count( ignore ),
                i;
            oyProfile_s * sp = oyProfile_FromMem( n, data, 0,0 ); /* server p */

            /* Ignore to be ignored profiles, which are set from the 
             * colourserver itself. */
            for(i = pn-1; i >= 0; --i)
            {
              oyProfile_s * p = oyProfiles_Get( ignore, i );
              if(oyProfile_Equal( sp, p ))
              {
                oyProfiles_ReleaseAt( ignore, i );
#if defined(PLUGIN_DEBUG)
                tmp = strdup(oyProfile_GetFileName( sp, 0 ));
                printf( DBG_STRING"found ignore[%d] %s and forget\n",
                       DBG_ARGS, i, oyProfile_GetFileName( p, 0 ) );
#endif
                oyProfile_Release( &p );
                oyProfile_Release( &sp );
                ignore_profile = 1;
                break;
              }
              oyProfile_Release( &p );
            }

            if(sp)
            {
              if(ps->nCcontexts > screen)
              {
                oyProfile_Release( &ps->ccontexts[screen].oy_profile );
                ps->ccontexts[screen].oy_profile = sp;
              } else
                oyCompLogMessage( s->display, "colour_desktop",CompLogLevelWarn,
                    DBG_STRING "ccontexts not ready for screen %d / %d",
                    DBG_ARGS, screen, ps->nCcontexts );
              
              changeProperty ( d->display,
                               da, XA_CARDINAL,
                               (unsigned char*)NULL, 0 );
            }
#if defined(PLUGIN_DEBUG)
            if(!tmp) tmp = oyProfile_GetFileName( sp, 0 );
            if(!tmp) tmp = oyProfile_GetText( sp, oyNAME_DESCRIPTION );
            if(!tmp) tmp = "----";
            printf( DBG_STRING"ignor profiles: %d; %s :%s \"%s\"\n",
                    DBG_ARGS, oyProfiles_Count( ignore ), ignore_profile?"ignoring":"accept",
                    atom_name, (strrchr(tmp, OY_SLASH_C)) ? strrchr(tmp, OY_SLASH_C) + 1 : tmp );
      
#endif
            sp = 0;
            XFree( data );
          }
        }

        if(icc_colour_server_profile_atom) free(icc_colour_server_profile_atom);

        if(!ignore_profile)
          updateOutputConfiguration( s, FALSE );
      }
    }
    break;
  case ClientMessage:
    if (event->xclient.message_type == pd->netColorManagement)
    {
#if defined(PLUGIN_DEBUG)
      printf( DBG_STRING "ClientMessage: %s\n", DBG_ARGS,
               XGetAtomName( event->xany.display, event->xclient.message_type) );
#endif
      CompWindow *w = findWindowAtDisplay (d, event->xclient.window);
      PrivWindow *pw = compObjectGetPrivate((CompObject *) w);

      pw->active = 1;
    }
    break;
  default:
#ifdef HAVE_XRANDR
    if (event->type == d->randrEvent + RRNotify) {
      XRRNotifyEvent *rrn = (XRRNotifyEvent *) event;
      CompScreen *s = findScreenAtDisplay(d, rrn->window);
#if defined(PLUGIN_DEBUG)
      printf( DBG_STRING "XRRNotifyEvent %d\n", DBG_ARGS, rrn->subtype );
      /*if(rrn->subtype == RRNotify_OutputProperty)
      {
        XRROutputChangeNotifyEvent * oce = (XRROutputChangeNotifyEvent*) rrn;
      }*/
#endif
      updateOutputConfiguration(s, TRUE);
    }
#endif
    break;
  }
}

/**
 * Make region relative to the window. 
 * Uses static variables to prevent
 * allocating and freeing the Region in pluginDrawWindow().
 */
static Region absoluteRegion(CompWindow *w, Region region)
{
  Region r = XCreateRegion();
  XUnionRegion( region, r, r );

  for (int i = 0; i < r->numRects; ++i) {
    r->rects[i].x1 += w->attrib.x;
    r->rects[i].x2 += w->attrib.x;
    r->rects[i].y1 += w->attrib.y;
    r->rects[i].y2 += w->attrib.y;

    EXTENTS(&r->rects[i], r);
  }

  return r;
}

static void damageWindow(CompWindow *w, void *closure)
{
  PrivWindow *pw = compObjectGetPrivate((CompObject *) w);
  int * all = closure;

  /* scrissored rects seem to be insensible to artifacts from other windows */
  if((pw->stencil_id || (all && *all == 1)) &&
      pw->absoluteWindowRectangleOld /*&&
      (w->type ==1 || w->type == 128) &&
      w->resName*/)
  {
#if defined(PLUGIN_DEBUG_)
    printf( "damaged - %dx%d+%d+%d  %s\n", 
            w->serverWidth, w->serverHeight,w->serverX, w->serverY,
            w->resName?w->resName:"???" );
#endif
    /* what is so expensive */
    addWindowDamage(w);
  }
#if defined(PLUGIN_DEBUG_)
  else
    printf( "%dx%d+%d+%d  resName %s\n", 
            w->serverWidth, w->serverHeight,w->serverX, w->serverY,
            w->resName?w->resName:"???" );
#endif
}


/**
 * CompScreen::drawWindow
 *  The window's texture is mapped on screen.
 *  As this is the second step of drawing a window it is not best suited to
 *  declare a region for colour conversion.
 *  On the other side here can overlapping regions be found to reduce the 
 *  colour transformed area like:
 *  - draw all windows regions into the stencil buffer
 *  - draw all window textures as needed by the flat desktop
 *  - map all windows to the screen
 *  The inclusion of perspective shifts is not reasonably well done.
 */
static Bool pluginDrawWindow(CompWindow *w, const CompTransform *transform, const FragmentAttrib *attrib, Region region, unsigned int mask)
{
  CompScreen *s = w->screen;
  PrivScreen *ps = compObjectGetPrivate((CompObject *) s);
  int i;

  updateNetColorDesktopAtom( s, ps, 0 );

  UNWRAP(ps, s, drawWindow);
  Bool status = (*s->drawWindow) (w, transform, attrib, region, mask);
  WRAP(ps, s, drawWindow, pluginDrawWindow);

  /* If no regions have been enabled, just return as we're done */
  PrivWindow *pw = compObjectGetPrivate((CompObject *) w);

  /* initialise window regions */
  if (pw->active == 0)
    updateWindowRegions( w );

  oyRectangle_s * rect = oyRectangle_NewWith( w->serverX, w->serverY, w->serverWidth, w->serverHeight, 0 );

  /* update to window movements and resizes */
  if( !oyRectangle_IsEqual( rect, pw->absoluteWindowRectangleOld ) )
  {
    forEachWindowOnScreen(s, damageWindow, NULL);

    if(w->serverWidth != pw->absoluteWindowRectangleOld->width ||
       w->serverHeight != pw->absoluteWindowRectangleOld->height)
      updateWindowRegions( w );

    /* Clear the stencil buffer with zero. But we do not know when the loop
     * starts */
    //glClear(GL_STENCIL_BUFFER_BIT);

    oyRectangle_SetByRectangle( pw->absoluteWindowRectangleOld, rect );

#if defined(PLUGIN_DEBUG_)
    printf( DBG_STRING "%s\n", DBG_ARGS, oyRectangle_Show(rect) );
#endif
  }

  oyRectangle_Release( &rect );

  /* skip the stencil drawing for to be scissored windows */
  if( !pw->stencil_id )
    return status;

  PrivColorRegion * window_region = pw->pRegion + pw->nRegions - 1;
  Region aRegion = absoluteRegion( w, window_region->xRegion);

  glEnable(GL_STENCIL_TEST);

  /* Replace the stencil value in places where we'd draw something */
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

  /* Disable color mask as we won't want to draw anything */
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  for( i = 0; i < ps->nCcontexts; ++i )
  {
    /* Each region gets its own stencil value */
    glStencilFunc(GL_ALWAYS, STENCIL_ID, ~0);

    /* intersect window with monitor */
    Region screen = XCreateRegion();
    XUnionRectWithRegion( &ps->ccontexts[i].xRect, screen, screen );    
    Region intersection = XCreateRegion();
    XIntersectRegion( screen, aRegion, intersection );
    BOX * b = &intersection->extents;
    if(b->x1 == 0 && b->x2 == 0 && b->y1 == 0 && b->y2 == 0)
      goto cleanDrawWindow;

#if defined(PLUGIN_DEBUG_)
    //if(b->y2 - b->y1 == 190)
    //if((int)pw->stencil_id == 7)
    printf( DBG_STRING "%dx%d+%d+%d  %d[%d] on %d\n", DBG_ARGS,
            b->x2 - b->x1, b->y2 - b->y1, b->x1, b->y1,
            (int)pw->stencil_id, (int)STENCIL_ID, i );
    b = &region->extents;
    printf( DBG_STRING "region: %dx%d+%d+%d\n", DBG_ARGS,
            b->x2 - b->x1, b->y2 - b->y1, b->x1, b->y1 );
#endif

    w->vCount = w->indexCount = 0;
    (*w->screen->addWindowGeometry) (w, &w->matrix, 1, intersection, region);

    /* If the geometry is non-empty, draw the window */
    if (w->vCount > 0)
    {
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      (*w->drawWindowGeometry) (w);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    cleanDrawWindow:
    XDestroyRegion( intersection );
    XDestroyRegion( screen );
  }

  /* Reset the color mask */
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_STENCIL_TEST);

  XDestroyRegion( aRegion ); aRegion = 0;

#if defined(PLUGIN_DEBUG_)
  printf( DBG_STRING "\n", DBG_ARGS );
#endif

  return status;
}

/**
 * CompScreen::drawWindowTexture
 *  The window's texture or content is drawn here.
 *  Where does we know which monitor we draw on? from
 *  - pluginDrawWindow()
 *  - Oyranos
 */
static void pluginDrawWindowTexture(CompWindow *w, CompTexture *texture, const FragmentAttrib *attrib, unsigned int mask)
{
  CompScreen *s = w->screen;
  PrivScreen *ps = compObjectGetPrivate((CompObject *) s);

  UNWRAP(ps, s, drawWindowTexture);
  (*s->drawWindowTexture) (w, texture, attrib, mask);
  WRAP(ps, s, drawWindowTexture, pluginDrawWindowTexture);

  PrivWindow *pw = compObjectGetPrivate((CompObject *) w);
  if (pw->active == 0)
    return;

  /* Set up the shader */
  FragmentAttrib fa = *attrib;

  int param = allocFragmentParameters(&fa, 2);
  int unit = allocFragmentTextureUnits(&fa, 1);

  int function = getProfileShader(s, texture, param, unit);
  if (function)
    addFragmentFunction(&fa, function);

#if 1
  if( pw->stencil_id )
  {
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
  } else
    glEnable(GL_SCISSOR_TEST);
#endif

  if(w->screen->nOutputDev != ps->nCcontexts)
    oyCompLogMessage( s->display, "colour_desktop", CompLogLevelWarn,
                    DBG_STRING "Need to update screen outputs, %d / %d",
                    DBG_ARGS, ps->nCcontexts, w->screen->nOutputDev );

  for(int i = 0; i < ps->nCcontexts; ++i)
  {
    /* get the window region */
    PrivColorRegion * window_region = pw->pRegion + pw->nRegions - 1;
    Region tmp = absoluteRegion( w, window_region->xRegion);
    Region screen = XCreateRegion();
    XUnionRectWithRegion( &ps->ccontexts[i].xRect, screen, screen );    
    Region intersection = XCreateRegion();

    /* create intersection of window and monitor */
    XIntersectRegion( screen, tmp, intersection );

    BOX * b = &intersection->extents;

    GLint width =  b->x2 - b->x1,
          height = b->y2 - b->y1;
    glScissor( b->x1, s->height - b->y1 - height, width, height);

    if(WINDOW_INVISIBLE(w))
      goto cleanDrawTexture;

    if(b->x1 == 0 && b->x2 == 0 && b->y1 == 0 && b->y2 == 0)
      goto cleanDrawTexture;

    PrivColorOutput * c = &ps->ccontexts[i];
    /* Set the environment variables */
    glProgramEnvParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, param + 0, 
                                c->scale, c->scale, c->scale, 1.0);
    glProgramEnvParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, param + 1,
                                c->offset, c->offset, c->offset, 0.0);

    /* Activate the 3D texture */
    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
    glEnable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, c->glTexture);
    (*s->activeTexture) (GL_TEXTURE0_ARB);

    /* Only draw where the stencil value matches the window and output */
    glStencilFunc(GL_EQUAL, STENCIL_ID, ~0);

#if defined(PLUGIN_DEBUG_)
#if 1
    //if(b->y2 - b->y1 == 190)
    //if((int)pw->stencil_id == 7)
    printf( DBG_STRING "%dx%d+%d+%d  %d[%d] on %d\n", DBG_ARGS,
            b->x2 - b->x1, b->y2 - b->y1, b->x1, b->y1,
            (int)pw->stencil_id, (int)STENCIL_ID, i );
#else
    printf( DBG_STRING "%d[%d]\n", DBG_ARGS, (int)pw->stencil_id,
            (int)(pw->stencil_id*ps->nCcontexts + i + 1));
#endif
#endif

    /* Now draw the window texture */
    UNWRAP(ps, s, drawWindowTexture);
    if(c->oy_profile)
      (*s->drawWindowTexture) (w, texture, &fa, mask);
    else
      /* ignore the shader */
      (*s->drawWindowTexture) (w, texture, attrib, mask);
    WRAP(ps, s, drawWindowTexture, pluginDrawWindowTexture);

    cleanDrawTexture:
    XDestroyRegion( intersection );
    XDestroyRegion( tmp );
    XDestroyRegion( screen );
  }

  /* Deactivate the 3D texture */
  (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
  glBindTexture(GL_TEXTURE_3D, 0);
  glDisable(GL_TEXTURE_3D);
  (*s->activeTexture) (GL_TEXTURE0_ARB);

  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);

#if defined(PLUGIN_DEBUG_)
  printf( DBG_STRING "\n", DBG_ARGS );
#endif
}

#if 0
/**
 * This is really stupid, object->parent isn't inisialized when 
 * pluginInitObject() is called. So this is a wrapper to get the parent because
 * compObjectAllocPrivate() needs it.
 */
static CompObject *getParent(CompObject *object)
{
  switch (object->type) {
  case 0:
    return NULL;
  case 1:
    return (CompObject *) &core;
  case 2:
    return (CompObject *) ((CompScreen *) object)->display;
  case 3:
    return (CompObject *) ((CompWindow *) object)->screen;
  default:
    return NULL;
  }
}
#endif

/**
 *    Object Init Functions
 */

static CompBool pluginInitCore(CompPlugin *plugin, CompObject *object, void *privateData)
{
#if defined(PLUGIN_DEBUG_)
  int dbg_switch = 60;

  while(dbg_switch--)
    sleep(1);
#endif

  return TRUE;
}

/**
 *  Check and update the _NET_COLOR_DESKTOP status atom. It is used to 
 *  communicate to the colour server.
 *
 *  The _NET_COLOR_DESKTOP atom is a string with following usages:
 *  - uniquely identify the colour server
 *  - tell the name of the colour server
 *  - tell the colour server is alive
 *  All sections are separated by one space char ' ' for easy parsing.
 *  The first section contains the pid_t of the process which has set the atom.
 *  The second section contains time since epoch GMT as returned by time(NULL).
 *  The thired section contains the bar '|' separated and surrounded
 *  capabilities:
 *    - NCP  _NET_COLOR_PROFILES
 *    - NCT  _NET_COLOR_TARGET
 *    - NCM  _NET_COLOR_MANAGEMENT
 *    - NCR  _NET_COLOR_REGIONS
 *    - _NET_COLOR_DESKTOP is omitted
 *    - V0.3 indicates version compliance to the _ICC_Profile in X spec
 *  The fourth section contains the server name identifier.
 *
 * @param[in]      request             - 0  update
 *                                     - 2  init
 * @return                             - 0  all fine
 *                                     - 1  inactivate
 *                                     - 2  activate
 *                                     - 3  error
 */
static int updateNetColorDesktopAtom ( CompScreen        * s,
                                       PrivScreen        * ps,
                                       int                 request )
{
  CompDisplay * d = s->display;
  PrivDisplay * pd = compObjectGetPrivate((CompObject *) d);
  static time_t net_color_desktop_last_time = 0;
  time_t  cutime;         /* Time since epoch */
  cutime = time(NULL);    /* current user time */
  const char * my_id = "compiz_colour_desktop",
             * my_capabilities = "|NCR|V0.3|"; /* _NET_COLOR_REGIONS */
  unsigned long n = 0;
  char * data = 0;
  const char * old_atom = 0;
  int status = 0;
 

  /* set the colour management desktop service activity atom */
  pid_t pid = getpid();
  int old_pid = 0;
  long atom_time = 0;
  char * atom_colour_server_name,
       * atom_capabilities_text;

  if(!colour_desktop_can)
    return 1;

  /* check every 10 seconds */
  if(request == 0 &&
     (cutime - net_color_desktop_last_time < (time_t)10))
    return 0;

#if defined(PLUGIN_DEBUG_)
  printf( DBG_STRING "net_color_desktop_last_time: %ld/%ld %d\n",
          DBG_ARGS, cutime-net_color_desktop_last_time, cutime, request );
#endif

  atom_colour_server_name = (char*)malloc(1024);
  atom_capabilities_text = (char*)malloc(1024);
  if(!atom_colour_server_name || !atom_capabilities_text)
  {
    status = 3;
    goto clean_updateNetColorDesktopAtom;
  }

  data = fetchProperty( d->display, RootWindow(d->display,0),
                        pd->netColorDesktop, XA_STRING, &n, False);

  atom_colour_server_name[0] = 0;
  if(n && data && strlen(data))
  {
    sscanf( (const char*)data, "%d %ld %s %s",
            &old_pid, &atom_time,
            atom_capabilities_text, atom_colour_server_name );
    old_atom = data;
  }

  if(n && data && old_pid != (int)pid)
  {
    if(old_atom && atom_time + 60 < cutime)
      oyCompLogMessage( d, "colour_desktop", CompLogLevelWarn,
                    DBG_STRING "\n!!! Found old _NET_COLOR_DESKTOP pid: %s.\n"
                    "Eigther there was a previous crash or your setup can be double colour corrected.",
                    DBG_ARGS, old_atom ? old_atom : "????" );
    /* check for taking over of colour service */
    if(atom_colour_server_name && strcmp(atom_colour_server_name, my_id) != 0)
    {
      if(atom_time < net_color_desktop_last_time ||
         request == 2)
      {
        oyCompLogMessage( d, "colour_desktop", CompLogLevelWarn,
                    DBG_STRING "\nTaking over colour service from old _NET_COLOR_DESKTOP: %s.",
                    DBG_ARGS, old_atom ? old_atom : "????" );
      } else
      if(atom_time > net_color_desktop_last_time)
        oyCompLogMessage( d, "colour_desktop", CompLogLevelWarn,
                    DBG_STRING "\nGiving colour service to _NET_COLOR_DESKTOP: %s.",
                    DBG_ARGS, old_atom ? old_atom : "????" );
     
    } else
    if(old_atom)
      oyCompLogMessage( d, "colour_desktop", CompLogLevelWarn,
                    DBG_STRING "\nTaking over colour service from old _NET_COLOR_DESKTOP: %s.",
                    DBG_ARGS, old_atom ? old_atom : "????" );
  }

  int attached_profiles = 0;
  for(int i = 0; i < ps->nCcontexts; ++i)
    attached_profiles += ps->ccontexts[i].oy_profile ? 1 : 0;

  if( (atom_time + 10) < net_color_desktop_last_time ||
      request == 2 )
  {
    char * atom_text = malloc(1024);
    if(!atom_text) goto clean_updateNetColorDesktopAtom;
    sprintf( atom_text, "%d %ld %s %s",
             (int)pid, (long)cutime, my_capabilities, my_id );
 
   if(attached_profiles)
      changeProperty( d->display,
                                pd->netColorDesktop, XA_STRING,
                                (unsigned char*)atom_text,
                                strlen(atom_text) + 1 );
    else if(old_atom)
    {
      /* switch off the plugin */
      changeProperty( d->display,
                                pd->netColorDesktop, XA_STRING,
                                (unsigned char*)NULL, 0 );
      colour_desktop_can = 0;
    }

    if(atom_text) free( atom_text );
  }

clean_updateNetColorDesktopAtom:
  if(atom_colour_server_name) free(atom_colour_server_name);
  if(atom_capabilities_text) free(atom_capabilities_text);

  net_color_desktop_last_time = cutime;

  return status;
}

static CompBool pluginInitDisplay(CompPlugin *plugin, CompObject *object, void *privateData)
{
  CompDisplay *d = (CompDisplay *) object;
  PrivDisplay *pd = privateData;

  if (d->randrExtension == False)
    return FALSE;

  WRAP(pd, d, handleEvent, pluginHandleEvent);

  printf( DBG_STRING "HUHU\n", DBG_ARGS );

  pd->netColorManagement = XInternAtom(d->display, "_NET_COLOR_MANAGEMENT", False);

  pd->netColorProfiles = XInternAtom(d->display, "_NET_COLOR_PROFILES", False);
  pd->netColorRegions = XInternAtom(d->display, "_NET_COLOR_REGIONS", False);
  pd->netColorTarget = XInternAtom(d->display, "_NET_COLOR_TARGET", False);
  pd->netColorDesktop = XInternAtom(d->display, "_NET_COLOR_DESKTOP", False);

  return TRUE;
}


static CompBool pluginInitScreen(CompPlugin *plugin, CompObject *object, void *privateData)
{
  CompScreen *s = (CompScreen *) object;
  PrivScreen *ps = privateData;
#ifdef HAVE_XRANDR
  int screen = DefaultScreen( s->display->display );
#endif
  fprintf( stderr, DBG_STRING"dev %d contexts %d \n", DBG_ARGS,
          s->nOutputDev, ps->nCcontexts );
    

  GLint stencilBits = 0;
  glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
  if (stencilBits == 0)
    return FALSE;

  WRAP(ps, s, drawWindow, pluginDrawWindow);
  WRAP(ps, s, drawWindowTexture, pluginDrawWindowTexture);

  ps->nProfiles = 0;
  ps->profile = NULL;

  ps->function = 0;
  ps->function_2 = 0;
  ps->param = ps->param_2 = -1;
  ps->unit = ps->unit_2 = -1;

  /* XRandR setup code */

#ifdef HAVE_XRANDR
  XRRSelectInput( s->display->display,
                  XRootWindow( s->display->display, screen ),
                  RROutputPropertyNotifyMask);
#endif

  /* initialisation is done in pluginHandleEvent() by checking ps->nCcontexts */
  ps->nCcontexts = 0;

  return TRUE;
}

static CompBool pluginInitWindow(CompPlugin *plugin, CompObject *object, void *privateData)
{
  /* CompWindow *w = (CompWindow *) object; */
  PrivWindow *pw = privateData;

  pw->nRegions = 0;
  pw->pRegion = 0;
  pw->active = 0;

  pw->absoluteWindowRectangleOld = 0;
  pw->output = NULL;

  return TRUE;
}

static dispatchObjectProc dispatchInitObject[] = {
  pluginInitCore, pluginInitDisplay, pluginInitScreen, pluginInitWindow
};

/**
 *    Object Fini Functions
 */


static CompBool pluginFiniCore(CompPlugin *plugin, CompObject *object, void *privateData)
{
  /* Don't crash if something goes wrong inside lcms */
  //cmsErrorAction(LCMS_ERRC_WARNING);

  return TRUE;
}

static CompBool pluginFiniDisplay(CompPlugin *plugin, CompObject *object, void *privateData)
{
  CompDisplay *d = (CompDisplay *) object;
  PrivDisplay *pd = privateData;

  UNWRAP(pd, d, handleEvent);

  /* remove desktop colour management service mark */
  changeProperty( d->display,
                                pd->netColorDesktop, XA_STRING,
                                (unsigned char*)NULL, 0 );

  return TRUE;
}

static CompBool pluginFiniScreen(CompPlugin *plugin, CompObject *object, void *privateData)
{
  CompScreen *s = (CompScreen *) object;
  PrivScreen *ps = privateData;

  int error = 0,
      n,
      init = 0;
  oyConfigs_s * devices = 0;
  oyConfig_s * device = 0;

  error = oyDevicesGet( OY_TYPE_STD, "monitor", 0, &devices );

  n = oyConfigs_Count( devices );
#if defined(PLUGIN_DEBUG)
  oyCompLogMessage( s->display, "colour_desktop", CompLogLevelDebug,
                  DBG_STRING "Oyranos monitor \"%s\" devices found: %d",
                  DBG_ARGS, DisplayString( s->display->display ), n);
#endif

  /* switch profile atoms back */
  for(int i = 0; i < ps->nCcontexts; ++i)
  {
    device = oyConfigs_Get( devices, i );

    if(ps->ccontexts[i].oy_profile)
      setupICCprofileAtoms( s, i, init );

    oyConfig_Release( &device );
  }
  oyConfigs_Release( &devices );


  /* clean memory */
  freeOutput(ps);

  UNWRAP(ps, s, drawWindow);
  UNWRAP(ps, s, drawWindowTexture);

  return TRUE;
}

static CompBool pluginFiniWindow(CompPlugin *plugin, CompObject *object, void *privateData)
{
  return TRUE;
}

static dispatchObjectProc dispatchFiniObject[] = {
  pluginFiniCore, pluginFiniDisplay, pluginFiniScreen, pluginFiniWindow
};


/**
 *    Plugin Interface
 */
static CompBool pluginInit(CompPlugin *p)
{
#if 0
  corePrivateIndex = allocateCorePrivateIndex();

  if (corePrivateIndex < 0)
    return FALSE;
#endif
  return TRUE;
}

static oyStructList_s * privates_cache = 0;
oyStructList_s * pluginGetPrivatesCache ()
{
  if(!privates_cache)
    privates_cache = oyStructList_New( 0 );
  return privates_cache;
}

int pluginPrivatesRelease( oyPointer * ptr )
{
  if(ptr)
  {
    free(*ptr);
    *ptr = 0;
    return 0;
  }
  return 1;
}

static char * hash_text_ = 0;
static int hash_len_ = sizeof(intptr_t);
/** create a string of the exact Oyranos expected hash size */
const char * pluginGetHashText( CompObject * o )
{
  const char * type_name = "unknown";
  char * ptr;
  unsigned char c,d,e;
  int i;
  if(!hash_text_)
  {
    hash_text_ = (char*) malloc (2*OY_HASH_SIZE);
    memset( hash_text_, 0, 2*OY_HASH_SIZE );
    hash_text_[4 + hash_len_] = 0;
  }
  if(!hash_text_ || !o) return NULL;
  switch(o->type)
  {
    case COMP_OBJECT_TYPE_CORE: type_name = "CORE"; break;
    case COMP_OBJECT_TYPE_DISPLAY: type_name = "DISPLAY"; break;
    case COMP_OBJECT_TYPE_SCREEN: type_name = "SCREEN"; break;
    case COMP_OBJECT_TYPE_WINDOW: type_name = "WINDOW"; break;
  }
  /* use memcpy to be fast */
#if 0
  static int init = 0;
  sprintf( hash_text_, "%s[0x%lx]", type_name, (intptr_t)o );
  if(o->type == COMP_OBJECT_TYPE_DISPLAY)
  if(init++<10)
  printf("%s\n", hash_text_);
#else
  memcpy( hash_text_, type_name, 4 );
  ptr = (char*)&o;
  for(i = 0; i < sizeof(int*); ++i)
  {
    c = ptr[sizeof(int*)-i-1];
    d = c&0x0f;
    e = c >> 4;
    hash_text_[4 + 2*i+1] = d < 10 ? d+48 : d+87;
    hash_text_[4 + 2*i] = e < 10 ? e+48 : e+87;
  }
#if defined(PLUGIN_DEBUG_)
  static int init = 0;
  if(init++<10)
  printf("%s 0x%lx\n", hash_text_, o);
#endif
#endif
  return hash_text_;
}


oyPointer pluginAllocatePrivatePointer( CompObject * o )
{
  oyPointer ptr = 0;
  int index = -1;
  size_t size = 0;
  static const int privateSizes[] = {
  sizeof(PrivCore), sizeof(PrivDisplay), sizeof(PrivScreen), sizeof(PrivWindow)
  };

#ifdef OY_CACHE
  return pluginGetPrivatePointer( o );
#endif

  if(!o)
    return 0;
  switch(o->type)
  {
    case COMP_OBJECT_TYPE_CORE:
           if(core_priv_index == -1)
             core_priv_index = allocateCorePrivateIndex( );
           index = core_priv_index;
           size = privateSizes[o->type];
         break;
    case COMP_OBJECT_TYPE_DISPLAY:
           if(display_priv_index == -1)
             display_priv_index = allocateDisplayPrivateIndex( );
           index = display_priv_index;
           size = privateSizes[o->type];
         break;
    case COMP_OBJECT_TYPE_SCREEN:
         {
           CompScreen * s = (CompScreen*)o;
           if(screen_priv_index == -1)
             screen_priv_index = allocateScreenPrivateIndex( s->display );
           index = screen_priv_index;
           size = privateSizes[o->type];
         }
         break;
    case COMP_OBJECT_TYPE_WINDOW:
         {
           CompWindow * w = (CompWindow*)o;
           if(window_priv_index == -1)
             window_priv_index = allocateWindowPrivateIndex( w->screen );
           index = window_priv_index;
           size = privateSizes[o->type];
         }
         break;
  }
#if defined(PLUGIN_DEBUG_)
  fprintf(stderr, "index %d for %d\n", index, o->type );
#endif

  if(index < 0)
    return 0;

  {
    o->privates[index].ptr = malloc(size);
#if defined(PLUGIN_DEBUG_)
    fprintf(stderr, "index=%d, 0x%lx size=%d\n", 
           index, o->privates[index].ptr, (int)size );
#endif
    if(!o->privates[index].ptr) return 0;
#if defined(PLUGIN_DEBUG_)
    fprintf(stderr, "memset index=%d, 0x%lx size=%d\n", 
           index, o->privates[index].ptr, (int)size );
#endif
    memset( o->privates[index].ptr, 0, size);
  }

  ptr = o->privates[index].ptr;

#if defined(PLUGIN_DEBUG_)
  fprintf(stderr, "return ptr=0x%lx for type=%d[ 0x%lx]\n", ptr, o->type, o );
#endif

  return ptr;
}

oyPointer pluginGetPrivatePointer( CompObject * o )
{
  oyPointer ptr = 0;

  if(!o)
    return 0;
#if OY_CACHE
  uint32_t exact_hash_size = 1;
  static const int privateSizes[] = {
  sizeof(PrivCore), sizeof(PrivDisplay), sizeof(PrivScreen), sizeof(PrivWindow)
  };
  const char * hash_text = pluginGetHashText( o ); if(!hash_text) return FALSE;
  oyHash_s * entry = oyCacheListGetEntry_( pluginGetPrivatesCache(),
                                           exact_hash_size, hash_text );
  oyCMMptr_s * priv_ptr = (oyCMMptr_s*) oyHash_GetPointer_( entry,
                                                        oyOBJECT_CMM_POINTER_S);

  if(!priv_ptr)
  {
    ptr = malloc( privateSizes[o->type] ); if(!ptr) return FALSE;
    memset( ptr, 0, privateSizes[o->type] );
    priv_ptr = oyCMMptr_New( malloc );
    int error = oyCMMptr_Set( priv_ptr, "colour_desktop", hash_text, ptr, 
                              "pluginPrivatesRelease", pluginPrivatesRelease );
#if defined(PLUGIN_DEBUG_)
    printf( DBG_STRING "allocated private data: %s", DBG_ARGS, hash_text );
#endif
    if(error) return FALSE;
    if(error <= 0 && priv_ptr)
      /* update cache entry */
      error = oyHash_SetPointer_( entry, (oyStruct_s*) priv_ptr );
  } else
  {
    ptr = priv_ptr->ptr;
#if defined(PLUGIN_DEBUG_)
    printf( DBG_STRING "private data obtained: %s", DBG_ARGS, hash_text );
#endif
  }
  oyHash_Release_( &entry );
#else
  int index = -1;
  switch(o->type)
  {
    case COMP_OBJECT_TYPE_CORE:
           index = core_priv_index;
         break;
    case COMP_OBJECT_TYPE_DISPLAY:
           index = display_priv_index;
         break;
    case COMP_OBJECT_TYPE_SCREEN:
           index = screen_priv_index;
         break;
    case COMP_OBJECT_TYPE_WINDOW:
           index = window_priv_index;
         break;
  }

  if(index < 0)
    return 0;

  ptr = o->privates[index].ptr;
  if(!ptr)
    fprintf( stderr, "object[0x%lx] type=%d no private data reserved\n",
            (intptr_t)o, o->type );
#endif

  return ptr;
}

static CompBool pluginInitObject(CompPlugin *p, CompObject *o)
{
  /* use Oyranos for caching of private data */
  oyPointer private_data = pluginAllocatePrivatePointer( o );
#if defined(PLUGIN_DEBUG_)
  printf("get data=0x%lx for type=%d[ 0x%lx]\n", private_data, o->type, o );
#endif

#if 0
  void *privateData = compObjectAllocPrivate(getParent(o), o, privateSizes[o->type]);
  if (privateData == NULL)
    return TRUE;
#endif

  if (dispatchInitObject[o->type](p, o, private_data) == FALSE)
    return FALSE; //compObjectFreePrivate(getParent(o), o);
  return TRUE;
}

static void pluginFiniObject(CompPlugin *p, CompObject *o)
{
  void *privateData = compObjectGetPrivate(o);
  if (privateData == NULL)
    return;

  dispatchFiniObject[o->type](p, o, privateData);
  if(!o)
    return;

  /* release a cache entry of private data */
#ifdef OY_CACHE
  uint32_t exact_hash_size = 1;
  const char * hash_text;
  oyHash_s * entry;
  oyCMMptr_s * priv_ptr;
  hash_text = pluginGetHashText( o ); if(!hash_text) return;
  entry = oyCacheListGetEntry_( pluginGetPrivatesCache(), exact_hash_size,
                                hash_text );
  priv_ptr = (oyCMMptr_s*) oyHash_GetPointer_( entry, oyOBJECT_CMM_POINTER_S);
  if(priv_ptr)
    oyCMMptr_Release( &priv_ptr );
  oyHash_SetPointer_( entry, (oyStruct_s*) priv_ptr );
  oyHash_Release_( &entry );
#else
  compObjectFreePrivate( o );
#endif
}

static void pluginFini(CompPlugin *p)
{
  //freeCorePrivateIndex(corePrivateIndex);
  oyStructList_Release( &privates_cache );
}

static CompMetadata *pluginGetMetadata(CompPlugin *p)
{
  return &pluginMetadata;
}

CompPluginVTable pluginVTable = {
  "colour_desktop",
  pluginGetMetadata,
  pluginInit,
  pluginFini,
  pluginInitObject,
  pluginFiniObject,
  0,
  0
};

CompPluginVTable *getCompPluginInfo20070830(void)
{
  return &pluginVTable;
}


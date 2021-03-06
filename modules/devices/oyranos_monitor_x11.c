/** @file oyranos_monitor.c
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  @par Copyright:
 *            2005-2010 (C) Kai-Uwe Behrmann
 *
 *  @brief    monitor device detection
 *  @internal
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2005/01/31
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "limits.h"
#include <unistd.h>  /* intptr_t */
#include <locale.h>

#include "config.h"

# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xatom.h>
# include <X11/Xcm/XcmEdidParse.h>
# if HAVE_XIN
#  include <X11/extensions/Xinerama.h>
# endif
# ifdef HAVE_XF86VMODE
#  include <X11/extensions/xf86vmode.h>
# endif

#include "oyranos.h"
#include "oyranos_alpha.h"
#include "oyranos_internal.h"
#include "oyranos_monitor.h"
#include "oyranos_monitor_internal_x11.h"
#include "oyranos_monitor_internal.h"
#include "oyranos_debug.h"
#include "oyranos_helper.h"
#include "oyranos_sentinel.h"
#include "oyranos_string.h"

/* ---  Helpers  --- */

/* --- internal API definition --- */



int   oyX1Monitor_getScreenFromDisplayName_( oyX1Monitor_s   * disp );
char**oyX1GetAllScreenNames_        (const char *display_name, int *n_scr );
int   oyX1Monitor_getScreenGeometry_   ( oyX1Monitor_s       * disp );
/** @internal Display functions */
const char* oyX1Monitor_name_( oyX1Monitor_s *disp ) { return disp->name; }
const char* oyX1Monitor_hostName_( oyX1Monitor_s *disp ) { return disp->host; }
const char* oyX1Monitor_identifier_( oyX1Monitor_s *disp ) { return disp->identifier; }
/** @internal the screen appendment for the root window properties */
char*       oyX1Monitor_screenIdentifier_( oyX1Monitor_s *disp )
{ char *number = 0;

  oyAllocHelper_m_( number, char, 24, 0, return "");
  number[0] = 0;
  if( disp->geo[1] >= 1 && !disp->screen ) sprintf( number,"_%d", disp->geo[1]);
  return number;
}
int oyX1Monitor_deviceScreen_( oyX1Monitor_s *disp ) { return disp->screen; }
int oyX1Monitor_number_( oyX1Monitor_s *disp ) { return disp->geo[0]; }
int oyX1Monitor_screen_( oyX1Monitor_s *disp ) { return disp->geo[1]; }
int oyX1Monitor_x_( oyX1Monitor_s *disp ) { return disp->geo[2]; }
int oyX1Monitor_y_( oyX1Monitor_s *disp ) { return disp->geo[3]; }
int oyX1Monitor_width_( oyX1Monitor_s *disp ) { return disp->geo[4]; }
int oyX1Monitor_height_( oyX1Monitor_s *disp ) { return disp->geo[5]; }
int   oyX1Monitor_getGeometryIdentifier_(oyX1Monitor_s       * disp );
Display* oyX1Monitor_device_( oyX1Monitor_s *disp ) { return disp->display; }
const char* oyX1Monitor_systemPort_( oyX1Monitor_s *disp ) { return disp->system_port; }
oyBlob_s *  oyX1Monitor_edid_( oyX1Monitor_s * disp ) { return oyBlob_Copy( disp->edid, 0 ); }

oyX11INFO_SOURCE_e
    oyX1Monitor_infoSource_( oyX1Monitor_s *disp ) { return disp->info_source; }
# ifdef HAVE_XRANDR
XRRScreenResources *
    oyX1Monitor_xrrResource_( oyX1Monitor_s * disp ) { return disp->res; }
RROutput
    oyX1Monitor_xrrOutput_( oyX1Monitor_s * disp ) { return disp->output; }
XRROutputInfo *
    oyX1Monitor_xrrOutputInfo_( oyX1Monitor_s * disp ) { return disp->output_info; }
int oyX1Monitor_activeOutputs_( oyX1Monitor_s * disp ) { return disp->active_outputs; }


#endif

char* oyX1Monitor_getAtomName_         ( oyX1Monitor_s       * disp,
                                       const char        * base );
oyBlob_s *   oyX1Monitor_getProperty_  ( oyX1Monitor_s       * disp,
                                       const char        * prop_name,
                                       const char       ** prop_name_xrandr );
char* oyChangeScreenName_            ( const char        * display_name,
                                       int                 screen );
const char *xrandr_edids[] = {"EDID","EDID_DATA",0};






/** @internal
 *  Function oyX1Monitor_getProperty_
 *  @brief   obtain X property
 *
 *  @version Oyranos: 0.1.10
 *  @since   2009/01/17 (Oyranos: 0.1.10)
 *  @date    2009/08/18
 */
oyBlob_s *   oyX1Monitor_getProperty_  ( oyX1Monitor_s       * disp,
                                       const char        * prop_name,
                                       const char       ** prop_name_xrandr )
{
  oyBlob_s * prop = 0;
  Display *display = 0;
  Window w = 0;
  Atom atom = 0, a;
  char *atom_name;
  int actual_format_return;
  unsigned long nitems_return=0, bytes_after_return=0;
  unsigned char* prop_return=0;
  int error = !disp;

  if(!error)
  {
    display = oyX1Monitor_device_( disp );
# ifdef HAVE_XRANDR
    if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR )
    {
      int i = 0;
      if(prop_name_xrandr)
        while(!atom && prop_name_xrandr[i])
          atom = XInternAtom( display,
                              prop_name_xrandr[i++],
                              True );
      else
        atom = XInternAtom( display, prop_name, True );
      DBG_PROG1_S("atom: %ld", atom)

      if(atom)
      {
        error =
        XRRGetOutputProperty ( display, oyX1Monitor_xrrOutput_( disp ),
                      atom, 0, 32,
                      False, False, AnyPropertyType, &a,
                      &actual_format_return, &nitems_return,
                      &bytes_after_return, &prop_return );

      }
    }
# endif
    atom = 0;

    if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XINERAMA ||
        (oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR &&
          !nitems_return) )
    {
      atom_name = oyX1Monitor_getAtomName_( disp, prop_name );
      if(atom_name)
        atom = XInternAtom(display, atom_name, True);
      if(atom)
        w = RootWindow( display, oyX1Monitor_deviceScreen_( disp ) );
      if(w)
        /* AnyPropertyType does not work for OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE ---vvvvvvvvvv */
        XGetWindowProperty( display, w, atom, 0, INT_MAX, False, XA_CARDINAL,
                     &a, &actual_format_return, &nitems_return, 
                     &bytes_after_return, &prop_return );
      if(atom_name)
        oyFree_m_( atom_name )
    }
  }

  if(nitems_return && prop_return)
  {
    prop = oyBlob_New( 0 );
    oyBlob_SetFromData( prop, prop_return, nitems_return + bytes_after_return, 0 );
    XFree( prop_return ); prop_return = 0;
  }

  return prop;
}

/*#define IGNORE_EDID 1*/

int
oyX1GetMonitorInfo_               (const char* display_name,
                                   char**      manufacturer,
                                   char**      mnft,
                                   char**      model,
                                   char**      serial,
                                       char             ** vendor,
                                   char**      display_geometry,
                                       char             ** system_port,
                                       char             ** host,
                                       uint32_t          * week,
                                       uint32_t          * year,
                                       uint32_t          * mnft_id,
                                       uint32_t          * model_id,
                                       double            * colours,
                                       oyBlob_s         ** edid,
                                   oyAlloc_f     allocate_func,
                                       oyStruct_s        * user_data )
{
  int len;
  char * edi=0;
  char *t, * port = 0, * geo = 0;
  oyX1Monitor_s * disp = 0;
  oyBlob_s * prop = 0;
  oyOptions_s * options = (oyOptions_s*) user_data;
  int error = 0;

  DBG_PROG_START

  if(display_name)
    DBG_PROG1_S("display_name %s",display_name);

  disp = oyX1Monitor_newFrom_( display_name, 1 );
  if(!disp)
    return 1;

  if(!allocate_func)
    allocate_func = oyAllocateFunc_;

  if(options && options->type_ != oyOBJECT_OPTIONS_S)
  {
    options = 0;
    WARNcc2_S(user_data, "\n\t  ",_("unexpected user_data type"),
                                  oyStructTypeToText( user_data->type_ ));
  }

  {
    t = 0;
    if( oyX1Monitor_systemPort_( disp ) &&
        oyStrlen_(oyX1Monitor_systemPort_( disp )) )
    {
      len = oyStrlen_(oyX1Monitor_systemPort_( disp ));
      ++len;
      t = (char*)oyAllocateWrapFunc_( len, allocate_func );
      strcpy(t, oyX1Monitor_systemPort_( disp ));
    }
    port = t;
    if( system_port ) 
      *system_port = port;
    t = 0;
  }

  if( display_geometry )
    *display_geometry = oyStringCopy_( oyX1Monitor_identifier_( disp ),
                                       allocate_func );
  else
    geo = oyStringCopy_( oyX1Monitor_identifier_( disp ),
                                       oyAllocateFunc_ );
  if( host )
    *host = oyStringCopy_( oyX1Monitor_hostName_( disp ), allocate_func );

#if !defined(IGNORE_EDID)
  prop = oyX1Monitor_getProperty_( disp, "XFree86_DDC_EDID1_RAWDATA",
                                       xrandr_edids );
#endif

  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XINERAMA &&
      ((!prop || (prop && oyBlob_GetSize(prop)%128)) ||
       oyOptions_FindString( options, "edid", "refresh" )) )
  {
    int error = 0;
    char * txt = oyAllocateFunc_(1024); txt[0] = 0;

    /* test twinview edid */
    if(oy_debug)
      oySnprintf1_( txt, 1024, "OYRANOS_DEBUG=%d ", oy_debug);
    oySnprintf1_( &txt[strlen(txt)], 1024, "%s",
              "PATH=" OY_BINDIR ":$PATH; oyranos-monitor-nvidia -p" );

    error = system( txt );
    if(txt) { oyDeAllocateFunc_(txt); txt = 0; }

#if !defined(IGNORE_EDID)
    prop = oyX1Monitor_getProperty_( disp, "XFree86_DDC_EDID1_RAWDATA",
                                         xrandr_edids );
#endif
  }

  if( prop )
  {
    if( oyBlob_GetSize(prop)%128 )
    {
      WARNcc4_S(user_data, "\n\t  %s %d; %s %s",_("unexpected EDID lenght"),
               (int)oyBlob_GetSize(prop),
               "\"XFree86_DDC_EDID1_RAWDATA\"/\"EDID_DATA\"",
               _("Cant read hardware information from device."))
      error = -1;
    } else
    {
      /* convert to an deployable struct */
      edi = oyBlob_GetPointer(prop);

      error = oyUnrollEdid1_( edi, manufacturer, mnft, model, serial, vendor,
                      week, year, mnft_id, model_id, colours, allocate_func);

      if(edid && error != XCM_EDID_OK)
        oyBlob_Release( &prop );
    }
  }

  if( !prop )
  /* as a last means try Xorg.log for at least some informations */
  {
    char * log_file = 0;
    char * log_text = 0,
         * t;
    size_t log_size = 0;
    int screen = oyX1Monitor_screen_( disp ), i;

    {
#define X_LOG_PATH  "/var/log/"
      char num[12];
      sprintf( num, "%d", oyX1Monitor_number_(disp) );
      STRING_ADD( log_file, X_LOG_PATH "Xorg." );
      STRING_ADD( log_file, num );
      STRING_ADD( log_file, ".log" );
    }

    if(log_file)
    {
      log_text = oyReadFileToMem_( log_file, &log_size, oyAllocateFunc_);
      t = log_text;
    }

    if(log_text)
    {
      float rx=0,ry=0,gx=0,gy=0,bx=0,by=0,wx=0,wy=0,g=0;
      int year_ = 0, week_ = 0;
      const char * t;
      char mnft_[80] = {0};
      unsigned int model_id_ = 0;

      char * save_locale = 0;
      /* sensible parsing */
      save_locale = oyStringCopy_( setlocale( LC_NUMERIC, 0 ),
                                         oyAllocateFunc_ );
      setlocale( LC_NUMERIC, "C" );

      t = strstr( log_text, "Connected Display" );
      if(!t) t = log_text;
      t = strstr( t, port );
      if(!t)
      {
        t = log_text;
        if(t)
        for(i = 0; i < screen; ++i)
        {
          ++t;
          t = strstr( t, "redX:" );
        }
      }

      if(t && (t = strstr( t, "Manufacturer:" )) != 0)
      {
        sscanf( t,"Manufacturer: %s", mnft_ );
      }

      if(t && (t = strstr( t, "Model:" )) != 0)
      {
        sscanf( t,"Model: %x ", &model_id_ );
      }

      if(t && (t = strstr( t, "Year:" )) != 0)
      {
        sscanf( t,"Year: %d  Week: %d", &year_, &week_ );
      } 

      if(t && (t = strstr( t, "Gamma:" )) != 0)
        sscanf( t,"Gamma: %g", &g );
      if(t && (t = strstr( t, "redX:" )) != 0)
        sscanf( t,"redX: %g redY: %g", &rx,&ry );
      if(t && (t = strstr( t, "greenX:" )) != 0)
        sscanf( t,"greenX: %g greenY: %g", &gx,&gy );
      if(t && (t = strstr( t, "blueX:" )) != 0)
        sscanf( t,"blueX: %g blueY: %g", &bx,&by );
      if(t && (t = strstr( t, "whiteX:" )) != 0)
        sscanf( t,"whiteX: %g whiteY: %g", &wx,&wy );

      if(mnft_[0])
      {
        *mnft = oyStringCopy_( mnft_, oyAllocateFunc_ );
        *model_id = model_id_;
        colours[0] = rx;
        colours[1] = ry;
        colours[2] = gx;
        colours[3] = gy;
        colours[4] = bx;
        colours[5] = by;
        colours[6] = wx;
        colours[7] = wy;
        colours[8] = g;
        *year = year_;
        *week = week_;
        WARNcc5_S( user_data, "found %s in \"%s\": %s %d %s",
                   log_file, display_name, mnft_, model_id_,
                   display_geometry?oyNoEmptyString_m_(*display_geometry):geo);

        setlocale(LC_NUMERIC, save_locale);
        if(save_locale)
          oyFree_m_( save_locale );
      }
    }
  }

  if(edid)
  {
    *edid = prop;
    prop = 0;
  }

  oyX1Monitor_release_( &disp );
  if(geo) oyFree_m_(geo);

  if(prop || (edid && *edid))
  {
    oyBlob_Release( &prop );
    DBG_PROG_ENDE
    return 0;
  } else {
    const char * log = _("Cant read hardware information from device.");
    int r = -1;

    if(*mnft && (*mnft)[0])
    {
      log = "using Xorg log fallback.";
      r = 0;
    }

    WARNcc3_S( user_data, "\n  %s:\n  %s\n  %s",
               _("no EDID available from X properties"),
               "\"XFree86_DDC_EDID1_RAWDATA\"/\"EDID_DATA\"",
               oyNoEmptyString_m_(log))
    DBG_PROG_ENDE
    return r;
  }
}



char *       oyX1GetMonitorProfile   ( const char        * device_name,
                                       uint32_t            flags,
                                       size_t            * size,
                                       oyAlloc_f           allocate_func )
{
  char       *moni_profile=0;
  int error = 0;


  oyX1Monitor_s * disp = 0;
  oyBlob_s * prop = 0;

  DBG_PROG_START

  if(device_name)
    DBG_PROG1_S("device_name %s",device_name);

  disp = oyX1Monitor_newFrom_( device_name, 0 );
  if(!disp)
    return 0;

  /* support the colour server device profile */
  if(flags & 0x01)
    prop = oyX1Monitor_getProperty_( disp,
                             OY_ICC_COLOUR_SERVER_TARGET_PROFILE_IN_X_BASE, 0 );

  /* alternatively fall back to the non colour server or pre v0.4 atom */
  if(!prop)
    prop = oyX1Monitor_getProperty_( disp, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE, 0 );

  if(prop)
  {
    oyAllocHelper_m_( moni_profile, char, oyBlob_GetSize(prop), allocate_func, error = 1 )
    if(!error)
      error = !memcpy( moni_profile, oyBlob_GetPointer(prop),
                       oyBlob_GetSize(prop) );
    if(!error)
      *size = oyBlob_GetSize(prop);
    oyBlob_Release( &prop );
  } /*else
    WARNc1_S("\n  %s",
         _("Could not get Xatom, probably your monitor profile is not set:"));*/

  oyX1Monitor_release_( &disp );

  DBG_PROG_ENDE
  if(!error)
    return moni_profile;
  else
    return NULL;
}


int      oyX1GetAllScreenNames       ( const char        * display_name,
                                       char            *** display_names,
                                       oyAlloc_f           allocateFunc )
{
  int i = 0;
  char** list = 0;

  list = oyX1GetAllScreenNames_( display_name, &i );

  *display_names = 0;

  if(list && i)
  {
    *display_names = oyStringListAppend_( 0, 0, (const char**)list, i, &i,
                                          allocateFunc );
    oyStringListRelease_( &list, i, oyDeAllocateFunc_ );
  }

  return i; 
}

char**
oyX1GetAllScreenNames_          (const char *display_name,
                                 int *n_scr)
{
  int i = 0;
  char** list = 0;

  Display *display = 0;
  int len = 0;
  oyX1Monitor_s * disp = 0;

  *n_scr = 0;

  if(!display_name || !display_name[0])
    return 0;

  disp = oyX1Monitor_newFrom_( display_name, 0 );
  if(!disp)
    return 0;

  display = oyX1Monitor_device_( disp );

  if( !display || (len = ScreenCount( display )) == 0 )
    return 0;

# if HAVE_XRANDR
  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR)
    len = disp->active_outputs;
# endif

# if HAVE_XIN
  /* test for Xinerama screens */
  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XINERAMA)
    {
      int n_scr_info = 0;
      XineramaScreenInfo *scr_info = XineramaQueryScreens( display, &n_scr_info );
      oyPostAllocHelper_m_(scr_info, n_scr_info, return 0 )

      if( n_scr_info >= 1 )
        len = n_scr_info;

      XFree( scr_info );
    }
# endif

  oyAllocHelper_m_( list, char*, len, 0, return NULL )

  for (i = 0; i < len; ++i)
    if( (list[i] = oyChangeScreenName_( display_name, i )) == 0 )
      return NULL;

  *n_scr = len;
  oyX1Monitor_release_( &disp );


  return list;
}


/** @internal
 *  Function oyX1Rectangle_FromDevice
 *  @memberof monitor_api
 *  @brief   value filled a oyStruct_s object
 *
 *  @param         device_name         the Oyranos specific device name
 *  @return                            the rectangle the device projects to
 *
 *  @version Oyranos: 0.1.10
 *  @since   2009/01/28 (Oyranos: 0.1.10)
 *  @date    2009/01/28
 */
oyRectangle_s* oyX1Rectangle_FromDevice ( const char        * device_name )
{
  oyRectangle_s * rectangle = 0;
  int error = !device_name;

  if(!error)
  {
    oyX1Monitor_s * disp = 0;

    disp = oyX1Monitor_newFrom_( device_name, 0 );
    if(!disp)
      return 0;

    rectangle = oyRectangle_NewWith( oyX1Monitor_x_(disp), oyX1Monitor_y_(disp),
                           oyX1Monitor_width_(disp), oyX1Monitor_height_(disp), 0 );

    oyX1Monitor_release_( &disp );
  }

  return rectangle;
}


/** @internal
    takes a chaked display name and point as argument and
    gives a string back for search in the db
 */
int
oyX1Monitor_getGeometryIdentifier_         (oyX1Monitor_s  *disp)
{
  int len = 64;

  oyAllocHelper_m_( disp->identifier, char, len, 0, return 1 )

  oySnprintf4_( disp->identifier, len, "%dx%d+%d+%d", 
            oyX1Monitor_width_(disp), oyX1Monitor_height_(disp),
            oyX1Monitor_x_(disp), oyX1Monitor_y_(disp) );

  return 0;
}

char* oyX1Monitor_getAtomName_         ( oyX1Monitor_s       * disp,
                                       const char        * base )
{
  int len = 64;
  char *atom_name = 0;
  char *screen_number = oyX1Monitor_screenIdentifier_( disp );

  oyPostAllocHelper_m_( screen_number, 1, return 0 )
  oyAllocHelper_m_( atom_name, char, len, 0, return 0 )

  oySnprintf2_( atom_name, len, "%s%s", base, screen_number );

  oyFree_m_( screen_number );

  return atom_name;
}


int      oyX1MonitorProfileSetup     ( const char        * display_name,
                                       const char        * profil_name )
{
  int error = 0;
  const char * profile_fullname = 0;
  const char * profil_basename = 0;
  char* profile_name_ = 0;
  oyProfile_s * prof = 0;
  oyX1Monitor_s * disp = 0;
  char       *dpy_name = NULL;
  char *text = 0;

  DBG_PROG_START
  disp = oyX1Monitor_newFrom_( display_name, 0 );
  if(!disp)
    return 1;

  dpy_name = calloc( sizeof(char), MAX_PATH );
  if( display_name && !strstr( disp->host, display_name ) )
    oySnprintf1_( dpy_name, MAX_PATH, ":%d", disp->geo[0] );
  else
    oySnprintf2_( dpy_name, MAX_PATH, "%s:%d", disp->host, disp->geo[0] );

  if(profil_name)
  {
    DBG_PROG1_S( "profil_name = %s", profil_name );
    prof = oyProfile_FromFile( profil_name, 0, 0 );
    profile_fullname = oyProfile_GetFileName( prof, -1 );
  }

  if( profile_fullname && profile_fullname[0] )
  {

    if(profil_name && strrchr(profil_name,OY_SLASH_C))
      profil_basename = strrchr(profil_name,OY_SLASH_C)+1;
    else
    {
      if(profil_name)
        profil_basename = profil_name;
      else if( profile_fullname && strrchr(profile_fullname,OY_SLASH_C))
        profil_basename = strrchr( profile_fullname, OY_SLASH_C)+1;
    }


    oyAllocHelper_m_( text, char, MAX_PATH, 0, error = 1; goto Clean )
    DBG_PROG1_S( "profile_fullname %s", profile_fullname );

    /** set vcgt tag with xcalib
       not useable with multihead Xinerama at one screen

       @todo TODO xcalib should be configurable as a module
     */
    sprintf(text,"xcalib -d %s -s %d %s \'%s\'", dpy_name, disp->geo[1],
                 oy_debug?"-v":"", profile_fullname);
    {
      Display * display = oyX1Monitor_device_( disp );
      int effective_screen = oyX1Monitor_screen_( disp );
      int screen = oyX1Monitor_deviceScreen_( disp );
      int size = 0,
          can_gamma = 0;

      if(!display)
      {
        WARNc3_S("%s %s %s", _("open X Display failed"), dpy_name, display_name)
        return 1;
      }

#ifdef HAVE_XF86VMODE
      if(effective_screen == screen)
      {
        XF86VidModeGamma gamma;
        /* check for gamma capabiliteis on the given screen */
        if (XF86VidModeGetGamma(display, effective_screen, &gamma))
          can_gamma = 1;
        else
        if (XF86VidModeGetGammaRampSize(display, effective_screen, &size))
        {
          if(size)
            can_gamma = 1;
        }
      }
#endif

      /* Check for incapabilities of X gamma table access */
      if(can_gamma || oyX1Monitor_screen_( disp ) == 0)
        error = system(text);
      if(error &&
         error != 65280)
      { /* hack */
        WARNc2_S("%s %s", _("No monitor gamma curves by profile:"),
                oyNoEmptyName_m_(profil_basename) )
      } else
        /* take xcalib error not serious, turn into a issue */
        error = -1;
    }

    if(oy_debug)
      DBG1_S( "system: %s", text )

    /* set OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE atom in X */
    {
      Display *display;
      Atom atom = 0;
      int screen = 0;
      Window w;

      unsigned char *moni_profile=0;
      size_t      size=0;
      char       *atom_name=0;
      int         result = 0;

      if(display_name)
        DBG_PROG1_S("display_name %s",display_name);

      display = oyX1Monitor_device_( disp );

      screen = oyX1Monitor_deviceScreen_( disp );
      DBG_PROG_V((screen))
      w = RootWindow(display, screen); DBG_PROG1_S("w: %ld", w)

      if(profile_fullname)
        moni_profile = oyGetProfileBlock( profile_fullname, &size, oyAllocateFunc_ );
      else if(profil_name)
        moni_profile = oyGetProfileBlock( profil_name, &size, oyAllocateFunc_ );

      if(!size || !moni_profile)
        WARNc_S(_("Error obtaining profile"));

      atom_name = oyX1Monitor_getAtomName_( disp, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE );
      if( atom_name )
      {
        atom = XInternAtom (display, atom_name, False);
        if (atom == None) {
          WARNc2_S("%s \"%s\"", _("Error setting up atom"), atom_name);
        }
      } else WARNc_S(_("Error setting up atom"));

      if( atom && moni_profile)
      result = XChangeProperty( display, w, atom, XA_CARDINAL,
                       8, PropModeReplace, moni_profile, (int)size );

      /* claim to be compatible with 0.4 
       * http://www.freedesktop.org/wiki/OpenIcc/ICC_Profiles_in_X_Specification_0.4
       */
      atom = XInternAtom( display, "_ICC_PROFILE_IN_X_VERSION", False );
      if(atom)
      {
        Atom a;
        /* 0.4 == 100*0 + 1*4 = 4 */
        const unsigned char * value = (const unsigned char*)"4";
        int actual_format_return;
        unsigned long nitems_return=0, bytes_after_return=0;
        unsigned char* prop_return=0;

        XGetWindowProperty( display, w, atom, 0, INT_MAX, False, XA_STRING,
                     &a, &actual_format_return, &nitems_return, 
                     &bytes_after_return, &prop_return );
        /* check if the old value is the same as our intented */
        if(actual_format_return != XA_STRING ||
           nitems_return == 0)
        {
          if(!prop_return || strcmp( (char*)prop_return, (char*)value ) != 0)
          result = XChangeProperty( display, w, atom, XA_STRING,
                                    8, PropModeReplace,
                                    value, 4 );
        }
      }

      if(moni_profile)
        oyFree_m_( moni_profile )
      oyFree_m_( atom_name )
    }

    oyFree_m_( text );
  }

  Clean:
  oyX1Monitor_release_( &disp );
  oyProfile_Release( &prof );
  if(profile_name_) oyFree_m_( profile_name_ );
  if(dpy_name) oyFree_m_( dpy_name );

  DBG_PROG_ENDE
  return error;
}


int      oyX1MonitorProfileUnset     ( const char        * display_name )
{
  int error = 0;


  oyX1Monitor_s * disp = 0;
  oyProfile_s * prof = 0;

  DBG_PROG_START

  disp = oyX1Monitor_newFrom_( display_name, 0 );
  if(!disp)
  {
    DBG_PROG_ENDE
    return 1;
  }


  {
    /* unset the OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE atom in X */
      Display *display;
      Atom atom;
      int screen = 0;
      Window w;
      char *atom_name = 0;

      if(display_name)
        DBG_PROG1_S("display_name %s",display_name);

      display = oyX1Monitor_device_( disp );

      screen = oyX1Monitor_deviceScreen_( disp );
      DBG_PROG_V((screen))
      w = RootWindow(display, screen); DBG_PROG1_S("w: %ld", w)

      DBG_PROG

      atom_name = oyX1Monitor_getAtomName_( disp, OY_ICC_V0_3_TARGET_PROFILE_IN_X_BASE );
      atom = XInternAtom (display, atom_name, True);
      if (atom != None)
        XDeleteProperty( display, w, atom );
      else
      {
        WARNc2_S("%s \"%s\"", _("Error getting atom"), atom_name);
      }

      {
        char *dpy_name = oyStringCopy_( oyNoEmptyString_m_(display_name), oyAllocateFunc_ );
        char * command = 0;
        char *ptr = NULL;
        int r;

        oyAllocHelper_m_( command, char, 1048, 0 , goto finish );

        if( (ptr = strchr(dpy_name,':')) != 0 )
          if( (ptr = strchr(ptr,'.')) != 0 )
            ptr[0] = '\000';

        oySnprintf2_(command, 1024, "xgamma -gamma 1.0 -screen %d -display %s",
                 disp->geo[1], dpy_name);

        if(screen == disp->geo[1])
          r = system( command );

        oyFree_m_( command )
      }

      oyFree_m_( atom_name )
      DBG_PROG
    goto finish;
  }


  finish:
  oyProfile_Release( &prof );
  oyX1Monitor_release_( &disp );

  DBG_PROG_ENDE
  return error;
}

int
oyGetDisplayNumber_        (oyX1Monitor_s *disp)
{
  int dpy_nummer = 0;
  const char *display_name = oyX1Monitor_name_(disp);

  DBG_PROG_START

  if( display_name )
  {
    char ds[8];             /* display.screen*/
    const char *txt = strchr( display_name, ':' );
    
    if( !txt )
    { WARNc1_S( "invalid display name: %s", display_name )
      return -1;
    }

    ++txt;
    strncpy( ds, txt, 8 );
    if( strrchr( ds, '.' ) )
    {
      char *end = strchr( ds, '.' );
      if( end )
        *end = 0;
    }
    dpy_nummer = atoi( ds );
  }

  DBG_PROG_ENDE
  return dpy_nummer;
}

int   oyX1Monitor_getScreenFromDisplayName_( oyX1Monitor_s   * disp )
{
  int scr_nummer = 0;
  const char *display_name = oyX1Monitor_name_(disp);

  DBG_PROG_START

  if( display_name )
  {
    char ds[8];             /* display.screen*/
    const char *txt = strchr( display_name, ':' );
    
    if( !txt )
    { WARNc1_S( "invalid display name: %s", display_name )
      return -1;
    }

    strncpy( ds, txt, 8 );
    if( strrchr( display_name, '.' ) )
    {
      char *nummer_text = strchr( ds, '.' );
      if( nummer_text )
        scr_nummer = atoi( &nummer_text[1] );
    }
  }

  DBG_PROG_ENDE
  return scr_nummer;
}


/**  @internal
 *  extract the host name or get from environment
 */
char*
oyExtractHostName_           (const char* display_name)
{
  char* host_name = 0;

  DBG_PROG_START

  oyAllocHelper_m_( host_name, char, strlen( display_name ) + 48,0,return NULL);

  /* Is this X server identifyable? */
  if(!display_name)
  {
    char *host = getenv ("HOSTNAME");
    if (host) {
        strcpy( host_name, host );
    }
  } else if (strchr(display_name,':') == display_name ||
             !strchr( display_name, ':' ) )
  {
    char *host = getenv ("HOSTNAME");
    /* good */
    if (host) {
        strcpy( host_name, host );
    }
  } else if ( strchr( display_name, ':' ) )
  {
    char* ptr = 0;
    strcpy( host_name, display_name );
    ptr = strchr( host_name, ':' );
    ptr[0] = 0;
  }

  DBG_PROG1_S( "host_name = %s", host_name )

  DBG_PROG_ENDE
  return host_name;
}

/** @internal Do a full check and change the screen name,
 *  if the screen arg is appropriate. Dont care about the host part
 */
char*
oyChangeScreenName_                (const char* display_name,
                                    int         screen)
{
  char* host_name = 0;

  DBG_PROG_START

  /* Is this X server identifyable? */
  if(!display_name)
    display_name = ":0.0";


  oyAllocHelper_m_( host_name, char, strlen( display_name ) + 48,0,return NULL);

  strcpy( host_name, display_name );

  /* add screen */
  {
    const char *txt = strchr( host_name, ':' );

    /* fail if no display was given */
    if( !txt )
    { WARNc1_S( "invalid display name: %s", display_name )
      host_name[0] = 0;
      return host_name;
    }

    if( !strchr( txt, '.' ) )
    {
      sprintf( &host_name[ strlen(host_name) ], ".%d", screen );
    } else
    if( screen >= 0 )
    {
      char *txt_scr = strchr( txt, '.' );
      sprintf( txt_scr, ".%d", screen );
    }
  }

  DBG_PROG1_S( "host_name = %s", host_name )

  DBG_PROG_ENDE
  return host_name;
}

/** @internal get the geometry of a screen 
 *
 *  @param          disp      display info structure
 *  @return         error
 */
int
oyX1Monitor_getScreenGeometry_            (oyX1Monitor_s *disp)
{
  int error = 0;
  int screen = 0;


  disp->geo[0] = oyGetDisplayNumber_( disp );
  disp->geo[1] = screen = oyX1Monitor_getScreenFromDisplayName_( disp );

  if(screen < 0)
    return screen;

# if HAVE_XRANDR
  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR )
  {
    XRRCrtcInfo * crtc_info = 0;

    crtc_info = XRRGetCrtcInfo( disp->display, disp->res,
                                disp->output_info->crtc );
    if(crtc_info)
    {
      disp->geo[2] = crtc_info->x;
      disp->geo[3] = crtc_info->y;
      disp->geo[4] = crtc_info->width;
      disp->geo[5] = crtc_info->height;

      XRRFreeCrtcInfo( crtc_info );
    } else
    {
      WARNc3_S( "%s output: \"%s\" crtc: %d",
               _("XRandR CrtcInfo request failed"), 
               oyNoEmptyString_m_(disp->output_info->name),
               disp->output_info->crtc)
    }
  }
# endif /* HAVE_XRANDR */

# if HAVE_XIN
  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XINERAMA )
  {
    int n_scr_info = 0;

    XineramaScreenInfo *scr_info = XineramaQueryScreens( disp->display,
                                                         &n_scr_info );
    oyPostAllocHelper_m_(scr_info, n_scr_info, return 1 )

    if( !scr_info )
    {
      WARNc_S(_("Xinerama request failed"))
      return 1;
    }
    {
        disp->geo[2] = scr_info[screen].x_org;
        disp->geo[3] = scr_info[screen].y_org;
        disp->geo[4] = scr_info[screen].width;
        disp->geo[5] = scr_info[screen].height;
    }
    XFree( scr_info );
  }
# endif /* HAVE_XIN */

  if( oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_SCREEN )
  {
    Screen *scr = XScreenOfDisplay( disp->display, screen );
    oyPostAllocHelper_m_(scr, 1, WARNc_S(_("open X Screen failed")); return 1;)
    {
        disp->geo[2] = 0;
        disp->geo[3] = 0;
        disp->geo[4] = XWidthOfScreen( scr );
        disp->geo[5] = XHeightOfScreen( scr );
        disp->screen = screen;
    }
  }

  return error;
}

/** @internal
 *  @brief create a monitor information struct for a given display name
 *
 *  @param   display_name              Oyranos display name
 *  @param   expensive                 probe XRandR even if it causes flickering
 *  @return                            a monitor information structure
 *
 *  @version Oyranos: 0.1.10
 *  @since   2007/12/17 (Oyranos: 0.1.8)
 *  @date    2009/01/13
 */
oyX1Monitor_s* oyX1Monitor_newFrom_      ( const char        * display_name,
                                       int                 expensive )
{
  int error = 0;
  int i = 0;
  oyX1Monitor_s * disp = 0;

  DBG_PROG_START

  disp = oyAllocateFunc_( sizeof(oyX1Monitor_s) );
  error = !disp;
  if(!error)
    error = !memset( disp, 0, sizeof(oyX1Monitor_s) );

  disp->type_ = oyOBJECT_MONITOR_S;

  if( display_name )
  {
    if( display_name[0] )
      disp->name = oyStringCopy_( display_name, oyAllocateFunc_ );
  } else
  {
    if(getenv("DISPLAY") && strlen(getenv("DISPLAY")))
      disp->name = oyStringCopy_( getenv("DISPLAY"), oyAllocateFunc_ );
    else
      disp->name = oyStringCopy_( ":0", oyAllocateFunc_ );
  }

  if( !error &&
      (disp->host = oyExtractHostName_( disp->name )) == 0 )
    error = 1;

  {
  Display *display = 0;
  int len = 0;
  int monitors = 0;

  disp->display = XOpenDisplay (disp->name);

  /* switch to Xinerama mode */
  if( !disp->display ) {
    char *text = oyChangeScreenName_( disp->name, 0 );
    oyPostAllocHelper_m_( text, 1,
                          WARNc_S(_("allocation failed"));return 0 )

    disp->display = XOpenDisplay( text );
    oyFree_m_( text );

    if( !disp->display )
      oyPostAllocHelper_m_( disp->display, 1,
                            WARNc_S(_("open X Display failed")); return 0 )

    disp->screen = 0;
  }
  display = oyX1Monitor_device_( disp );

  if( !display || (len = ScreenCount( display )) <= 0 )
  {
    WARNc2_S("%s: \"%s\"", _("no Screen found"),
                           oyNoEmptyString_m_(disp->name));
    return 0;
  }

  disp->info_source = oyX11INFO_SOURCE_SCREEN;

  if(len == 1)
  {
# if HAVE_XRANDR
    int major_versionp = 0;
    int minor_versionp = 0;
    int i, n = 0;
    XRRQueryVersion( display, &major_versionp, &minor_versionp );

    if((major_versionp*100 + minor_versionp) >= 102)
    {
      /* too expensive
      Time xrr_config_time0 = 0,
           xrr_config_time = XRRTimes( display, oyX1Monitor_screen_(disp),
                                       &xrr_config_time0 );

      // expensive too: XRRConfigTimes()
      */
    }

    if((major_versionp*100 + minor_versionp) >= 102 && expensive)
    {
      Window w = RootWindow(display, oyX1Monitor_screen_(disp));
      XRRScreenResources * res = 0;
      int selected_screen = oyX1Monitor_getScreenFromDisplayName_( disp );
      int xrand_screen = -1;
      int geo[4] = {-1,-1,-1,-1};
      int geo_monitors = 0;

# if HAVE_XIN
      /* sync numbering with Xinerama screens */
      if( XineramaIsActive( display ) )
      {
        int n_scr_info = 0;
        XineramaScreenInfo *scr_info = XineramaQueryScreens( display,
                                                             &n_scr_info );
        geo[0] = scr_info[selected_screen].x_org;
        geo[1] = scr_info[selected_screen].y_org;
        geo[2] = scr_info[selected_screen].width;
        geo[3] = scr_info[selected_screen].height;
        oyPostAllocHelper_m_(scr_info, n_scr_info, return 0 )

        XFree( scr_info );

      }
# endif /* HAVE_XIN */


      /* a havily expensive call */
      DBG_NUM_S("going to call XRRGetScreenResources()");
      res = XRRGetScreenResources(display, w);
      DBG_NUM_S("end of call XRRGetScreenResources()");
      if(res)
        n = res->noutput;
      for(i = 0; i < n; ++i)
      {
        XRRScreenResources * res_temp = res ? res : disp->res;
        XRROutputInfo * output_info = XRRGetOutputInfo( display, res_temp,
                                                        res_temp->outputs[i]);
 
        /* we work on connected outputs */
        if( output_info && output_info->crtc )
        {
          XRRCrtcGamma * gamma = 0;
          XRRCrtcInfo * crtc_info = 0;

          if(monitors == 0)
          {
            gamma = XRRGetCrtcGamma( display, output_info->crtc );
            if(gamma && gamma->size &&
               strcmp("default", output_info->name) != 0)
            {
              disp->info_source = oyX11INFO_SOURCE_XRANDR;

              XRRFreeGamma( gamma );
            } else
            {
              if(gamma)
                XRRFreeGamma( gamma );
              XRRFreeOutputInfo( output_info );
              break;
            }
          }

          crtc_info = XRRGetCrtcInfo( disp->display, res_temp,
                                      output_info->crtc );
          if(crtc_info)
          {
            /* compare with Xinerama geometry */
            if(
               geo[0] == crtc_info->x &&
               geo[1] == crtc_info->y &&
               geo[2] == crtc_info->width &&
               geo[3] == crtc_info->height )
            {
              xrand_screen = monitors;
              ++geo_monitors;
            }

            XRRFreeCrtcInfo( crtc_info );
          }

          if(xrand_screen == monitors &&
             oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR &&
             (geo_monitors == 1 || geo_monitors-1 == selected_screen))
          {
            if(output_info)
              disp->output_info = output_info;
            disp->output = res_temp->outputs[i];
            output_info = 0;
            if(res) /* only needed for a second geo matching monitor */
              disp->res = res;
            res = 0;
            if(disp->output_info->name && oyStrlen_(disp->output_info->name))
              disp->system_port = oyStringCopy_( disp->output_info->name,
                                                 oyAllocateFunc_ );
          }

          ++ monitors;
        }

        if(output_info)
          XRRFreeOutputInfo( output_info );
      }

      if(res)
        XRRFreeScreenResources(res); res = 0;

      if(oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_XRANDR)
      {
        if(monitors >= len && disp->output_info)
          disp->active_outputs = monitors;
        else
          disp->info_source = oyX11INFO_SOURCE_SCREEN;
      }
    }
# endif /* HAVE_XRANDR */

    if(oyX1Monitor_infoSource_( disp ) == oyX11INFO_SOURCE_SCREEN)
    {
# if HAVE_XIN
      /* test for Xinerama screens */
      if( XineramaIsActive( display ) )
      {
        int n_scr_info = 0;
        XineramaScreenInfo *scr_info = XineramaQueryScreens( display,
                                                             &n_scr_info );
        oyPostAllocHelper_m_(scr_info, n_scr_info, return 0 )

        if( n_scr_info >= 1 )
          len = n_scr_info;

        if(n_scr_info < monitors)
        {
          WARNc3_S( "%s: %d < %d", _("less Xinerama monitors than XRandR ones"),
                    n_scr_info, monitors );
        } else
          disp->info_source = oyX11INFO_SOURCE_XINERAMA;

        XFree( scr_info );
      }
# endif /* HAVE_XIN */
    }
  }
  }

  for( i = 0; i < 6; ++i )
    disp->geo[i] = -1;

  if( !error &&
      oyX1Monitor_getScreenGeometry_( disp ) != 0 )
    error = 1;

  if( error <= 0 )
    error = oyX1Monitor_getGeometryIdentifier_( disp );

  if( !disp->system_port || !oyStrlen_( disp->system_port ) )
  if( 0 <= oyX1Monitor_screen_( disp ) && oyX1Monitor_screen_( disp ) < 10000 )
  {
    disp->system_port = (char*)oyAllocateWrapFunc_( 12, oyAllocateFunc_ );
    oySprintf_( disp->system_port, "%d", oyX1Monitor_screen_( disp ) );
  }

  DBG_PROG_ENDE
  return disp;
}


/** @internal
 *  @brief release a monitor information stuct
 *
 *  @version Oyranos: 0.1.10
 *  @since   2007/12/17 (Oyranos: 0.1.8)
 *  @date    2009/04/01
 */
int          oyX1Monitor_release_      ( oyX1Monitor_s      ** obj )
{
  int error = 0;
  /* ---- start of common object destructor ----- */
  oyX1Monitor_s * s = 0;
  
  if(!obj || !*obj)
    return 0;
  
  s = *obj;
  
  if( s->type_ != oyOBJECT_MONITOR_S)
  { 
    WARNc_S("Attempt to release a non oyX1Monitor_s object.")
    return 1;
  }
  /* ---- end of common object destructor ------- */

  if(s->name) oyDeAllocateFunc_( s->name );
  if(s->host) oyDeAllocateFunc_( s->host );
  if(s->identifier) oyDeAllocateFunc_( s->identifier );


  s->geo[0] = s->geo[1] = -1;

  if( s->display )
  {
#  ifdef HAVE_XRANDR
    if(s->output_info)
      XRRFreeOutputInfo( s->output_info ); s->output_info = 0;
    if(s->res)
      XRRFreeScreenResources( s->res ); s->res = 0;
#  endif
    XCloseDisplay( s->display );
    s->display=0;
  }

  oyDeAllocateFunc_( s );
  s = 0;

  *obj = 0;

  return error; 
}


/* separate from the internal functions */

/** @brief pick up monitor information with Xlib
 *  @deprecated because sometimes is no ddc information available
 *  @todo include connection information - grafic cart
 *
 *  @param      display_name  the display string
 *  @param[out] manufacturer  the manufacturer of the monitor device
 *  @param[out] model         the model of the monitor device
 *  @param[out] serial        the serial number of the monitor device
 *  @param      allocate_func the allocator for the above strings
 *  @return     error
 *
 */
int
oyX1GetMonitorInfo_lib            (const char* display_name,
                                   char**      manufacturer,
                                       char             ** mnft,
                                   char**      model,
                                   char**      serial,
                                       char             ** vendor,
                                       char             ** display_geometry,
                                       char             ** system_port,
                                       char             ** host,
                                       uint32_t          * week,
                                       uint32_t          * year,
                                       uint32_t          * mnft_id,
                                       uint32_t          * model_id,
                                       double            * colours,
                                       oyBlob_s         ** edid,
                                   oyAlloc_f     allocate_func,
                                       oyStruct_s        * user_data)
{
  int err = 0;

  DBG_PROG_START

  err = oyX1GetMonitorInfo_(display_name, manufacturer, mnft, model, serial,
                           vendor,
                           display_geometry, system_port, host, week, year,
                           mnft_id, model_id, colours, edid,
                           allocate_func, user_data );

  if(*manufacturer)
    DBG_PROG_S( *manufacturer );
  if(*model)
    DBG_PROG_S( *model );
  if(*serial)
    DBG_PROG_S( *serial );

  DBG_PROG_ENDE
  return err;
}




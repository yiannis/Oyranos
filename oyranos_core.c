/** @file oyranos_core.c
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  @par Copyright:
 *            2004-2011 (C) Kai-Uwe Behrmann
 *
 *  @brief    public Oyranos API's
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2004/11/25
 */


#include "oyranos_core.h" /* define HAVE_POSIX */

#include <sys/stat.h>
#ifdef HAVE_POSIX
#include <unistd.h>
#include <langinfo.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "oyranos_debug.h"
#include "oyranos_helper.h"
#include "oyranos_internal.h"
#include "oyranos_icc.h"
#include "oyranos_io.h"
#include "oyranos_sentinel.h"
#include "oyranos_string.h"
#include "oyranos_texts.h"

#include "oyStruct_s.h"
#include "oyObject_s_.h"
#include "oyName_s_.h"

static oyStruct_RegisterStaticMessageFunc_f * oy_static_msg_funcs_ = 0;
static int oy_msg_func_n_ = 0;

/** @func    oyStruct_RegisterStaticMessageFunc
 *  @brief   register a function for verbosity
 *
 *  @param[in]     type                the object oyOBJECT_e type 
 *  @param[in]     f                   the object string function
 *  @return                            0 - success; >= 1 - error
 *
 *  @version Oyranos: 0.2.1
 *  @since   2011/01/14
 *  @date    2011/01/14
 */
int oyStruct_RegisterStaticMessageFunc (
                                       int                 type,
                                       oyStruct_RegisterStaticMessageFunc_f f )
{
  int error = 0;
  if((int)type >= oy_msg_func_n_)
  {
    int n = oy_msg_func_n_;
    oyStruct_RegisterStaticMessageFunc_f * tmp = 0;

    if(oy_msg_func_n_)
      n *= 2;
    else
      n = (int) oyOBJECT_MAX;


    tmp = oyAllocateFunc_(sizeof(oyStruct_RegisterStaticMessageFunc_f) * n);
    if(tmp && oy_msg_func_n_)
      memcpy( tmp, oy_static_msg_funcs_, sizeof(oyStruct_RegisterStaticMessageFunc_f) * oy_msg_func_n_ );
    else if(!tmp)
    {
      error = 1;
      return error;
    }

    oyDeAllocateFunc_(oy_static_msg_funcs_);
    oy_static_msg_funcs_ = tmp;
    tmp = 0;
    oy_static_msg_funcs_[type] = f;
    oy_msg_func_n_ = n;
  }
  return error;
}
                                       
/** @func    oyStruct_GetInfo
 *  @brief   get a additional string from a object
 *
 *  The content can be provided by object authors by using
 *  oyStruct_RegisterStaticMessageFunc() typical at the first time of object
 *  creation.
 *
 *  @param[in]     context_object      the object to get informations about
 *  @param[in]     flags               currently not used
 *  @return                            a string or NULL; The pointer might
 *                                     become invalid after further using the
 *                                     object pointed to by context.
 *  @version Oyranos: 0.2.1
 *  @since   2011/01/15
 *  @date    2011/01/15
 */
const char *   oyStruct_GetInfo      ( oyPointer           context_object,
                                       int                 flags )
{
  const char * text = NULL;
  oyStruct_s * c = (oyStruct_s*) context_object;
  oyStruct_RegisterStaticMessageFunc_f f;

  if(!c)
    return NULL;

  if(oy_static_msg_funcs_ != NULL)
  {
    f = oy_static_msg_funcs_[c->type_];
    if(f)
      text = f( c, 0 );
  }

  if(text == NULL)
    text = oyStructTypeToText( c->type_ );

  return text;
}


/** Function oyStructTypeToText
 *  @brief   Objects type to small string
 *
 *  Give a basic description of inbuild object types.
 *
 *  @version Oyranos: 0.1.10
 *  @since   2008/06/24 (Oyranos: 0.1.8)
 *  @date    2008/12/28
 */
const char *     oyStructTypeToText  ( oyOBJECT_e          type )
{
  const char * text = "unknown";
  switch(type) {
    case oyOBJECT_NONE: text = "Zero - none"; break;
    case oyOBJECT_OBJECT_S: text = "oyObject_s"; break;
    case oyOBJECT_MONITOR_S: text = "oyMonitor_s"; break;
    case oyOBJECT_NAMED_COLOUR_S: text = "oyNamedColour_s"; break;
    case oyOBJECT_NAMED_COLOURS_S: text = "oyNamedColours_s"; break;
    case oyOBJECT_PROFILE_S: text = "oyProfile_s"; break;
    case oyOBJECT_PROFILE_TAG_S: text = "oyProfileTag_s"; break;
    case oyOBJECT_PROFILES_S: text = "oyProfiles_s"; break;
    case oyOBJECT_OPTION_S: text = "oyOption_s"; break;
    case oyOBJECT_OPTIONS_S: text = "oyOptions_s"; break;
    case oyOBJECT_RECTANGLE_S: text = "oyRectangle_s"; break;
    case oyOBJECT_IMAGE_S: text = "oyImage_s"; break;
    case oyOBJECT_ARRAY2D_S: text = "oyArray2d_s"; break;
    case oyOBJECT_COLOUR_CONVERSION_S: text = "oyColourConversion_s";break;
    case oyOBJECT_FILTER_CORE_S: text = "oyFilterCore_s"; break;
    case oyOBJECT_FILTER_CORES_S: text = "oyFilterCores_s"; break;
    case oyOBJECT_CONVERSION_S: text = "oyConversion_s"; break;
    case oyOBJECT_CONNECTOR_S: text = "oyConnector_s"; break;
    case oyOBJECT_CONNECTOR_IMAGING_S: text = "oyConnectorImaging_s"; break;
    case oyOBJECT_FILTER_PLUG_S: text = "oyFilterPlug_s"; break;
    case oyOBJECT_FILTER_PLUGS_S: text = "oyFilterPlugs_s"; break;
    case oyOBJECT_FILTER_SOCKET_S: text = "oyFilterSocket_s"; break;
    case oyOBJECT_FILTER_NODE_S: text = "oyFilterNode_s"; break;
    case oyOBJECT_FILTER_NODES_S: text = "oyFilterNodes_s"; break;
    case oyOBJECT_FILTER_GRAPH_S: text = "oyFilterGraph_s"; break;
    case oyOBJECT_PIXEL_ACCESS_S: text = "oyPixelAccess_s"; break;
    case oyOBJECT_CMM_HANDLE_S: text = "oyCMMhandle_s"; break;
    case oyOBJECT_POINTER_S: text = "oyPointer_s"; break;
    case oyOBJECT_CMM_INFO_S: text = "oyCMMInfo_s"; break;
    case oyOBJECT_CMM_API_S: text = "oyCMMapi_s generic"; break;
    case oyOBJECT_CMM_APIS_S: text = "oyCMMapis_s generic"; break;
    case oyOBJECT_CMM_API1_S: text = "oyCMMapi1_s old CMM"; break;
    case oyOBJECT_CMM_API2_S: text = "oyCMMapi2_s Monitors"; break;
    case oyOBJECT_CMM_API3_S: text = "oyCMMapi3_s Profile tags"; break;
    case oyOBJECT_CMM_API4_S: text = "oyCMMapi4_s Filter"; break;
    case oyOBJECT_CMM_API5_S: text = "oyCMMapi5_s MetaFilter"; break;
    case oyOBJECT_CMM_API6_S: text = "oyCMMapi6_s Context convertor"; break;
    case oyOBJECT_CMM_API7_S: text = "oyCMMapi7_s Filter run"; break;
    case oyOBJECT_CMM_API8_S: text = "oyCMMapi8_s Devices"; break;
    case oyOBJECT_CMM_API9_S: text = "oyCMMapi9_s Graph Policies"; break;
    case oyOBJECT_CMM_API10_S: text = "oyCMMapi10_s generic command"; break;
    case oyOBJECT_CMM_DATA_TYPES_S: text = "oyCMMDataTypes_s Filter"; break;
    case oyOBJECT_CMM_API_FILTERS_S: text="oyCMMapiFilters_s Filter list";break;
    case oyOBJECT_CMM_API_MAX: text = "not defined"; break;
    case oyOBJECT_ICON_S: text = "oyIcon_s"; break;
    case oyOBJECT_MODULE_S: text = "oyModule_s"; break;
    case oyOBJECT_EXTERNFUNC_S: text = "oyExternFunc_s"; break;
    case oyOBJECT_NAME_S: text = "oyName_s"; break;
    case oyOBJECT_COMP_S_: text = "oyComp_s_"; break;
    case oyOBJECT_FILE_LIST_S_: text = "oyFileList_s_"; break;
    case oyOBJECT_HASH_S: text = "oyHash_s"; break;
    case oyOBJECT_STRUCT_LIST_S: text = "oyStructList_s"; break;
    case oyOBJECT_BLOB_S: text = "oyBlob_s"; break;
    case oyOBJECT_CONFIG_S: text = "oyConfig_s"; break;
    case oyOBJECT_CONFIGS_S: text = "oyConfigs_s"; break;
    case oyOBJECT_UI_HANDLER_S: text = "oyUiHandler_s"; break;
    case oyOBJECT_FORMS_ARGS_S: text = "oyFormsArgs_s"; break;
    case oyOBJECT_CALLBACK_S: text = "oyCallback_s"; break;
    case oyOBJECT_OBSERVER_S: text = "oyObserver_s"; break;
    case oyOBJECT_CONF_DOMAIN_S: text = "oyConfDomain_s"; break;
    case oyOBJECT_INFO_STATIC_S: text = "oyObjectInfoStatic_s"; break;
    case oyOBJECT_MAX: text = "Max - none"; break;
  }
  return text;
}


/** Function oyObject_GetId
 *  @memberof oyObject_s
 *  @brief   get the identification number of a object 
 *
 *  @version Oyranos: 0.1.8
 *  @since   2008/07/10 (Oyranos: 0.1.8)
 *  @date    2008/07/10
 */
int            oyObject_GetId        ( oyObject_s          object )
{
  struct oyObject_s_* obj = (struct oyObject_s_*)object;
  if(obj)
    return obj->id_;

  return -1;
}



/** @func    oyMessageFormat
 *  @brief   default function to form a message string
 *
 *  This default message function is used as a message formatter.
 *  The resulting string can be placed anywhere, e.g. in a GUI.
 *
 *  @see the oyMessageFunc() needs just to replaxe the fprintf with your 
 *  favourite GUI call.
 *
 *  @version Oyranos: 0.2.1
 *  @since   2008/04/03 (Oyranos: 0.2.1)
 *  @date    2011/01/15
 */
int                oyMessageFormat   ( char             ** message_text,
                                       int                 code,
                                       const oyPointer     context_object,
                                       const char        * string )
{
  char * text = 0, * t = 0;
  int i;
  const char * type_name = "";
  int id = -1;
#ifdef HAVE_POSIX
  pid_t pid = 0;
#else
  int pid = 0;
#endif
  FILE * fp = 0;
  const char * id_text = 0;
  char * id_text_tmp = 0;
  oyStruct_s * c = (oyStruct_s*) context_object;

  if(code == oyMSG_DBG && !oy_debug)
    return 0;

  if(c && oyOBJECT_NONE < c->type_)
  {
    type_name = oyStructTypeToText( c->type_ );
    id = oyObject_GetId( c->oy_ );
    id_text = oyStruct_GetInfo( (oyStruct_s*)c, 0 );
    if(id_text)
      id_text_tmp = strdup(id_text);
    id_text = id_text_tmp;
  }

  text = calloc( sizeof(char), 256 );

# define MAX_LEVEL 20
  if(level_PROG < 0)
    level_PROG = 0;
  if(level_PROG > MAX_LEVEL)
    level_PROG = MAX_LEVEL;
  for (i = 0; i < level_PROG; i++)
    oySprintf_( &text[oyStrlen_(text)], " ");

  STRING_ADD( t, text );

  text[0] = 0;

  switch(code)
  {
    case oyMSG_WARN:
         STRING_ADD( t, _("WARNING") );
         break;
    case oyMSG_ERROR:
         STRING_ADD( t, _("!!! ERROR"));
         break;
  }

  /* reduce output for non core messages */
  if( id > 0 || (oyMSG_ERROR <= code && code <= 399) )
  {
    oyStringAddPrintf_( &t, oyAllocateFunc_,oyDeAllocateFunc_,
                        " %03f: ", DBG_UHR_);
    oyStringAddPrintf_( &t, oyAllocateFunc_,oyDeAllocateFunc_,
                        "%s[%d]%s%s%s ", type_name, id,
             id_text ? "=\"" : "", id_text ? id_text : "", id_text ? "\"" : "");
  }

  STRING_ADD( t, string );

  if(oy_backtrace)
  {
#   define TMP_FILE "/tmp/oyranos_gdb_temp." OYRANOS_VERSION_NAME "txt"
#ifdef HAVE_POSIX
    pid = (int)getpid();
#endif
    fp = fopen( TMP_FILE, "w" );

    if(fp)
    {
      fprintf(fp, "attach %d\n", pid);
      fprintf(fp, "thread 1\nbacktrace\n"/*thread 2\nbacktrace\nthread 3\nbacktrace\n*/"detach" );
      fclose(fp);
      fprintf( stderr, "GDB output:\n" );
      system("gdb -batch -x " TMP_FILE);
    } else
      fprintf( stderr, "could not open " TMP_FILE "\n" );
  }

  free( text ); text = 0;
  if(id_text_tmp) free(id_text_tmp); id_text_tmp = 0;

  *message_text = t;

  return 0;
}

/** @func    oyMessageFunc
 *  @brief   default message function to console
 *
 *  The default message function is used as a message printer to the console 
 *  from library start.
 *
 *  @param         code                a message code understood be your message
 *                                     handler or oyMSG_e
 *  @param         context_object      a oyStruct_s is expected from Oyranos
 *  @param         format              the text format string for following args
 *  @param         ...                 the variable args fitting to format
 *  @return                            0 - success; 1 - error
 *
 *  @version Oyranos: 0.3.0
 *  @since   2008/04/03 (Oyranos: 0.1.8)
 *  @date    2009/07/20
 */
int oyMessageFunc( int code, const oyPointer context_object, const char * format, ... )
{
  char * text = 0, * msg = 0;
  int error = 0;
  va_list list;
  size_t sz = 256;
  int len;
  oyStruct_s * c = (oyStruct_s*) context_object;

  text = calloc( sizeof(char), sz );
  if(!text)
  {
    fprintf(stderr,
    "oyranos_core.c:257 oyMessageFunc() Could not allocate 256 byte of memory.\n");
    return 1;
  }

  text[0] = 0;

  va_start( list, format);
  len = vsnprintf( text, sz-1, format, list);
  va_end  ( list );

  if (len >= ((int)sz - 1))
  {
    text = realloc( text, (len+2)*sizeof(char) );
    va_start( list, format);
    len = vsnprintf( text, len+1, format, list);
    va_end  ( list );
  }

  error = oyMessageFormat( &msg, code, c, text );

  if(msg)
    fprintf( stderr, "%s\n", msg );

  free( text ); text = 0;
  free( msg ); msg = 0;

  return error;
}

oyMessage_f     oyMessageFunc_p = oyMessageFunc;

/** @func    oyMessageFuncSet
 *  @brief
 *
 *  @version Oyranos: 0.1.8
 *  @date    2008/04/03
 *  @since   2008/04/03 (Oyranos: 0.1.8)
 */
int            oyMessageFuncSet      ( oyMessage_f         message_func )
{
  if(message_func)
    oyMessageFunc_p = message_func;
  return 0;
}


/* --- internal API decoupling --- */



/** \addtogroup misc Miscellaneous
 *  Miscellaneous stuff.

 *  @{
 */

/** @brief  get language code
 *
 *  @since Oyranos: version 0.1.8
 *  @date  26 november 2007 (API 0.1.8)
 */
const char *   oyLanguage            ( void )
{
  const char * text = 0;

  DBG_PROG_START
  oyInit_();

  text = oyLanguage_();

  oyExportEnd_();
  DBG_PROG_ENDE

  return text;
}

/** @brief  get country code
 *
 *  @since Oyranos: version 0.1.8
 *  @date  26 november 2007 (API 0.1.8)
 */
const char *   oyCountry             ( void )
{
  const char * text = 0;

  DBG_PROG_START
  oyInit_();

  text = oyCountry_();

  oyExportEnd_();
  DBG_PROG_ENDE

  return text;
}

/** @brief  get LANG code/variable
 *
 *  @since Oyranos: version 0.1.8
 *  @date  26 november 2007 (API 0.1.8)
 */
const char *   oyLang                ( void )
{
  const char * text = 0;

  DBG_PROG_START
  oyInit_();

  text = oyLang_();

  oyExportEnd_();
  DBG_PROG_ENDE

  return text;
}

/** @brief   reset i18n language and  country variables
 *
 *  @version Oyranos: 0.1.10
 *  @since   2009/01/05 (Oyranos: 0.1.10)
 *  @date    2009/01/05
 */
void           oyI18Nreset           ( void )
{
  DBG_PROG_START
  oyI18Nreset_();
  oyInit_();
  oyExportEnd_();
  DBG_PROG_ENDE
}


/** @brief  give the compiled in library version
 *
 *  @param[in]  type           0 - Oyranos API
 *                             1 - start month
 *                             2 - start year
 *                             3 - development last month
 *                             4 - development last year
 *
 *  @return                    OYRANOS_VERSION at library compile time
 */
int            oyVersion             ( int                 type )
{
  if(type == 1)
    return OYRANOS_START_MONTH;
  if(type == 2)
    return OYRANOS_START_YEAR;
  if(type == 3)
    return OYRANOS_DEVEL_MONTH;
  if(type == 4)
    return OYRANOS_DEVEL_YEAR;

  return OYRANOS_VERSION;
}

#include "config.log.h"
/** @brief  give the configure options for Oyranos
 *
 *  @param[in] type
                               - 1  OYRANOS_VERSION_NAME;
                               - 2  git master hash;
                               - 3  OYRANOS_CONFIG_DATE,
                               - 4  development period
 *  @param     allocateFunc    user allocator, e.g. malloc
 *
 *  @return                    Oyranos configure output
 *
 *  @since     Oyranos: version 0.1.8
 *  @date      18 december 2007 (API 0.1.8)
 */
char *       oyVersionString         ( int                 type,
                                       oyAlloc_f           allocateFunc )
{
  char * text = 0, * tmp = 0;
  char temp[24];
  char * git = OYRANOS_GIT_MASTER;

  if(!allocateFunc)
    allocateFunc = oyAllocateFunc_;

  if(type == 1)
    return oyStringCopy_(OYRANOS_VERSION_NAME, allocateFunc);
  if(type == 2)
  {
    if(git[0])
      return oyStringCopy_(git, allocateFunc);
    else
      return 0;
  }
  if(type == 3)
    return oyStringCopy_(OYRANOS_CONFIG_DATE, allocateFunc);

  if(type == 4)
  {
#ifdef HAVE_POSIX
    oyStringAdd_( &text, nl_langinfo(MON_1-1+oyVersion(1)),
                                            oyAllocateFunc_, oyDeAllocateFunc_);
#endif
    oySprintf_( temp, " %d - ", oyVersion(2) );
    oyStringAdd_( &text, temp, oyAllocateFunc_, oyDeAllocateFunc_);
#ifdef HAVE_POSIX
    oyStringAdd_( &text, nl_langinfo(MON_1-1+oyVersion(3)),
                                            oyAllocateFunc_, oyDeAllocateFunc_);
#endif
    oySprintf_( temp, " %d", oyVersion(4) );
    oyStringAdd_( &text, temp, oyAllocateFunc_, oyDeAllocateFunc_);

    tmp = oyStringCopy_( text , allocateFunc);
    oyDeAllocateFunc_(text);
    return tmp;
  }

#ifdef HAVE_POSIX
  return oyStringCopy_(oy_config_log_, allocateFunc);
#else
  return oyStringCopy_("----", allocateFunc);
#endif
}

int                oyBigEndian       ( void )
{
  int big = 0;
  char testc[2] = {0,0};
  uint16_t *testu = (uint16_t*)testc;
  *testu = 1;
  big = testc[1];
  return big;
}


/** @brief MSB<->LSB */
icUInt16Number
oyValueUInt16 (icUInt16Number val)
{
  if(!oyBigEndian())
  {
  # define BYTES 2
  # define KORB  4
    unsigned char        *temp  = (unsigned char*) &val;
    unsigned char  korb[KORB];
    int i;
    for (i = 0; i < KORB ; i++ )
      korb[i] = (int) 0;  /* empty */

    {
    int klein = 0,
        gross = BYTES - 1;
    for (; klein < BYTES ; klein++ ) {
      korb[klein] = temp[gross--];
    }
    }

    {
    unsigned int *erg = (unsigned int*) &korb[0];

  # undef BYTES
  # undef KORB
    return (long)*erg;
    }
  } else
  return (long)val;
}

icUInt32Number
oyValueUInt32 (icUInt32Number val)
{
  if(!oyBigEndian())
  {
    unsigned char        *temp = (unsigned char*) &val;

    unsigned char  uint32[4];

    uint32[0] = temp[3];
    uint32[1] = temp[2];
    uint32[2] = temp[1];
    uint32[3] = temp[0];

    {
    unsigned int *erg = (unsigned int*) &uint32[0];


    return (icUInt32Number) *erg;
    }
  } else
    return (icUInt32Number)val;
}


icS15Fixed16Number      oyValueInt32    (icS15Fixed16Number val)
{
  if(!oyBigEndian())
  {
    unsigned char        *temp = (unsigned char*) &val;

    unsigned char  uint32[4];

    uint32[0] = temp[3];
    uint32[1] = temp[2];
    uint32[2] = temp[1];
    uint32[3] = temp[0];

    {
    int *erg = (int*) &uint32[0];


    return (icS15Fixed16Number) *erg;
    }
  } else
    return (icS15Fixed16Number)val;
}

unsigned long
oyValueUInt64 (icUInt64Number val)
{
  if(!oyBigEndian())
  {
    unsigned char        *temp  = (unsigned char*) &val;

    unsigned char  uint64[8];
    int little = 0,
        big    = 8;

    for (; little < 8 ; little++ ) {
      uint64[little] = temp[big--];
    }

    {
    unsigned long *erg = (unsigned long*) &uint64[0];

    return (long)*erg;
    }
  } else
  return (long)val;
}


/** @} */



/* deprecated function to reduce dlopen warnings after API break in 0.3.0; 
   these APIs are not deselected after dlopen */
int oyFilterMessageFunc() { return 1; }



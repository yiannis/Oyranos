/** @file oyranos_cmm_oyIM_profile.c
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  @par Copyright:
 *            2008-2011 (C) Kai-Uwe Behrmann
 *
 *  @brief    modules for Oyranos
 *  @internal
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2008/12/16
 */

#include "config.h"
#include "oyranos_cmm_oyIM.h"
#include "oyranos_alpha.h"
#include "oyranos_cmm.h"
#include "oyranos_helper.h"
#include "oyranos_icc.h"
#include "oyranos_i18n.h"
#include "oyranos_io.h"
#include "oyranos_definitions.h"
#include "oyranos_string.h"
#include "oyranos_texts.h"
#include <iconv.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CMM_BASE_REG OY_TOP_SHARED OY_SLASH OY_DOMAIN_INTERNAL OY_SLASH OY_TYPE_STD OY_SLASH "icc."

uint32_t oyGetTableUInt32_           ( const char        * mem,
                                       int                 entry_size,
                                       int                 entry_pos,
                                       int                 pos );
uint16_t oyGetTableUInt16_           ( const char        * mem,
                                       int                 entry_size,
                                       int                 entry_pos,
                                       int                 pos );
oyStructList_s *   oyStringsFrommluc ( const char        * mem,
                                       uint32_t            size );
oyStructList_s *   oyCurveFromTag    ( char              * data,
                                       size_t              size );
oyStructList_s *   oyCurvesFromTag   ( char              * data,
                                       size_t              size,
                                       int                 count );

#define AD oyAllocateFunc_, oyDeAllocateFunc_

/* --- implementations --- */

/** @func  oyIMProfileCanHandle
 *  @brief inform about icTagTypeSignature capabilities
 *
 *  @version Oyranos: 0.1.8
 *  @since   2008/01/03 (Oyranos: 0.1.8)
 *  @date    2008/05/23
 */
int        oyIMProfileCanHandle      ( oyCMMQUERY_e      type,
                                       uint32_t          value )
{
  int ret = -1;

  switch(type)
  {
    case oyQUERY_OYRANOS_COMPATIBILITY:
         ret = OYRANOS_VERSION; break;
    case oyQUERY_PROFILE_FORMAT:
         if(value == 1)
           ret = 1;
         else
           ret = 0;
         break;
    case oyQUERY_PROFILE_TAG_TYPE_READ:
         switch(value) {
         case icSigColorantOrderType:
         case icSigColorantTableType:
         case icSigCurveType:
         case icSigDeviceSettingsType:
         case icSigDescriptiveNameValueMuArrayType_:
         case icSigLutAtoBType:
         case icSigLutBtoAType:
         case icSigMakeAndModelType:
         case icSigNativeDisplayInfoType:
         case icSigDictType:
         case icSigMultiLocalizedUnicodeType:
         case icSigWCSProfileTag:
         case icSigParametricCurveType:
         case icSigProfileSequenceDescType:
         case icSigProfileSequenceIdentifierType:
         case icSigSignatureType:
         case icSigTextDescriptionType:
         case icSigTextType:
              ret = 1; break;
         default: ret = 0; break;
         }
         break;
    case oyQUERY_PROFILE_TAG_TYPE_WRITE:
         switch(value) {
         case icSigMultiLocalizedUnicodeType:
         case icSigProfileSequenceIdentifierType:
         case icSigTextDescriptionType:
         case icSigTextType:
              ret = 1; break;
         default: ret = 0; break;
         }
         break;
    default: break;
  }

  return ret;
}

int    oyICCparametricCurveToSegments (oyOption_s        * parameters,
                                       oyOption_s        * segments,
                                       int                 segments_start,
                                       int                 count,
                                       double              start,
                                       double              end )
{
  int error = 0;

  if(parameters &&
     oyFilterRegistrationMatchKey( oyOption_GetRegistration( parameters ),
                                   "////icParametricCurveType", oyOBJECT_NONE ))
  {
    double type = oyOption_GetValueDouble( parameters, 0 );
    /*double params_n = oyOption_GetValueDouble( parameters, 1 );*/
    double gamma = oyOption_GetValueDouble( parameters, 2 ),
           a,b,c,d,e,f;

    double step = (end - start) / (count-1),
           X, Y;
    int i;


             if(type == 0) /* gamma */
             {
               for(i = 0; i < count; ++i)
               {
                 X = start + step * i;

                 /* Y = X^gamma */
                 Y = pow( X, gamma );

                 oyOption_SetFromDouble( segments, Y, segments_start + i, 0 );
               }
             } else if(type == 1)
             {
               for(i = 0; i < count; ++i)
               {
                 a = oyOption_GetValueDouble( parameters, 2+1 ),
                 b = oyOption_GetValueDouble( parameters, 2+2 ),
                 X = start + step * i;
                 /* Y=(a*X+b)^gamma for X>=-b/a
                  * Y=0 for (X<-b/a)
                  */
                 if(X >= -b/a)
                   Y = pow( a*X+b, gamma );
                 else
                   Y = 0;

                 oyOption_SetFromDouble( segments, Y, segments_start + i, 0 );
               }
             } else if(type == 2)
             {
               for(i = 0; i < count; ++i)
               {
                 a = oyOption_GetValueDouble( parameters, 2+1 ),
                 b = oyOption_GetValueDouble( parameters, 2+2 ),
                 c = oyOption_GetValueDouble( parameters, 2+3 ),
                 X = start + step * i;

                 /* Y=(a*X+b)^gamma + c for X>=-b/a
                  * Y=c for (X<-b/a)
                  */
                 if(X >= -b/a)
                   Y = pow( a*X+b, gamma ) + c;
                 else
                   Y = c;

                 oyOption_SetFromDouble( segments, Y, segments_start + i, 0 );
               }
             } else if(type == 3)
             {
               for(i = 0; i < count; ++i)
               {
                 a = oyOption_GetValueDouble( parameters, 2+1 ),
                 b = oyOption_GetValueDouble( parameters, 2+2 ),
                 c = oyOption_GetValueDouble( parameters, 2+3 ),
                 d = oyOption_GetValueDouble( parameters, 2+4 ),
                 X = start + step * i;

                 /* Y=(a*X+b)^gamma for X>=d
                  * Y=c*X for (X<d)
                  */
                 if(X >= d)
                   Y = pow( a*X+b, gamma );
                 else
                   Y = c*X;

                 oyOption_SetFromDouble( segments, Y, segments_start + i, 0 );
               }
             } else if(type == 4)
             {
               for(i = 0; i < count; ++i)
               {
                 a = oyOption_GetValueDouble( parameters, 2+1 ),
                 b = oyOption_GetValueDouble( parameters, 2+2 ),
                 c = oyOption_GetValueDouble( parameters, 2+3 ),
                 d = oyOption_GetValueDouble( parameters, 2+4 ),
                 e = oyOption_GetValueDouble( parameters, 2+5 ),
                 f = oyOption_GetValueDouble( parameters, 2+6 ),
                 X = start + step * i;

                 /* Y=(a*X+b)^gamma + e for X>=d
                  * Y=c*X+f for (X<d)
                  */
                 if(X >= d)
                   Y = pow( a*X+b, gamma ) + e;
                 else
                   Y = c*X+f;

                 oyOption_SetFromDouble( segments, Y, segments_start + i, 0 );
               }
             }

  }

  return error;
}

int  oyWriteIcSigLutAtoBType         ( oyStructList_s    * texts,
                                       int                 channels_in,
                                       int                 channels_out,
                                       icTagTypeSignature  tag_sig,
                                       char              * mem,
                                       size_t              offset_bcurve,
                                       size_t              offset_matrix,
                                       size_t              offset_mcurve,
                                       size_t              offset_clut,
                                       size_t              offset_acurve,
                                       size_t              tag_size )
{
  int error = 0;
  int size, i;
  size_t off;
  uint8_t * dimensions, precission, *u8;
  uint16_t u16;
  char * tmp = 0;
  char * text = oyAllocateFunc_(128);
  oyOption_s        * opt = 0;
  oyStructList_s * list;
  int curves_n;
  double val; 


             if(offset_acurve)
             {
               off = offset_acurve;
               list = oyCurvesFromTag( &mem[off], tag_size - off, channels_in);
               curves_n = oyStructList_Count( list );
               if(curves_n == channels_in)
               {
                 opt = oyOption_FromRegistration("////color_space",0);
                 oyOption_SetFromText( opt, "1", 0 );
                 for(i = 0; i < curves_n; ++i)
                 {
                   oyStructList_s * element = (oyStructList_s*)
                                               oyStructList_GetRefType( list, i,
                                               oyOBJECT_STRUCT_LIST_S );
                   oyOption_s * tmp = oyOption_Copy(opt,0);
                   oyStructList_MoveIn( element, (oyStruct_s**)&tmp, -1, 0 );
                   oyStructList_Release( &element );
                 }
                 oyOption_Release( &opt );
               }

               oyStringAddPrintf_( &tmp, AD, "%s A: %d",
                                   _("Curves"), channels_in );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
               oyStructList_MoveIn( texts, (oyStruct_s**)&list, -1, 0 );
             }

             if(offset_clut)
             {
               off = offset_clut;
               if(tag_size >= off + 20)
               {
                 dimensions = (uint8_t*)&mem[off+0];
                 precission = mem[off+16];
                 size = 1;
                 for(i = 0; i < channels_in; ++i)
                   size *= dimensions[i];
                 size *= precission * channels_out;

                 if(tag_size < off+20+size)
                   error = 1;
                 else
                 {
                   opt =oyOption_FromRegistration("////icSigLutAtoBTypeNlut",0);
                   oyOption_SetFromDouble( opt, channels_in, 0, 0 );
                   oyOption_SetFromDouble( opt, channels_out, 1, 0 );
                   oyOption_SetFromDouble( opt, precission, 2, 0 );
                   for(i = channels_in-1; i >= 0; --i)
                     oyOption_SetFromDouble( opt, dimensions[i], 2+i, 0 );
                 }
               }

               if(error <= 0)
               {
                 oyStringAddPrintf_( &tmp, AD, "%s: %d->%d[%s] ",
                                     _("nLUT"), channels_in, channels_out,
                             precission == 1 ? "8-bit":"16-bit" );
                 for(i = 0; i < channels_in; ++i)
                 {
                   if(i)
                     oyStringAddPrintf_( &tmp, AD, "x" );
                   oyStringAddPrintf_( &tmp, AD, "%d",
                                       dimensions[i] );
                 }
                 oyStructList_AddName( texts, tmp, -1);
                 oyFree_m_( tmp );
               } else
               {
                 oySprintf_( text, "%s %s", _("nLUT"), _("Error"));
                 oyStructList_AddName( texts, text, -1);
               }

               if(error <= 0)
               {
                 size /= precission;
                 u8 = (uint8_t*)&mem[off+20];
                 if(precission == 1) /* 8-bit */
                   for(i = size-1; i >= 0; --i)
                   {
                     val = u8[i]/256.0;
                     oyOption_SetFromDouble( opt, val,
                                             3 + channels_in + i, 0 );
                   }
                 else if(precission == 2) /* 16-bit */
                   for(i = size-1; i >= 0; --i)
                   {
                     u16 = oyGetTableUInt16_( &mem[off+20], 0, 0, i );
                     val = u16/65536.0;
                     oyOption_SetFromDouble( opt, val, 
                                             3 + channels_in + i, 0 );
                   }
               }
               oyStructList_MoveIn( texts, (oyStruct_s**)&opt, -1, 0 );
             }

             if(offset_mcurve)
             {
               off = offset_mcurve;
               list = oyCurvesFromTag( &mem[off], tag_size - off, channels_out);
               curves_n = oyStructList_Count( list );
               if(curves_n == channels_in)
               {
                 opt = oyOption_FromRegistration("////color_space",0);
                 oyOption_SetFromText( opt, "0", 0 );
                 for(i = 0; i < curves_n; ++i)
                 {
                   oyStructList_s * element = (oyStructList_s*)
                                               oyStructList_GetRefType( list, i,
                                               oyOBJECT_STRUCT_LIST_S );
                   oyOption_s * tmp = oyOption_Copy(opt,0);
                   oyStructList_MoveIn( element, (oyStruct_s**)&tmp, -1, 0 );
                   oyStructList_Release( &element );
                 }
                 oyOption_Release( &opt );
               }

               oyStringAddPrintf_( &tmp, AD, "%s M: %d",
                                   _("Curves"), channels_in );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
               oyStructList_MoveIn( texts, (oyStruct_s**)&list, -1, 0 );
             }

             if(offset_matrix)
             {
               int32_t i32;
               opt =oyOption_FromRegistration("////Matrix3x3+3",0);
               off = offset_matrix;
               if(tag_size >= off + 12*4)
               for(i = 0; i < 12; ++i)
               {
                 i32 = oyValueInt32( (int32_t)*((int32_t*)&mem[off+i*4]));
                 oyOption_SetFromDouble( opt, i32/65536.0, i, 0 );
               }

               oySprintf_( text, "%s", _("Matrix"));
               oyStructList_AddName( texts, text, -1);
               oyStructList_MoveIn( texts, (oyStruct_s**)&opt, -1, 0 );
             }

             if(offset_bcurve)
             {
               off = offset_bcurve;
               list = oyCurvesFromTag( &mem[off], tag_size - off, channels_out);
               curves_n = oyStructList_Count( list );
               if(curves_n == channels_in)
               {
                 opt = oyOption_FromRegistration("////color_space",0);
                 oyOption_SetFromText( opt, "0", 0 );
                 for(i = 0; i < curves_n; ++i)
                 {
                   oyStructList_s * element = (oyStructList_s*)
                                               oyStructList_GetRefType( list, i,
                                               oyOBJECT_STRUCT_LIST_S );
                   oyOption_s * tmp = oyOption_Copy(opt,0);
                   oyStructList_MoveIn( element, (oyStruct_s**)&tmp, -1, 0 );
                   oyStructList_Release( &element );
                 }
                 oyOption_Release( &opt );
               }

               oyStringAddPrintf_( &tmp, AD, "%s B: %d",
                                   _("Curves"), channels_out );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
               oyStructList_MoveIn( texts, (oyStruct_s**)&list, -1, 0 );
             }

  oyFree_m_( text );

  return error;
}

/** @func    oyIMProfileTag_GetValues
 *  @brief   get values from ICC profile tags
 *
 *  The function implements oyCMMProfileTag_GetValues_t for 
 *  oyCMMapi3_s::oyCMMProfileTag_GetValues.
 *
 *  - function description are obtained by following steps:
 *    - set the tag argument to zero
 *    - the returned list will be filled in with oyName_s' each matching a tag_type
 *      - oyNAME_NICK contains the module info, e.g. 'oyIM'
 *      - oyNAME_NAME contains the tag_type, e.g. 'icSigMultiLocalizedUnicodeType' or 'mluc'
 *      - oyNAME_DESCRIPTION contains text as in above documentation
 *    - dont copy the list as content may be statically allocated
 *
 *  The output depends on the tags type signature in tag->tag_type_ as follows:
 *
 *  - icSigColorantOrderType and :
 *    - since Oyranos 0.1.12 (API 0.1.12)
 *    - returns: text list
 *      - the number of channels
 *      - the position of the normal channel names as strings 1 + i
 *
 *  - icSigColorantTableType:
 *    - since Oyranos 0.1.12 (API 0.1.12)
 *    - The PCS values are integers in the range of 0-65535.
 *    - The PCS value interpretation depends on the profiles PCS header field.
 *      - the short channel name as string in 1 + 2 * i
 *      - PCS representation as three space separated integers in 1 + 2 * i + 1
 *
 *  - icSigCurveType:
 *    - since Oyranos 0.3.1 (API 0.3.1)
 *    - returns:
 *      - a string describing the curve
 *      - a option containing doubles
 *        - first entry : count with zero means idendity, one means gamma
 *        - more doubles means a segmented curve
 *        - second entry : values start
 *
 *  - icSigTextType and icSigWCSProfileTag:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - returns one string
 *
 *  - icSigParametricCurveType:
 *    - since Oyranos 0.3.1 (API 0.3.1)
 *    - returns
 *      - position 0 : the type of the curve as of ICC spec 10.15\
 *      - position 1 : count of parameters - param_n\
 *      - position 2 : first paramter for the parametric formula\
 *      - position 2 + param_n : the number of a segmented curve - seg_count\
 *      - position 2 + param_n + 1 : the curves value for 0.0\
 *      - position 2 + param_n + 1 + seg_count - 1 : the curves value for 1.0"
 *
 *  - icSigTextDescriptionType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - returns one string
 *
 *  - icSigMultiLocalizedUnicodeType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - list: will contain oyName_s objects
 *      - oyName_s::name will hold the name
 *      - oyName_s::lang will hold i18n specifier, e.g. \"en_GB\"
 *
 *  - icSigSignatureType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - returns one string
 *    - for the value see oyICCTechnologyDescription
 *
 *  - icSigDescriptiveNameValueMuArrayType_:
 *    - since Oyranos 0.1.10 (API 0.1.10)
 *    - returns
 *      - introduction text
 *      - ascii string with the number (i) of the found elements
 *      - a key string in 2 + i * 2
 *      - a value string in in 2 + i * 2 + 1
 *
 *  - icSigLutAtoBType
 *
 *  - icSigMakeAndModelType:
 *    - since Oyranos 0.1.8 (API 0.1.10)
 *    - returns eigth strings, uneven is descriptive, even from a uint32_t
 *      - manufacturer id
 *      - model id
 *      - serialNumber id
 *      - manufacturer date id
 *
 *  - icSigNativeDisplayInfoType:
 *    - since Oyranos 0.1.11 (API 0.1.11)
 *    - returns a list of strings, uneven is descriptive, even contains values
 *
 *  - icSigDictType:
 *    - since Oyranos 0.1.10 (API 0.1.12)
 *    - returns four strings each originating from a uint32_t
 *      - the size of components (c) as ascii string (2 - key/value pairs;
 *        3 - key/value pairs + key UI translations, 3 - key/value pairs + 
 *        key UI translations + value UI translations)
 *      - the number (i) of the found elements as ascii string
 *      - key string in 2 + i * c
 *      - value string in 2 + i * c + 1
 *      - oyStructList_s with language strings in 2 + i * c + 2
 *      - oyStructList_s with language strings in 2 + i * c + 3
 *
 *  - icSigProfileSequenceDescType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - returns
 *      - first string as ascii the number (i) of the found elements
 *      - a profile anounce string in 1 + i * 7
 *      - the translated "Manufacturer:" string in 1 + i * 7 + 1
 *      - the manufacturer string in 1 + i * 7 + 2, the full lenght or 4 byte
 *      - the translated "Model:" string in 1 + i * 7 + 3
 *      - the model string in 1 + i * 7 + 4, the full lenght or 4 byte one
 *      - the translated "Technology:" string in 1 + i * 7 + 5
 *      - the tech string in 1 + i * 7 + 6, see oyICCTechnologyDescription
 *
 *  - icSigProfileSequenceIdentifierType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - returns
 *      - first string as ascii the number (i) of the found elements
 *      - a profile anounce string in 1 + i * 5
 *      - the string "md5id:" in in 1 + i * 5 + 1
 *      - the low letter hexadecimal hash value in 1 + i * 5 + 2
 *      - mluc translated by oyICCTagDescription in 1 + i * 5 + 3
 *      - the icSigProfileDescriptionTag according to language in 1 + i * 5 + 4
 *
 *  - icSigDeviceSettingsType:
 *    - since Oyranos 0.1.10 (API 0.1.10)
 *    - returns
 *      - version announce string
 *      - string version
 *      - announce string
 *      - device serial
 *      - announce string
 *      - driver name
 *      - announce string
 *      - driver version
 *      - announce string
 *      - driver signature/encoding
 *      - announce string
 *      - priority (0-255)
 *      - announce string
 *      - oyBlob_s data blob
 *
 *  @version Oyranos: 0.1.10
 *  @since   2008/01/02 (Oyranos: 0.1.8)
 *  @date    2009/01/03
 */
oyStructList_s * oyIMProfileTag_GetValues(
                                       oyProfileTag_s    * tag )
{
  oyStructList_s * values = 0;
  icUInt32Number error = 0, len = 0, count = 0, entry_size = 0;
  oyStructList_s * texts = 0, * temp = 0;
  char * tmp = 0;
  char * mem = 0;
  char * pos = 0;
  icTagBase * tag_base = 0;
  icTagTypeSignature  sig = 0;
  int32_t size_ = -1;
  oyStructList_s * list = 0;
  icTagTypeSignature tag_sig = (icTagSignature)0;
  char num[32];
  oyName_s * name = 0;
  oyBlob_s * o = 0;
  oyOption_s * opt = 0;

  int mluc_size = 0;
  oyStructList_s * desc_tmp = 0;
  int desc_tmp_n = 0;
  oyProfileTag_s * tmptag = 0;



  /* provide information about the function */
  if(!tag)
  {
    oyStructList_s * list = oyStructList_New( 0 );

    oyName_s description_clro = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "clro",
      "\
- icSigColorantOrderType and :\
  - since Oyranos 0.1.12 (API 0.1.12)\
  - returns: text list\
    - the number of channels\
    - the position of the normal channel names as strings 1 + i"
    };

    oyName_s description_clrt = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "clrt",
      "\
- icSigColorantTableType:\
 - since Oyranos 0.1.12 (API 0.1.12)\
 - The PCS values are integers in the range of 0-65535.\
 - The PCS value interpretation depends on the profiles PCS header field.\
 - returns: text list\
    - the number of channels\
    - the short channel name as string in 1 + 2 * i\
    - PCS representation as three space separated integers in 1 + 2 * i + 1"
    };

    oyName_s description_curv = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "curv",
      "\
- icSigCurveType:\
  - since Oyranos 0.3.1 (API 0.3.1)\
  - returns:\
    - a string describing the curve\
    - a option containing doubles\
      - first entry : count with zero means idendity, one means gamma\
      - more doubles means a segmented curve\
      - second entry : values start"
    };

    oyName_s description_mluc = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "mluc",
      "\
- icSigMultiLocalizedUnicodeType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: will contain oyName_s objects\
    - oyName_s::name will hold the name\
    - oyName_s::lang will hold i18n specifier, e.g. \"en_GB\""
    };

    oyName_s description_nvmt = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "nvmt",
      "\
- icSigDescriptiveNameValueMuArrayType_:\
  - since Oyranos 0.1.10 (API 0.1.10)\
  - returns\
    - introduction text\
    - ascii string with the number (i) of the found elements\
    - a key string in 2 + i * 2\
    - a value string in in 2 + i * 2 + 1"
    };

    oyName_s description_mAB = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "mAB",
      "\
- icSigLutAtoBType:"
    };

    oyName_s description_mmod = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "mmod",
      "\
- icSigMakeAndModelType:\
  - since Oyranos 0.1.8 (API 0.1.10)\
  - returns eigth strings, uneven is descriptive, even from a uint32_t\
    - manufacturer id\
    - model id\
    - serialNumber id\
    - manufacturer date id"
    };

    oyName_s description_ndin = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "ndin",
      "\
- icSigNativeDisplayInfoType:\
  - since Oyranos 0.1.11 (API 0.1.11)\
  - returns a list of strings, uneven is descriptive, even contains values"
    };

    oyName_s description_psid = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "psid",
      "\
- icSigProfileSequenceIdentifierType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: will contain oyName_s objects\
    - first string as ascii the number (i) of the found elements\
    - a profile anounce string in 1 + i * 5\
    - the string \"md5id:\" in in 1 + i * 5 + 1\
    - the low letter hexadecimal hash value in 1 + i * 5 + 2\
    - mluc translated by oyICCTagDescription in 1 + i * 5 + 3\
    - the icSigProfileDescriptionTag according to language in 1 + i * 5 + 4"
    };

    oyName_s description_MS10 = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "MS10",
      "\
- icSigWCSProfileTag:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyName_s"
    };

    oyName_s description_para = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "para",
      "\
- icSigParametricCurveType:\
  - since Oyranos 0.3.1 (API 0.3.1)\
  - returns\
    - a string describing the curve\
    - a option containing doubles\
      - position 0 : the type of the curve as of ICC spec 10.15\
      - position 1 : count of parameters - param_n\
      - position 2 : first paramter for the parametric formula\
      - position 2 + param_n : the number of a segmented curve - seg_count\
      - position 2 + param_n + 1 : the curves value for 0.0\
      - position 2 + param_n + 1 + seg_count - 1 : the curves value for 1.0"
    };

    oyName_s description_text = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "text",
      "\
- icSigTextType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyName_s"
    };

    oyName_s description_desc = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "desc",
      "\
- icSigTextDescriptionType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyName_s"
    };

    oyName_s description_DevS = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "DevS",
      "\
- icSigDeviceSettingsType:\
  - since Oyranos 0.1.10 (API 0.1.10)\
  - returns\
    - version announce string \
    - string version \
    - announce string\
    - device serial\
    - announce string\
    - driver name\
    - announce string\
    - driver version\
    - announce string\
    - driver signature/encoding\
    - announce string\
    - priority (0-255)\
    - announce string\
    - oyBlob_s data blob"
    };
    oyStruct_s * description = 0;

    description = (oyStruct_s*) &description_mluc;
    error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_clro;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_clrt;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_curv;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_nvmt;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_mAB;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_mmod;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_ndin;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_psid;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_MS10;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_para;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_text;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_desc;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_DevS;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    return list;
  }

  texts = oyStructList_New(0);
  temp = oyStructList_New(0);

  error = !texts || ! temp;

  if(!error && tag->status_ == oyOK)
  {
    tag_base = tag->block_;
    mem = tag->block_;
    sig = tag->tag_type_;

    error = !mem || !tag->size_ > 12;

    if(!error)
    switch( (uint32_t)sig )
    {
      case icSigColorantOrderType:
           if (tag->size_ <= 12)
           { return texts; }
           else
           {
             count = *(icUInt32Number*)(mem+8);
             count = oyValueUInt32( count );

             oySprintf_( num, "%d", count );
             oyStructList_AddName( texts, num, -1);

             size_ = 12 + count;

             error = tag->size_ < size_;
           }

           if(error <= 0)
           {
             int i;
             for(i = 0; i < count; ++i)
             {
               oySprintf_( num, "%d", mem[13 + i] );
               oyStructList_AddName( texts, num, -1);
             }
           }

           break;
      case icSigCurveType:

           if (tag->size_ <= 12)
           { return texts; }
           else
           {
             count = *(icUInt32Number*)(mem+8);
             count = oyValueUInt32( count );

             size_ = 12 + count * 4;

             error = tag->size_ < size_;
           }

           if(error <= 0)
           {
             double val; 
             int i;
             icCurveType * daten = (icCurveType*)&mem[0]; 

             opt = oyOption_FromRegistration("////icCurveType",0);
             oyOption_SetFromDouble( opt, count, 0, 0 );
             if(count == 0) /* idendity */
             {
               oyOption_SetFromDouble( opt, 1.0, 1, 0 );
               oyStringAddPrintf_( &tmp, AD, "%s", _("Idendity") );
             } else
             if(count == 1) /* icU16Fixed16Number */
             {
               val = oyValueUInt16(daten->curve.data[0]) / 256.0;
               oyOption_SetFromDouble( opt, val, 1, 0 );
               oyStringAddPrintf_( &tmp, AD, "%s: %g", _("Gamma"), val );
             } else
             {
               oyStringAddPrintf_( &tmp, AD, "%s: %d", _("Segmented Curve"),
                                   count );
               for( i = 0; i < count; ++i)
               {
                 val = oyValueUInt16(daten->curve.data[i])/65536.0;
                 oyOption_SetFromDouble( opt, val, 1 + i, 0 );
               }
             }
             oyStructList_AddName( texts, tmp, -1 );
             oyStructList_MoveIn( texts, (oyStruct_s**)&opt, -1, 0 );
           }

           break;
      case icSigColorantTableType:
           if (tag->size_ <= 12)
           { return texts; }
           else
           {
             count = *(icUInt32Number*)(mem+8);
             count = oyValueUInt32( count );

             oySprintf_( num, "%d", count );
             oyStructList_AddName( texts, num, -1);

             size_ = 12 + count * 38;

             error = tag->size_ < size_;
           }

           if(error <= 0)
           {
             int i, j;
             icUInt16Number * pcs;
             uint16_t pcs_triple[3];
             for(i = 0; i < count; ++i)
             {
               memcpy( num, &mem[12 + i*38], 32 );
               num[31] = 0;
               oyStructList_AddName( texts, num, -1);
               pcs =  (icUInt16Number*)&mem[12 + i*38 + 32];
               for(j = 0; j < 3; ++j)
                 pcs_triple[j] = oyValueUInt16(pcs[j]);
               oySprintf_( num, "%d %d %d", 
                                pcs_triple[0], pcs_triple[1], pcs_triple[2] );
               oyStructList_AddName( texts, num, -1);
             }
           }

           break;
      case icSigDictType:
           error = tag->size_ < 16;
           if(error)
             oyStructList_AddName( texts, "unrecoverable parameters found", -1);

           if(error <= 0)
           {
             count = *(icUInt32Number*)(mem+8);
             count = oyValueUInt32( count );
             entry_size = *(icUInt32Number*)(mem+12);
             entry_size = oyValueUInt32( entry_size );
           }

           if(error <= 0)
           {
             switch( entry_size )
             {
               case 16: oySprintf_( num, "2" ); break;
               case 24: oySprintf_( num, "3" ); break;
               case 32: oySprintf_( num, "4" ); break;
               default: error = 1;
             }
             if(error <= 0)
               oyStructList_AddName( texts, num, -1);
             else
              oyStructList_AddName( texts, "unrecoverable parameter found", -1);

             oySprintf_( num, "%d", count );
             oyStructList_AddName( texts, num, -1);
             /*size_ = 12;*/
           }

           if(error <= 0)
           {
             uint32_t i = 0,
                      key_offset = 0, key_size = 0,
                      value_offset = 0, value_size = 0,
                      key_ui_offset = 0, key_ui_size = 0,
                      value_ui_offset = 0, value_ui_size = 0;

             for(i = 0; i < count && tag->size_ >= i * entry_size; ++i)
             {
               key_offset = oyGetTableUInt32_( &mem[16], entry_size, i, 0 );
               key_size = oyGetTableUInt32_( &mem[16], entry_size, i, 1 );
               value_offset = oyGetTableUInt32_( &mem[16], entry_size, i, 2 );
               value_size = oyGetTableUInt32_( &mem[16], entry_size, i, 3 );

               /* add key */
               if( key_offset && key_offset >= 16+entry_size*count-1 &&
                   key_offset + key_size < tag->size_ )
               {
                 tmp = oyAllocateFunc_( key_size * 2 + 2 );
                 error = oyIMIconv( &mem[key_offset], key_size, tmp,
                                    "UTF-16BE" );
               } else
                 STRING_ADD( tmp, "" );
               oyStructList_MoveInName( texts, &tmp, -1 );

                /* add value */
               if( value_offset && value_offset >= 16+entry_size*count-1 &&
                   value_offset + value_size < tag->size_ )
               {
                 tmp = oyAllocateFunc_( value_size * 2 + 2 );
                 error = oyIMIconv( &mem[value_offset], value_size, tmp,
                                    "UTF-16BE" );
               } else
                 STRING_ADD( tmp, "" );
               oyStructList_MoveInName( texts, &tmp, -1 );

                                      

               if(entry_size == 24 || entry_size == 32)
               {
                 key_ui_offset = oyGetTableUInt32_( &mem[16], entry_size, i, 4);
                 key_ui_size = oyGetTableUInt32_( &mem[16], entry_size, i, 5 );

                 list = oyStringsFrommluc( &mem[key_ui_offset], key_ui_size );

                 oyStructList_MoveIn( texts, (oyStruct_s**)&list, -1, 0 );
                 
                 if(entry_size == 32)
                 {
                 value_ui_offset = oyGetTableUInt32_( &mem[16], entry_size,i,6);
                 value_ui_size = oyGetTableUInt32_( &mem[16], entry_size, i, 7);

                 list = oyStringsFrommluc( &mem[value_ui_offset],value_ui_size);

                 oyStructList_MoveIn( texts, (oyStruct_s**)&list, -1, 0 );
                 }
               }
             }
           }

           break;
      case icSigDescriptiveNameValueMuArrayType_:
           error = tag->size_ < 12;
           count = *(icUInt32Number*)(mem+8);
           count = oyValueUInt32( count );

           if(error <= 0)
           {
             /* "key/value pairs found:" followed by the number on the next line"%d" */
             STRING_ADD( tmp, _("key/value pairs found:") );
             oyStructList_MoveInName( texts, &tmp, -1 );
             oySprintf_( num, "%d", count );
             oyStructList_AddName( texts, num, -1);
             /*size_ = 12;*/
           }

           {
             uint32_t i = 0;
             char text[68];

             for(i = 0; i < count && tag->size_ >= i * 144; ++i)
             {
               memcpy( text, &mem[12 + i*144 + 0], 64);
               text[64] = 0;
               oyStructList_AddName( texts, text, -1);
               memcpy( text, &mem[12 + i*144 + 64], 64);
               text[64] = 0;
               oyStructList_AddName( texts, text, -1);
             }
           }

           if(tag->size_ < count * 144)
             oyStructList_AddName( texts, "unrecoverable parameters found", -1);

           break;
      case icSigLutAtoBType:
           error = tag->size_ < 32;
           count = *(icUInt8Number*)(mem+8);
           
           if(error <= 0)
           {
             uint8_t channels_in = *(icUInt8Number*)(mem+8);
             uint8_t channels_out = *(icUInt8Number*)(mem+9);
             uint32_t offset_bcurve, offset_matrix, offset_mcurve, offset_clut,
                      offset_acurve;
             int type = 0;

             offset_bcurve = oyGetTableUInt32_( &mem[12], 0, 0, 0 );
             offset_matrix = oyGetTableUInt32_( &mem[12], 0, 0, 1 );
             offset_mcurve = oyGetTableUInt32_( &mem[12], 0, 0, 2 );
             offset_clut = oyGetTableUInt32_( &mem[12], 0, 0, 3 );
             offset_acurve = oyGetTableUInt32_( &mem[12], 0, 0, 4 );

             /* B */
             if(offset_acurve == 0 &&
                offset_clut == 0 &&
                offset_mcurve == 0 &&
                offset_matrix == 0 &&
                offset_bcurve != 0)
             {
               type = 1;
               
               oyStringAddPrintf_( &tmp, AD, "%s: B",
                                   _("Type") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* M - Ma - B */
             if(offset_acurve == 0 &&
                offset_clut == 0 &&
                offset_mcurve != 0 &&
                offset_matrix != 0 &&
                offset_bcurve != 0)
             {
               type = 2;
               
               oyStringAddPrintf_( &tmp, AD, "%s: M - %s - B",
                                   _("Type"), _("Matrix") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* A - C - B */
             if(offset_acurve != 0 &&
                offset_clut != 0 &&
                offset_mcurve == 0 &&
                offset_matrix == 0 &&
                offset_bcurve != 0)
             {
               type = 3;
               
               oyStringAddPrintf_( &tmp, AD, "%s: A - %s - B",
                                   _("Type"), _("CLUT") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* A - C - M - Ma - B */
             if(offset_acurve != 0 &&
                offset_clut != 0 &&
                offset_mcurve != 0 &&
                offset_matrix != 0 &&
                offset_bcurve != 0)
             {
               type = 4;
               
               oyStringAddPrintf_( &tmp, AD, "%s: A - %s - M - %s - B",
                                   _("Type"), _("CLUT"), _("Matrix") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             {
               type = 0;
               
               oyStringAddPrintf_( &tmp, AD,"%s: A%s - %s%s - M%s - %s%s - B%s",
                                   _("Undefined"),
                                   offset_acurve?"*":"",
                                   _("CLUT"), offset_clut ? "*":"",
                                   offset_mcurve?"*":"",
                                   _("Matrix"), offset_matrix ? "*":"",
                                   offset_bcurve?"*":""
                                 );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             }

             oyStringAddPrintf_( &tmp, AD, "%s %s: %d",
                                 _("Input"), _("Channels"), channels_in );
             oyStructList_AddName( texts, tmp, -1);
             oyFree_m_( tmp );
             oyStringAddPrintf_( &tmp, AD, "%s %s: %d",
                                 _("Output"), _("Channels"), channels_out );
             oyStructList_AddName( texts, tmp, -1);
             oyFree_m_( tmp );

             error = oyWriteIcSigLutAtoBType( texts, channels_in, channels_out,
                                                    sig, mem,
                                                    offset_bcurve,
                                                    offset_matrix,
                                                    offset_mcurve,
                                                    offset_clut,
                                                    offset_acurve,
                                                    tag->size_ );

           }
           else
             oyStructList_AddName( texts, "unrecoverable parameters found", -1);

           break;
      case icSigLutBtoAType:
           error = tag->size_ < 32;
           count = *(icUInt8Number*)(mem+8);
           
           if(error <= 0)
           {
             uint8_t channels_in = *(icUInt8Number*)(mem+8);
             uint8_t channels_out = *(icUInt8Number*)(mem+9);
             uint32_t offset_bcurve, offset_matrix, offset_mcurve, offset_clut,
                      offset_acurve;
             int type = 0;

             offset_bcurve = oyGetTableUInt32_( &mem[12], 0, 0, 0 );
             offset_matrix = oyGetTableUInt32_( &mem[12], 0, 0, 1 );
             offset_mcurve = oyGetTableUInt32_( &mem[12], 0, 0, 2 );
             offset_clut = oyGetTableUInt32_( &mem[12], 0, 0, 3 );
             offset_acurve = oyGetTableUInt32_( &mem[12], 0, 0, 4 );

             /* B */
             if(offset_acurve == 0 &&
                offset_clut == 0 &&
                offset_mcurve == 0 &&
                offset_matrix == 0 &&
                offset_bcurve != 0)
             {
               type = 1;
               
               oyStringAddPrintf_( &tmp, AD, "%s: B",
                                   _("Type") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* B - Ma - M */
             if(offset_acurve == 0 &&
                offset_clut == 0 &&
                offset_mcurve != 0 &&
                offset_matrix != 0 &&
                offset_bcurve != 0)
             {
               type = 2;
               
               oyStringAddPrintf_( &tmp, AD, "%s: B - %s - M",
                                   _("Type"), _("Matrix") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* B - C - A */
             if(offset_acurve != 0 &&
                offset_clut != 0 &&
                offset_mcurve == 0 &&
                offset_matrix == 0 &&
                offset_bcurve != 0)
             {
               type = 3;
               
               oyStringAddPrintf_( &tmp, AD, "%s: B - %s - A",
                                   _("Type"), _("CLUT") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             /* B - Ma - M - C - A */
             if(offset_acurve != 0 &&
                offset_clut != 0 &&
                offset_mcurve != 0 &&
                offset_matrix != 0 &&
                offset_bcurve != 0)
             {
               type = 4;
               
               oyStringAddPrintf_( &tmp, AD, "%s: B - %s - M - %s - A",
                                   _("Type"), _("Matrix"), _("CLUT") );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             } else
             {
               type = 0;
               
               oyStringAddPrintf_( &tmp, AD,"%s: A%s - %s%s - M%s - %s%s - B%s",
                                   _("Undefined"),
                                   offset_acurve?"*":"",
                                   _("CLUT"), offset_clut ? "*":"",
                                   offset_mcurve?"*":"",
                                   _("Matrix"), offset_matrix ? "*":"",
                                   offset_bcurve?"*":""
                                 );
               oyStructList_AddName( texts, tmp, -1);
               oyFree_m_( tmp );
             }

             oyStringAddPrintf_( &tmp, AD, "%s %s: %d",
                                 _("Input"), _("Channels"), channels_in );
             oyStructList_AddName( texts, tmp, -1);
             oyFree_m_( tmp );
             oyStringAddPrintf_( &tmp, AD, "%s %s: %d",
                                 _("Output"), _("Channels"), channels_out );
             oyStructList_AddName( texts, tmp, -1);
             oyFree_m_( tmp );

             error = oyWriteIcSigLutAtoBType( texts, channels_in, channels_out,
                                                    sig, mem,
                                                    offset_bcurve,
                                                    offset_matrix,
                                                    offset_mcurve,
                                                    offset_clut,
                                                    offset_acurve,
                                                    tag->size_ );

           }
           else
             oyStructList_AddName( texts, "unrecoverable parameters found", -1);

           break;
      case icSigDeviceSettingsTag:

           len = tag->size_ * sizeof(char);
           tmp = oyAllocateFunc_( len );
           error = tag->size_ < 80 || !memcpy( tmp, &mem[8], len - 8 );
           /*  - icSigDeviceSettingsType:
            *    - since Oyranos 0.1.10 (API 0.1.10)
            *    - returns */
           /*      - 0: first string version */
           if(!error)
           {
             oySprintf_( tmp, "%s", _("Tag Version:") );
             oyStructList_AddName( texts, tmp, -1 );

             oySprintf_( tmp, "%d", (int)((uint8_t) mem[8]) );
             error = (char) mem[8] != 1;
           }
           if(!error)
             oyStructList_AddName( texts, tmp, -1 );
           /*      - 1: device serial */
           if(!error)
           {
             oySprintf_( tmp, "%s", _("Device Serial:") );
             oyStructList_AddName( texts, tmp, -1 );

             error = !memcpy( tmp, &mem[9], 12 );
           }
           tmp[12] = 0;
           if(!error)
             oyStructList_AddName( texts, tmp, -1 );
           /*      - 2: driver name */
           if(!error)
           {
             oySprintf_( tmp, "%s", _("Driver name:") );
             oyStructList_AddName( texts, tmp, -1 );

             error = !memcpy( tmp, &mem[21], 12 );
           }
           tmp[12] = 0;
           if(!error)
             oyStructList_AddName( texts, tmp, -1 );
           /*      - 3: driver version */
           if(!error)
           {
             oySprintf_( tmp, "%s", _("Driver version:") );
             oyStructList_AddName( texts, tmp, -1 );

             error = !memcpy( tmp, &mem[33], 12 );
           }
           tmp[12] = 0;
           if(!error)
             oyStructList_AddName( texts, tmp, -1 );
           /*      - 4: driver signature/encoding */
           if(!error)
           {
             oySprintf_( tmp, "%s", _("Driver signature/encoding:") );
             oyStructList_AddName( texts, tmp, -1 );

             error = !memcpy( tmp, &mem[45], 12 );
           }
           tmp[12] = 0;
           if(!error)
             oyStructList_AddName( texts, tmp, -1 );
           /*      - 5: priority (0-255) */
           {
             uint32_t i = 0;
             deviceSettingsType * DevS = (deviceSettingsType*) mem;
             if(!error && DevS->data_size > 0)
             {
               oySprintf_( tmp, "%s", _("Priority:") );
               oyStructList_AddName( texts, tmp, -1 );

               i = *(uint8_t*)&mem[57];
               oySprintf_( tmp, "%d", (int)i );
             }
             if(!error)
               oyStructList_AddName( texts, tmp, -1 );
           /*      - 6: oyBlob_s data blob */
             i = 0;
             if(!error)
             {
               i = oyValueUInt32( (uint32_t)*((uint32_t*) &mem[80]));
               oySprintf_( tmp, "%d", (int)i );
             }
             if(!error && i && i + 84 <= len)
             {
               oySprintf_( tmp, "%s", _("Data:") );
               oyStructList_AddName( texts, tmp, -1 );

               o = oyBlob_New( 0 );
               oyBlob_SetFromData( o, &mem[84], i, mem );
               oyStructList_MoveIn( texts, (oyStruct_s**)&o, -1, 0 );
             }
             oyFree_m_( tmp );
           }
           break;
      case icSigTextType:

           len = tag->size_ * sizeof(char);
           tmp = oyAllocateFunc_( len );
           error = !memcpy( tmp, &mem[8], len - 8 );

           while (strchr(tmp, 13) > (char*)0) { /* \r 013 0x0d */
             pos = strchr(tmp, 13);
             if (pos > (char*)0) {
               if (*(pos+1) == '\n')
                 *pos = ' ';
               else
                 *pos = '\n';
             }
             count++;
           };
           if(!error)
           {
             oyStructList_MoveInName( texts, &tmp, -1 );
             size_ = len;
           }
           break;
      case icSigParametricCurveType:

           if (tag->size_ < 12)
           { return texts; }

           if(error <= 0)
           {
             uint16_t type = oyValueUInt16( (uint16_t)*((uint16_t*)&mem[8]) ); 
             double params[7]; /* Y(gamma),a,b,c,d,e,f */
             int i,
                 params_n = 0;
             int segments_n = 256;

             opt = oyOption_FromRegistration( "////icParametricCurveType", 0 );
             if(type == 0) /* gamma */
             {
               if(tag->size_ < 16) return texts;

               oyOption_SetFromDouble( opt, type, 0, 0 );
               params_n = 1;
             } else if(type == 1)
             {
               if(tag->size_ < 24) return texts;

               oyOption_SetFromDouble( opt, type, 0, 0 );
               params_n = 3;
             } else if(type == 2)
             {
               if(tag->size_ < 28) return texts;

               oyOption_SetFromDouble( opt, type, 0, 0 );
               params_n = 4;
             } else if(type == 3)
             {
               if(tag->size_ < 32) return texts;

               oyOption_SetFromDouble( opt, type, 0, 0 );
               params_n = 5;
             } else if(type == 4)
             {
               if(tag->size_ < 40) return texts;

               oyOption_SetFromDouble( opt, type, 0, 0 );
               params_n = 7;
             }

             oyOption_SetFromDouble( opt, params_n, 1, 0 );
             for(i = 0; i < params_n; ++i)
             {
                 params[i] = oyValueInt32( (uint32_t)*((uint32_t*)&mem[12+i*4]))
                                           / 65536.0;
                 oyOption_SetFromDouble( opt, params[i], 2+i, 0 );
             }

             oyStringAddPrintf_( &tmp, AD, "%s %d\n", _("parametric function type"), type);
             if(type == 0) /* gamma */
             {
               oyStringAddPrintf_( &tmp, AD, "%s\n%s=%g",
                                   _("f(X)=X^gamma"),
                                   _("gamma"),
                                   params[0] );
             } else if(type == 1)
             {
               oyStringAddPrintf_( &tmp,AD, "%s\n%s\n%s=%g a=%g b=%g",
                                   _("f(X)=(a*X+b)^gamma for X>=-b/a"),
                                   _("f(X)=0 for (X<-b/a)"),
                                   _("gamma"),
                                   params[0], params[1], params[2] );
             } else if(type == 2)
             {
               oyStringAddPrintf_( &tmp,AD, "%s\n%s\n%s=%g a=%g b=%g c=%g",
                                   _("f(X)=(a*X+b)^gamma + c for X>=-b/a"),
                                   _("f(X)=c for (X<-b/a)"),
                                   _("gamma"),
                                   params[0], params[1], params[2], params[3] );
             } else if(type == 3)
             {
               oyStringAddPrintf_( &tmp,AD, "%s\n%s\n%s=%g a=%g b=%g c=%g d=%g",
                                   _("f(X)=(a*X+b)^gamma for X>=d"),
                                   _("f(X)=c*X for (X<d)"),
                                   _("gamma"),
                                   params[0], params[1], params[2], params[3],
                                   params[4] );
             } else if(type == 4)
             {
               oyStringAddPrintf_( &tmp,AD, "%s\n%s\n%s=%g a=%g b=%g c=%g d=%g"
                                   " e=%g f=%g",
                                   _("f(X)=(a*X+b)^gamma + e for X>=d"),
                                   _("f(X)=c*X+f for (X<d)"),
                                   _("gamma"),
                                   params[0], params[1], params[2], params[3],
                                   params[4], params[5], params[6] );
             }

             oyOption_SetFromDouble( opt, segments_n, 2+params_n, 0 );
             oyICCparametricCurveToSegments( opt, opt, 2 + params_n + 1,
                                             segments_n, 0.0, 1.0 );

             if(!tmp)
               oyStringAddPrintf_(&tmp, AD, "%s %d", "parametric curve", type );
             oyStructList_AddName( texts, tmp, -1 );
             oyStructList_MoveIn( texts, (oyStruct_s**)&opt, -1, 0 );

             if(tmp) oyFree_m_(tmp);
           }

           break;
      case icSigWCSProfileTag:
           len = tag->size_ * sizeof(char);
           tmp = oyAllocateFunc_( len*2 + 1 );

           {
                 int  offset = 8 + 24;

                 len = len - offset;

                 if(!error)
                 {
                   /* WCS provides UTF-16LE */
                   error = oyIMIconv( &mem[offset], len, tmp, "UTF-16LE" );

                   if(error != 0 || !oyStrlen_(tmp))
                   {
                     error = 1;
                     oyDeAllocateFunc_(tmp); tmp = 0;
                   }
                 }
           }

           if(!error)
           while (strchr(tmp, 13) > (char*)0) { /* \r 013 0x0d */
             pos = strchr(tmp, 13);
             if (pos > (char*)0) {
               if (*(pos+1) == '\n')
                 *pos = ' ';
               else
                 *pos = '\n';
             }
             count++;
           };

           if(!error)
           {
             size_ = oyStrlen_(tmp);
             oyStructList_MoveInName( texts, &tmp, -1 );
           }
           break;
      case icSigTextDescriptionType:
           count = *(icUInt32Number*)(mem+8);
           count = oyValueUInt32( count );

           if((int)count > tag->size_- 20)
           {
             int diff = count - tag->size_ - 20;
             char nt[128];
             char * txt = oyAllocateFunc_( tag->size_ );
             snprintf( nt, 128, "%d", diff );

             oyStructList_AddName( texts,
                                   _("Error in ICC profile tag found!"),
                                   -1 );
             
             oyStructList_AddName( texts,
                                 " Wrong \"desc\" tag count. Difference is :",
                                   -1 );
             STRING_ADD( tmp, "   " );
             STRING_ADD( tmp, nt );
             oyStructList_MoveInName( texts, &tmp, -1 );
             oyStructList_AddName( texts,
                                 " Try ordinary tag length instead (?):",
                                   -1 );
             
             STRING_ADD( tmp, "  " );
             error = !memcpy (txt, &mem[12], tag->size_ - 12);
             txt[ tag->size_ - 12 ] = 0;
             STRING_ADD( tmp, txt );
             oyStructList_MoveInName( texts, &tmp, -1 );
           }
           else
           {
             tmp = oyAllocateFunc_(count + 1);
             memset(tmp, 0, count + 1);
             error = !memcpy(tmp, mem+12, count);
             tmp[count] = 0;
             oyStructList_MoveInName( texts, &tmp, -1 );
           }

           {
             uint32_t off = 0, n_ascii = 0, n_uni16 = 0;

               /* 'desc' type */
               off += 8;

               /* ascii in 'desc' */
               if(off < tag->size_)
               {
                 len = *(uint32_t*)&mem[off];
                 n_ascii = oyValueUInt32( len );

                 off += 4;
                 off += n_ascii;
                 /*off += (off%4 ? 4 - off%4 : 0);*/
               }

               /* unicode section in 'desc' */
               if(off < tag->size_)
               {
                 off += 4;

                 len = *(icUInt32Number*)&mem[off];
                 n_uni16 = oyValueUInt32( len );
                 off += 4 + n_uni16*2 - 1;
               }
               /* script in 'desc' */
               if(off < tag->size_)
               {
                 len = *(icUInt32Number*)&mem[off];
                 len = oyValueUInt32( len );
                 off += 4 + 67;
               }
             tag->size_check_ = off;
           }
           break;
      case icSigMultiLocalizedUnicodeType:
           {
             int count = oyValueUInt32( *(icUInt32Number*)&mem[8] );
             int size = oyValueUInt32( *(icUInt32Number*)&mem[12] ); /* 12 */
             int i;
             int all = 1;

             error = tag->size_ < 24 + count * size;

             if(!error)
             for (i = 0; i < count; i++)
             {
               char lang[4] = {0,0,0,0};
               int  g = 0,
                    offset = 0;

               error = tag->size_ < 20 + i * size;
               if(!error)
                 g = oyValueUInt32( *(icUInt32Number*)&mem[20+ i*size] );

               lang[0] = mem[16+ i*size];
               lang[1] = mem[17+ i*size];

               {
                 oyName_s * name = 0;
                 oyStruct_s * oy_struct = 0;
                 char * t = 0;

                 error = tag->size_ < 20 + i * size + g + 4;
                 if(!error)
                 {
                   len = (g > 1) ? g : 8;
                   t = (char*) oyAllocateFunc_(len*4);
                   error = !t;
                 }

                 if(!error && all)
                 {
                   name = oyName_new(0);
                   oySprintf_( name->lang, "%s_%c%c", lang,
                               mem[18+ i*size], mem[19+ i*size] );
                 }

                 if(!error)
                   t[0] = 0;

                 if(!error)
                   error = (24 + i*size + 4) > tag->size_;

                 if(!error)
                   offset = oyValueUInt32( *(icUInt32Number*)&mem
                                                  [24+ i*size] );

                 if(!error)
                   error = offset + len > tag->size_;

                 if(!error)
                 {
                   /* ICC says UTF-16BE */
                   error = oyIMIconv( &mem[offset], len, t, "UTF-16BE" );

                   if(!error)
                     oy_struct = (oyStruct_s*) name;
                   /* eigther text or we have a non translatable string */
                   if(!error && (oyStrlen_(t) || oyStructList_Count(texts)))
                   {
                     name->name = t;
                     oyStructList_MoveIn( texts, &oy_struct, -1, 0 );
                   } else
                     name->release(&oy_struct);
                 }
               }

               if(i == count-1 && !error)
               {
                 if(!error)
                   error = (24 + i*size + 4) > tag->size_;

                 offset = oyValueUInt32( *(icUInt32Number*)&mem
                                                  [24+ i*size] );
                 size_ = offset + g;
               }
             }

             if (!oyStructList_Count(texts)) /* first entry */
             {
               int g =        oyValueUInt32(*(icUInt32Number*)&mem[20]),
                   offset = oyValueUInt32(*(icUInt32Number*)&mem[24]);
               char * t = 0;
               int n_;

               error = tag->size_ < offset + g;

               if(!error)
                 t = (char*) oyAllocateFunc_( g + 1 );
               error = !t;

               if(!error)
               {
                 for (n_ = 1; n_ < g ; n_ = n_+2)
                   t[n_/2] = mem[offset + n_];
                 t[n_/2] = 0;
                 STRING_ADD( tmp, t );
                 oyStructList_MoveInName( texts, &tmp, -1 );
                 oyFree_m_( t );
               }
             }
           }

           break;
      case icSigSignatureType:
           if (tag->size_ < 12)
           { return texts; }
           else
           {
             icTechnologySignature tech;
             const char * t =  0;

             error = !memcpy (&tech, &mem[8] , 4);
             tech = oyValueUInt32( tech );
             t = oyICCTechnologyDescription( tech );

             size_ = 8 + 4;

             tmp = oyAllocateFunc_(5);
             error = !memcpy (tmp, &mem[8] , 4);
             tmp[4] = 0;
             oyStructList_MoveInName( texts, &tmp, -1 );
             tmp = oyStringAppend_( 0, t, oyAllocateFunc_ );
             oyStructList_MoveInName( texts, &tmp, -1 );
           }
           break;
      case icSigMakeAndModelType:
           if(tag->size_ < 40)
           { return texts; }
           else
           {
             uint32_t val = 0, i;

             for(i = 0; i < 4; ++i)
             {
               val = oyValueUInt32( (uint32_t)*((uint32_t*)&mem[8 + i*4]) );
               oySprintf_(num, "%u", val);
               if(i==0)
                 oyStructList_AddName( texts, _("Manufacturer:"), -1 );
               if(i==1)
                 oyStructList_AddName( texts, _("Model:"), -1 );
               if(i==2)
                 oyStructList_AddName( texts, _("Serial:"), -1 );
               if(i==3)
                 oyStructList_AddName( texts, _("Date:"), -1 );
               oyStructList_AddName( texts, num, -1 );
             }
             size_ = 8 + 32;
           }
           break;
      case icSigNativeDisplayInfoType:
           if(tag->size_ < 56)
           { return texts; }
           else
           {
             uint32_t val = 0, i, tag_size;
             double dval[2];

             tag_size = val = oyValueUInt32( (uint32_t)*((uint32_t*)&mem[8]) );
             oySprintf_(num, "%u", val);
             oyStructList_AddName( texts, _("Size:"), -1 );
             oyStructList_AddName( texts, num, -1 );
             
             /* primaries */
             for(i = 0; i < 4; ++i)
             {
               val = oyValueUInt32( (uint32_t)*((uint32_t*)&mem[12 + i*2*4]) );
               dval[0] = val/65536.0;
               val = oyValueUInt32( (uint32_t)*((uint32_t*)&mem[12 + i*2*4+4]));
               dval[1] = val/65536.0;
               oySprintf_(num, "%.03g %.03g", dval[0], dval[1]);
               if(i==0)
                 oyStructList_AddName( texts, _("Red xy:"), -1 );
               if(i==1)
                 oyStructList_AddName( texts, _("Green xy:"), -1 );
               if(i==2)
                 oyStructList_AddName( texts, _("Blue xy:"), -1 );
               if(i==3)
                 oyStructList_AddName( texts, _("White xy:"), -1 );
               oyStructList_AddName( texts, num, -1 );
             }
             /* gamma value */
             for(i = 0; i < 3; ++i)
             {
               val = oyValueUInt32( (uint32_t)*((uint32_t*)&mem[44 + i*4]) );
               dval[0] = val/65536.0;
               oySprintf_(num, "%.03g", dval[0]);
               if(i==0)
                 oyStructList_AddName( texts, _("Red Gamma:"), -1 );
               if(i==1)
                 oyStructList_AddName( texts, _("Green Gamma:"), -1 );
               if(i==2)
                 oyStructList_AddName( texts, _("Blue Gamma:"), -1 );
               oyStructList_AddName( texts, num, -1 );
             }
             size_ = 8 + 56;
             /* gamma table */
             if(tag_size >= 62 && tag->size_ >= 62)
             {
               for(i = 0; i < 3; ++i)
               {
                 val = oyValueUInt16( (uint16_t)*((uint16_t*)&mem[56 + i*2]) );
                 oySprintf_(num, "%d", val);
                 if(i==0)
                   oyStructList_AddName( texts, _("Curve Channels:"), -1 );
                 if(i==1)
                   oyStructList_AddName( texts, _("Curve Segments:"), -1 );
                 if(i==2)
                   oyStructList_AddName( texts, _("Curve Precission:"), -1 );
                 oyStructList_AddName( texts, num, -1 );
               }
               size_ += 6;
             }
           }
           break;
      case icSigProfileSequenceDescType:
           if(tag->size_ > 12 + 20 + sizeof(icTextDescription)*2)
           {
             int off = 8;
             uint32_t i=0;
             icDescStruct * desc = 0;
             char mfg_local[5] = {0,0,0,0,0},
                  model_local[5] = {0,0,0,0,0};
             const char * mfg = 0;
             const char * model = 0;
             const char * tech = 0;
             oyStructList_s * mfg_tmp = 0, * model_tmp = 0;
             int32_t size = -1;

             count = *(icUInt32Number*)(mem+off);
             count = oyValueUInt32( count );
             off += 4;
#if 0
             len = *(icUInt32Number*)(mem+off);
             len = oyValueUInt32( len );
             off += 4;
#endif

             oySprintf_(num, "%d", count);
             STRING_ADD( tmp, num );
             oyStructList_MoveInName( texts, &tmp, -1 );

             if(count > 256) count = 256;
             for(i = 0; i < count; ++i)
             {
               if(tag->size_ > off + sizeof(icDescStruct))
                 desc = (icDescStruct*) &mem[off];

               off += 4+4+2*4+4;
               if(off < tag->size_)
               {

                 oySprintf_(num, "%d", i);
                 STRING_ADD( tmp, "profile[" );
                 STRING_ADD( tmp, num );
                 STRING_ADD( tmp, "]:" );
                 oyStructList_MoveInName( texts, &tmp, -1 );

                 mfg = oyICCTagName( oyValueUInt32(desc->deviceMfg) );
                 memcpy( mfg_local, mfg, 4 );
                 mfg = mfg_local;
                 model = oyICCTagName( oyValueUInt32(desc->deviceModel) );
                 memcpy( model_local, model, 4 );
                 model = model_local;
                 tech = oyICCTechnologyDescription( oyValueUInt32(desc->technology ));
               }

               /* first mnf */
               tmptag = oyProfileTag_New(0);
               tmp = oyAllocateFunc_(tag->size_ - off);
               error = !memcpy(tmp, &mem[off], tag->size_ - off);
               tag_sig = *(icUInt32Number*)(tmp);
               tag_sig = oyValueUInt32( tag_sig );
               oyProfileTag_Set( tmptag, icSigDeviceMfgDescTag,
                                         tag_sig, oyOK,
                                         tag->size_ - off, tmp );
               mfg_tmp = oyIMProfileTag_GetValues( tmptag );
               if(oyStructList_Count( mfg_tmp ) )
               {
                 name = 0;
                 name = (oyName_s*) oyStructList_GetRefType( mfg_tmp,
                                                   0, oyOBJECT_NAME_S );
                 if(name &&  name->name && name->name[0] )
                   mfg = name->name;
               }
               size = tmptag->size_check_;
               oyProfileTag_Release( &tmptag );
               tmp = 0;

               if(size > 0)
                 off += size;

               /* next model */
               tmptag = oyProfileTag_New(0);
               tmp = oyAllocateFunc_(tag->size_ - off);
               error = !memcpy(tmp, &mem[off], tag->size_ - off);
               tag_sig = *(icUInt32Number*)(tmp);
               tag_sig = oyValueUInt32( tag_sig );
               oyProfileTag_Set( tmptag, icSigDeviceModelDescTag,
                                         tag_sig, oyOK,
                                         tag->size_ - off, tmp );
               model_tmp = oyIMProfileTag_GetValues( tmptag );
               if(oyStructList_Count( model_tmp ) )
               {
                 name = 0;
                 name = (oyName_s*) oyStructList_GetRefType( model_tmp,
                                                   0, oyOBJECT_NAME_S );
                 if(name &&  name->name && name->name[0] )
                   model = name->name;
               }
               size = tmptag->size_check_;
               oyProfileTag_Release( &tmptag );
               tmp = 0;

               if(size > 0)
                 off += size;

               /* write to string */
               oyStructList_AddName( texts, _("Manufacturer:"), -1 );
               if(mfg && oyStrlen_(mfg))
                 oyStructList_AddName( texts, mfg, -1 );
               else
                 oyStructList_AddName( texts, 0, -1 );
               oyStructList_AddName( texts, _("Modell:"), -1 );
               if(model && oyStrlen_(model))
                 oyStructList_AddName( texts, model, -1 );
               else
                 oyStructList_AddName( texts, 0, -1 );
               oyStructList_AddName( texts, _("Technology:"), -1 );
               if(tech && oyStrlen_(tech))
                 oyStructList_AddName( texts, tech, -1 );
               else
                 oyStructList_AddName( texts, 0, -1 );

               oyStructList_Release( &mfg_tmp );
               oyStructList_Release( &model_tmp );
             }
             size_ = off;
           }
           break;
      case icSigProfileSequenceIdentifierType:
           /*
                ICC Votable Proposal Submission
                Profile Sequence Identifier Tag

		Proposer: Manish Kulkarni, Adobe Systems Inc.
		Date: November 27, 2006
		Proposal Version: 1.0
            */
           if(tag->size_ > 12)
           {
             int32_t off = 0;
             int i;
             int offset = 0, old_offset = 0;
             int size = 0;
             uint32_t * hash = 0;

             mluc_size = 0;
             desc_tmp = 0;
             desc_tmp_n = 0;
             tmptag = 0;

             off += 8;

             count = *(icUInt32Number*)(mem+off);
             count = oyValueUInt32( count );
             off += 4;

             if(count > 256) count = 256;

             oySprintf_(num, "%d", count);
             oyStructList_AddName( texts, num, -1 );

             for(i = 0; i < count; ++i)
             {
               oySprintf_(num, "%d", i);
               STRING_ADD( tmp, "profile[" );
               STRING_ADD( tmp, num );
               STRING_ADD( tmp, "]:" );
               oyStructList_MoveInName( texts, &tmp, -1 );

               if(!error && 12 + i*8 + 8 < tag->size_)
               {
                 /* implicite offset and size */
                 len = *(icUInt32Number*)&mem[12 + i*8 + 0];
                 offset = oyValueUInt32( len );
                 len = *(icUInt32Number*)&mem[12 + i*8 + 4];
                 size = oyValueUInt32( len );
               }

               if(!error)
                 error = offset + size < old_offset + 16 + mluc_size;

               if(!error && offset + size <= tag->size_)
               {
                 hash = (uint32_t*)&mem[offset];
                 tmp = oyAllocateFunc_(80);
                 error = !tmp;
                 oySprintf_(tmp, "%x%x%x%x",hash[0], hash[1], hash[2], hash[3]);
                 oyStructList_AddName( texts, "md5id:", -1 );
                 oyStructList_MoveInName( texts, &tmp, -1 );

                 old_offset = offset;

                 offset += 16;

                 /* 'mluc' type - desc */
                 memcpy( &sig,  &mem[offset], 4 );
                 sig = oyValueUInt32( sig );
                 if(sig != icSigMultiLocalizedUnicodeType)
                   oyIM_msg( oyMSG_WARN,0, OY_DBG_FORMAT_"\n"
                   "psid description not of icSigMultiLocalizedUnicodeType: %d",
                            OY_DBG_ARGS_, i );
                 else
                 {
                   tmptag = oyProfileTag_New(0);
                   tmp = oyAllocateFunc_(tag->size_ - offset);
                   error = !memcpy(tmp, &mem[offset], tag->size_ - offset);
                   oyProfileTag_Set( tmptag, icSigProfileDescriptionTag,
                                           icSigMultiLocalizedUnicodeType, oyOK,
                                           tag->size_ - offset, tmp );
                   tmp = 0;
                   desc_tmp_n = 0;
                   desc_tmp = oyIMProfileTag_GetValues( tmptag );
                   if(oyStructList_Count( desc_tmp ) )
                   {
                     name = 0;
                     name = (oyName_s*) oyStructList_GetRefType( desc_tmp,
                                                     0, oyOBJECT_NAME_S );
                     if(name)
                       tmp = name->name;
                   }
                   oyProfileTag_Release( &tmptag );
                 }

                 if(size_ < offset + mluc_size)
                   size_ = offset + mluc_size;

                 if(!error)
                   error = size < mluc_size;

                 oyStructList_AddName( texts,
                      oyICCTagDescription(icSigMultiLocalizedUnicodeType), -1 );
                 if(tmp)
                   oyStructList_MoveInName( texts, &tmp, -1 );
                 else
                   oyStructList_AddName( texts, OY_PROFILE_NONE, -1 );
               } else
                 error = 1;
             }
           }
           break;
    }
  }

  values = texts;

  return values;
}


/** @func  oyIMProfileTag_Create
 *  @brief create a ICC profile tag
 *
 *  This is a module function. For usage in Oyranos 
 *  @see oyProfileTag_Create
 *
 *  The output depends on the tag type signature and arguments in list:
 *
 *  - icSigProfileSequenceIdentifierType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - list: should contain only profiles
 *    - version: is not honoured; note 'psid' is known after ICC v4.2
 *  - icSigMultiLocalizedUnicodeType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - list: should contain only names in oyName_s objects
 *      - oyName_s::name is considered to hold the name
 *      - oyName_s::lang is required to hold i18n specifier, e.g. "en_GB"
 *      - the frist oyName_s::lang can have no i18n specifier as a default
 *    - version: is not honoured; note 'mluc' is known since ICC v4
 *  - icSigTextType:
 *    - since Oyranos 0.1.8 (API 0.1.8)
 *    - list: should contain only names in oyName_s objects
 *      - oyName_s::name is considered to hold the name
 *
 *  - non supported types
 *    - the tag->status_ field will be set to oyUNDEFINED 
 *
 *  - function description
 *    - set the tag argument to zero
 *    - provide a empty list to fill in with oyName_s' each matching a tag_type
 *      - oyNAME_NICK contains the module info, e.g. 'oyIM'
 *      - oyNAME_NAME contains the tag_type, e.g. 'icSigMultiLocalizedUnicodeType' or 'mluc'
 *      - oyNAME_DESCRIPTION contains text as in above documentation
 *    - dont copy the list as content may be statically allocated
 *
 *  @param[in,out] tag                 the profile tag
 *  @param[in,out] list                parameters
 *  @param[in]     tag_type            the ICC tag type
 *  @param[in]     version             version as supported
 *  @return                            oySTATUS_e status
 *
 *  @version Oyranos: 0.1.8
 *  @since   2008/01/08 (Oyranos: 0.1.8)
 *  @date    2008/03/11
 */
int          oyIMProfileTag_Create   ( oyProfileTag_s    * tag,
                                       oyStructList_s    * list,
                                       icTagTypeSignature  tag_type,
                                       uint32_t            version )
{
  oyProfileTag_s * s = tag,
                 * tmptag = 0;
  int error = !list;
  int n = oyStructList_Count( list ),
      i = 0, mem_len = 0, tmp_len = 0, mluc_len = 0, mluc_sum = 0,
      len = 0, j = 0;
  char * mem = 0,
       * tmp = 0;
  oyProfile_s * prof = 0;
  oyStructList_s * tmp_list = 0,
                 * tag_list = 0;
  oyName_s * string = 0;

  /* provide information about the function */
  if(!error && !s)
  {
    oyName_s description_mluc = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "mluc",
      "\
- icSigMultiLocalizedUnicodeType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only names in oyName_s objects\
    - oyName_s::name is considered to hold the name\
    - oyName_s::lang is required to hold i18n specifier, e.g. \"en_GB\"\
    - the frist oyName_s::lang can have no i18n specifier as a default\
  - version: is not honoured; note 'mluc' is known since ICC v4"
    };
    oyName_s description_psid = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "psid",
      "\
- icSigProfileSequenceIdentifierType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyProfile_s\
  - version: is not honoured; note 'psid' is known after ICC v4.2"
    };
    oyName_s description_text = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "text",
      "\
- icSigTextType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyName_s\
  - version: is not honoured"
    };
    oyName_s description_desc = {
      oyOBJECT_NAME_S, 0,0,0,
      CMM_NICK,
      "desc",
      "\
- icSigTextDescriptionType:\
  - since Oyranos 0.1.8 (API 0.1.8)\
  - list: should contain only oyName_s\
  - version: is not honoured; note 'desc' is a pre ICC v4.0 tag"
    };
    oyStruct_s * description = 0;

    description = (oyStruct_s*) &description_mluc;
    error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_psid;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_text;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    description = (oyStruct_s*) &description_desc;
    if(!error)
      error = oyStructList_MoveIn( list, &description, -1, 0 );

    return error;
  }

  if(!error)
  switch((uint32_t)tag_type)
  {
    case icSigMultiLocalizedUnicodeType:
       {
         size_t size = 0;
         /*      base   #  size  lang len off */
         mluc_len = 8 + 4 + 4 + (2+2 + 4 + 4) * n;
         /*             8  12   16    20  24 */

         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
               tmp_len = strlen( string->name );
             error = !tmp_len;
             
             if(i)
               error = !string->lang;

             len = tmp_len * 2 + 4;
             mluc_len += len + (len%4 ? len%4 : 0);
           }
         }

         if(!error)
           oyStruct_AllocHelper_m_( mem, char, mluc_len, s, error = 1 );

         if(!error)
         {
           *((uint32_t*)&mem[8]) = oyValueUInt32( n );
           *((uint32_t*)&mem[12]) = oyValueUInt32( 12 );
           mem_len += 16 + n*12;
         }

         if(!error)
         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
               tmp_len = strlen( string->name );
             error = !tmp_len;

             if(i)
               error = !string->lang;
           }

           if(!error)
           {
               if(string->lang && oyStrlen_(string->lang))
               {
                 if(strlen(string->lang) >= 2)
                   memcpy( &mem[16+i*12 + 0], string->lang, 2 );
                 if(strlen(string->lang) > 4)
                   memcpy( &mem[16+i*12 + 2], &string->lang[3], 2 );
               }

               *((uint32_t*)&mem[16+i*12 + 4]) = oyValueUInt32( tmp_len * 2 );
               *((uint32_t*)&mem[16+i*12 + 8]) = oyValueUInt32( mem_len );
           }

           if(!error)
           {
#if 0
             /* broken with glibc-2.3.3 */
             size = mbstowcs( (wchar_t*)&mem[mem_len], string->name,
                              tmp_len );
#else
             size = tmp_len;
             for(j = 0; j < tmp_len; ++j)
               mem[mem_len+2*j+1] = string->name[j];
#endif

             error = (size != tmp_len);

             if(!error)
             {
               len = tmp_len * 2 + 4;
               mem_len += len + (len%4 ? len%4 : 0);
             }
           }
         }

         if(error || !n)
         {
           s->status_ = oyUNDEFINED;

         } else {
           oyProfileTag_Set( s, s->use, tag_type, oyOK, mem_len, mem );
         }
       }

       break;

    case icSigProfileSequenceIdentifierType:
       {
         tag_list = oyStructList_New( 0 );

         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             prof = (oyProfile_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_PROFILE_S );
             error = !prof;
           }

           if(!error)
           {
             tmptag = oyProfile_GetTagById( prof, icSigProfileDescriptionTag );
             error = !tmptag;

             oyProfile_Release( &prof );
           }

           if(!error && tmptag->tag_type_ != icSigMultiLocalizedUnicodeType)
           {
             mluc_len = 0;
             tmp_list = oyIMProfileTag_GetValues( tmptag );

             if(!error)
             {
               error = oyIMProfileTag_Create( tmptag, tmp_list,
                                            icSigMultiLocalizedUnicodeType, 0 );
               tmp = 0;

               if(!error)
                 error = tmptag->status_;
             }
           }

           if(!error)
           {
             mluc_sum += tmptag->size_;
             error = oyStructList_MoveIn( tag_list, (oyStruct_s**)&tmptag, -1, 0 );
           }
         }

         if(!error)
         {
           mem_len = 12 + 8*n + 16*n + mluc_sum + 3*n;
           oyStruct_AllocHelper_m_( mem, char, mem_len, tag, error = 1 );

           if(!error)
           oyProfileTag_Set( s, icSigProfileSequenceIdentifierType,
                                icSigProfileSequenceIdentifierType, oyOK,
                                mem_len, mem );

           tmp_len = 0;

           for(i = 0; i < n; ++i)
           {
             if(!error)
             {
               tmptag = (oyProfileTag_s*) oyStructList_GetRefType( tag_list,
                                               i, oyOBJECT_PROFILE_TAG_S );
               error = !tmptag;
             }

             if(!error)
             {
               int pos = 12 + 8*n + tmp_len;
               error = !memcpy( &mem[pos + 16],
                                tmptag->block_, tmptag->size_ );
               *((uint32_t*)&mem[12 + 8*i + 0]) = oyValueUInt32( pos );

               prof = (oyProfile_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_PROFILE_S );
               error = !prof || !prof->block_ || !prof->size_;
               error = oyProfileGetMD5( prof->block_, prof->size_,
                                        (unsigned char*)&mem[pos] );
               oyProfile_Release( &prof );

               len = 16 + tmptag->size_;
               *((uint32_t*)&mem[12 + 8*i + 4]) = oyValueUInt32( len );
               tmp_len += len + (len%4 ? len%4 : 0);
             }
             oyProfileTag_Release( &tmptag );
           }

           if(!error)
             *((uint32_t*)&mem[8]) = oyValueUInt32( n );
         }
       }
       break;

    case icSigTextType:
       {
         mem_len = 8;

         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
               mem_len += strlen( string->name ) + 1;
             error = !mem_len;
             
             len = mem_len;
             mem_len = len + (len%4 ? len%4 : 0);
           }
         }

         if(!error)
           oyStruct_AllocHelper_m_( mem, char, mem_len, s, error = 1 );

         if(!error)
         {
           mem[0] = 0;
           mem_len = 8;
         }

         if(!error)
         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
               tmp_len = strlen( string->name );
             error = !tmp_len;
           }

           if(!error)
           {
             if(i)
               mem[mem_len++] = '\n';

             tmp = &mem[mem_len];
             error = !memcpy( tmp, string->name, tmp_len );
             mem_len += tmp_len;
             if(!error)
               mem[mem_len] = 0;
           }
         }

         if(error || !n)
         {
           s->status_ = oyUNDEFINED;

         } else {
           len = mem_len + 1;
           mem_len = len + (len%4 ? len%4 : 0);
           oyProfileTag_Set( s, s->use, tag_type, oyOK, mem_len, mem );
         }
       }

       break;

    case icSigTextDescriptionType:
       {
         mem_len = 8 + 4;
         tmp_len = 0;

         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
             {
               mem_len += strlen( string->name ) + 1;
               tmp_len += strlen( string->name ) + 1;
             }
             error = !mem_len;
           }
         }

         /* we are guessing here */
         len = mem_len + 8 + 8 + 67;
         len = len + (len%4 ? len%4 : 0);

         if(!error)
           oyStruct_AllocHelper_m_( mem, char, len, s, error = 1 );

         if(!error)
         {
           memset( mem, 0, mem_len );

           *((uint32_t*)&mem[8]) = oyValueUInt32( tmp_len );

           mem_len = 8 + 4;
         }

         if(!error)
         for(i = 0; i < n; ++i)
         {
           if(!error)
           {
             string = (oyName_s*) oyStructList_GetRefType( list,
                                                   i, oyOBJECT_NAME_S );
             error = !string;
           }

           if(!error)
           {
             if(string->name)
               tmp_len = strlen( string->name );
             error = !tmp_len;
           }

           if(!error)
           {
             if(i)
               mem[mem_len++] = '\n';

             tmp = &mem[mem_len];
             error = !memcpy( tmp, string->name, tmp_len );
             mem_len += tmp_len;
             if(!error)
               mem[mem_len] = 0;
           }
         }

         if(error || !n)
         {
           s->status_ = oyUNDEFINED;

         } else {
           /*len = mem_len + 1;
           mem_len = len + (len%4 ? len%4 : 0);*/
           oyProfileTag_Set( s, s->use, tag_type, oyOK, len, mem );
         }
       }

       break;

    default:
       s->status_ = oyUNDEFINED;
       break;
  }

  if(s)
  {
    memcpy( s->last_cmm_, CMM_NICK, 4 );
    if(s->status_ == oyOK && s->block_)
      *((uint32_t*)&mem[0]) = oyValueUInt32( tag_type );
  }

  return error;
}


/** @instance oyIM_api3
 *  @brief    oyIM oyCMMapi3_s implementations
 *
 *  @version Oyranos: 0.1.8
 *  @since   2008/01/02 (Oyranos: 0.1.8)
 *  @date    2008/01/02
 */
oyCMMapi3_s  oyIM_api3 = {

  oyOBJECT_CMM_API3_S,
  0,0,0,
  (oyCMMapi_s*) & oyIM_api5_meta_c,
  
  oyIMCMMInit,
  oyIMCMMMessageFuncSet,

  CMM_BASE_REG CMM_NICK,     /**< registration */
  {OYRANOS_VERSION_A,OYRANOS_VERSION_B,OYRANOS_VERSION_C},/**< version[3] */
  {0,3,0},                  /**< int32_t module_api[3] */
  0,                         /**< char * id_ */

  oyIMProfileCanHandle,

  oyIMProfileTag_GetValues,
  oyIMProfileTag_Create
};

uint32_t oyGetTableUInt32_           ( const char        * mem,
                                       int                 entry_size,
                                       int                 entry_pos,
                                       int                 pos )
{
  uint32_t value = 0;
  memcpy( &value, &mem[entry_size * entry_pos + pos * 4], 4 );
  value = oyValueUInt32( value );
  return value;
}

uint16_t oyGetTableUInt16_           ( const char        * mem,
                                       int                 entry_size,
                                       int                 entry_pos,
                                       int                 pos )
{
  uint32_t value = 0;
  memcpy( &value, &mem[entry_size * entry_pos + pos * 2], 2 );
  value = oyValueUInt16( value );
  return value;
}

oyStructList_s *   oyStringsFrommluc ( const char        * mem,
                                       uint32_t            size )
{
  oyStructList_s * desc = 0;
  oyProfileTag_s * tmptag = 0;
  char * tmp = 0;
  oyName_s * name = 0;
  int error = 0;

  /* 'mluc' type - desc */
  tmptag = oyProfileTag_New(0);
  tmp = oyAllocateFunc_(size);
  error = !memcpy(tmp, mem, size);
  oyProfileTag_Set( tmptag, icSigProfileDescriptionTag,
                            icSigMultiLocalizedUnicodeType, oyOK,
                            size, tmp );
  tmp = 0;

  desc = oyIMProfileTag_GetValues( tmptag );
  oyProfileTag_Release( &tmptag );
  oyName_release( &name );

  return desc;
}
 
oyStructList_s *   oyCurveFromTag    ( char              * mem,
                                       size_t              size )
{
  oyProfileTag_s * tmptag;
  int error = (size <= 0);
  oyStructList_s * list = 0;
  icTagTypeSignature tag_sig;

  if(!error)
  {
    memcpy( &tag_sig, mem, 4 );
    tag_sig = oyValueUInt32( tag_sig );
    tmptag = oyProfileTag_CreateFromData( icSigGrayTRCTag, tag_sig, oyOK,
                                          size, mem, 0 );

    if(tag_sig == icSigCurveType ||
       tag_sig == icSigParametricCurveType)
      list = oyIMProfileTag_GetValues( tmptag );

    oyProfileTag_Release( &tmptag );
  }

  return list;
}

oyStructList_s *   oyCurvesFromTag   ( char              * mem,
                                       size_t              size,
                                       int                 count )
{
  oyStructList_s * list = oyStructList_New(0),
                 * data;
  int i;
  uint32_t curve_bytes = 0,
           curve_bytes_total = 0;
  oyOption_s * values;

  for(i = 0; i < count; ++i)
  {
    if(curve_bytes_total > size) break;

    data = oyCurveFromTag( &mem[curve_bytes_total], size );

    values = (oyOption_s*) oyStructList_GetRefType( data,
                                                    1, oyOBJECT_OPTION_S );
    if(oyFilterRegistrationMatchKey( oyOption_GetRegistration( values ),
                                   "////icParametricCurveType", oyOBJECT_NONE ))
      curve_bytes = 12 + oyOption_GetValueDouble( values, 1 ) * 4;
    else
      if(oyFilterRegistrationMatchKey( oyOption_GetRegistration( values ),
                                   "////icCurveType", oyOBJECT_NONE ))
      curve_bytes = 12 + oyOption_GetValueDouble( values, 0 ) * 2;
    oyOption_Release( &values );

    curve_bytes_total += curve_bytes;
    curve_bytes = 0;
    oyStructList_MoveIn( list, (oyStruct_s**)&data, -1, 0 );
  }

  return list;
}


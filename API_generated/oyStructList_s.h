/** @file oyStructList_s.h

   [Template file inheritance graph]
   +-> StructList_s.template.h
   |
   +-> Base_s.h
   |
   +-- Struct_s.template.h

 *  Oyranos is an open source Colour Management System
 *
 *  @par Copyright:
 *            2004-2011 (C) Kai-Uwe Behrmann
 *
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD - see: http://www.opensource.org/licenses/bsd-license.php
 *  @date     2011/05/18
 */



#ifndef OY_STRUCT_LIST_S_H
#define OY_STRUCT_LIST_S_H

#ifdef __cplusplus
/*namespace oyranos {*/
  extern "C" {
#endif /* __cplusplus */


  
#include <oyranos_object.h>



#include "oyStruct_s.h"

#include "oyObserver_s.h"


typedef struct oyStructList_s oyStructList_s;
/* Include "StructList.public.h" { */
#define OY_OBSERVE_AS_WELL 0x01

/* } Include "StructList.public.h" */


/* Include "StructList.dox" { */
/** @struct oyStructList_s
 *  @ingroup objects_generic
 *  @extends oyStruct_s
 *  @brief   a pointer list
 *  @internal
 *  
 *  Memory management is done by Oyranos' oyAllocateFunc_ and oyDeallocateFunc_.
 *  @note New templates will not be created automaticly [notemplates]
 *
 *  @version Oyranos: x.x.x
 *  @since Oyranos: version 0.1.8
 *  @date  november 2007 (API 0.1.8)
 *  @since   YYYY/MM/DD (Oyranos: x.x.x)
 *  @date    YYYY/MM/DD
 */

/* } Include "StructList.dox" */

struct oyStructList_s {
/* Include "Struct.members.h" { */
oyOBJECT_e           type_;          /**< @private struct type */
oyStruct_Copy_f      copy;           /**< copy function */
oyStruct_Release_f   release;        /**< release function */
oyObject_s           oy_;            /**< @private features name and hash */

/* } Include "Struct.members.h" */
};


OYAPI oyStructList_s* OYEXPORT
  oyStructList_New( oyObject_s object );
OYAPI oyStructList_s* OYEXPORT
  oyStructList_Copy( oyStructList_s *structlist, oyObject_s obj );
OYAPI int OYEXPORT
  oyStructList_Release( oyStructList_s **structlist );



/* Include "StructList.public_methods_declarations.h" { */
int              oyStructList_MoveIn ( oyStructList_s    * list,
                                       oyStruct_s       ** ptr,
                                       int                 pos,
                                       uint32_t            flags );
oyStruct_s *     oyStructList_GetRef ( oyStructList_s    * list,
                                       int                 pos );
oyStruct_s *     oyStructList_GetRefType( oyStructList_s * list,
                                       int                 pos,
                                       oyOBJECT_e          type );
int              oyStructList_ReleaseAt( oyStructList_s  * list,
                                       int                 pos );
int              oyStructList_Count  ( oyStructList_s    * list );
const char *     oyStructList_GetText( oyStructList_s    * s,
                                       oyNAME_e            name_type,
                                       int                 intent_spaces,
                                       uint32_t            flags );
const char *     oyStructList_GetID  ( oyStructList_s    * s,
                                       int                 intent_spaces,
                                       uint32_t            flags );
int              oyStructList_Clear  ( oyStructList_s    * s );
int              oyStructList_CopyFrom(oyStructList_s    * list,
                                       oyStructList_s    * from,
                                       oyObject_s          object );
int              oyStructList_MoveTo ( oyStructList_s    * s,
                                       int                 pos,
                                       int                 new_pos );
int              oyStructList_Sort   ( oyStructList_s    * s,
                                       int32_t           * rank_map );
int              oyStructList_ObserverAdd (
                                       oyStructList_s    * list,
                                       oyStruct_s        * observer,
                                       oyStruct_s        * user_data,
                                       oyObserver_Signal_f signalFunc );
int              oyStructList_MoveInName (
                                       oyStructList_s    * texts,
                                       char             ** text,
                                       int                 pos );
int              oyStructList_AddName( oyStructList_s    * texts,
                                       const char        * text,
                                       int pos );
const char *     oyStructList_GetName( oyStructList_s    * texts,
                                       int pos );

oyStructList_s * oyStructList_Create ( oyOBJECT_e          parent_type,
                                       const char        * list_name,
                                       oyObject_s          object );
oyStruct_s *     oyStructList_GetType( oyStructList_s    * list,
                                       int                 pos,
                                       oyOBJECT_e          type );
oyOBJECT_e       oyStructList_GetParentObjType (
                                       oyStructList_s    * list );

/* } Include "StructList.public_methods_declarations.h" */


#ifdef __cplusplus
} /* extern "C" */
/*}*/ /* namespace oyranos */
#endif /* __cplusplus */

#endif /* OY_STRUCT_LIST_S_H */

/** @file oyObject_s.c

   [Template file inheritance graph]
   +-- Object_s.template.c

 *  Oyranos is an open source Colour Management System
 *
 *  @par Copyright:
 *            2004-2011 (C) Kai-Uwe Behrmann
 *
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD - see: http://www.opensource.org/licenses/bsd-license.php
 *  @date     2011/05/30
 */


#include "oyranos_object_internal.h"

#include "oyObject_s.h"
#include "oyObject_s_.h"

#include "oyName_s_.h"


/* Include "Object.public_methods_definitions.c" { */
#include <stdint.h>           /* uint64_t uintptr_t */

static int oy_object_id_ = 0;
#if OY_USE_OBJECT_POOL_
static oyObject_s oy_object_pool_[100] = {
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,

0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0,
0,0,0,0,0, 0,0,0,0,0
};
#endif

/* oyObject common object Functions { */

/** @brief   object management 
 *  @memberof oyObject_s
 *
 *  @version Oyranos: 0.1.10
 *  @since   2007/11/00 (Oyranos: 0.1.8)
 *  @date    2009/06/02
 */
oyObject_s
oyObject_New  ( void )
{
  return oyObject_NewWithAllocators( oyAllocateFunc_, oyDeAllocateFunc_ );
}

/** @brief   object management 
 *  @memberof oyObject_s
 *
 *  @version Oyranos: 0.1.10
 *  @since   2007/11/00 (Oyranos: 0.1.8)
 *  @date    2009/06/02
 */
oyObject_s
oyObject_NewWithAllocators  ( oyAlloc_f         allocateFunc,
                              oyDeAlloc_f       deallocateFunc )
{
  oyObject_s o = 0;
  int error = 0;
  int len = sizeof(struct oyObject_s_);
#if OY_USE_OBJECT_POOL_
  int old_obj = 0, i = 0;

  for(i = 0; i < 100; ++i)
    if(oy_object_pool_[i] != 0)
    {
      old_obj = 1;
      o = oy_object_pool_[i];
      oy_object_pool_[i] = 0;
      break;
    }

  if(old_obj == 0)
#endif
    o = oyAllocateWrapFunc_( len, allocateFunc );

  if(!o) return 0;

#if OY_USE_OBJECT_POOL_
  if(old_obj == 0)
#endif
    error = !memset( o, 0, len );

  if(error)
    return NULL;
  
  o = oyObject_SetAllocators_( o, allocateFunc, deallocateFunc );
  o->copy = (oyStruct_Copy_f) oyObject_Copy;
  o->release = (oyStruct_Release_f) oyObject_Release;
  o->ref_ = 1;
    
#if OY_USE_OBJECT_POOL_
  if(old_obj)
    return o;
#endif

  o->id_ = oy_object_id_++;
  o->type_ = oyOBJECT_OBJECT_S;
  o->version_ = oyVersion(0);
  o->hash_ptr_ = 0;
  o->parent_types_ = o->allocateFunc_(sizeof(oyOBJECT_e)*2);
  memset(o->parent_types_,0,sizeof(oyOBJECT_e)*2);

  return o;
}

/** @brief   object management 
 *  @memberof oyObject_s
 *
 *  @param[in]    object         the object
 *
 *  @since Oyranos: version 0.1.8
 *  @date  17 december 2007 (API 0.1.8)
 */
oyObject_s
oyObject_NewFrom ( oyObject_s      object )
{
  oyObject_s o = 0;
  int error = 0;

  if(object && object->type_ != oyOBJECT_OBJECT_S)
  {
    WARNc_S("Attempt to manipulate a non oyObject_s object.")
    return o;
  }

  if(object)
    o = oyObject_NewWithAllocators( object->allocateFunc_,
                                    object->deallocateFunc_ );
  else
    o = oyObject_New( );

  if(!o)
    error = 1;

  if(error <= 0 && object && object->name_)
    error = oyObject_SetNames( o, object->name_->nick, object->name_->name,
                               object->name_->description );

  if(error <= 0)
    error = 1;

  return o;
}

/** @brief   object management 
 *  @memberof oyObject_s
 *
 *  @param[in]    object         the object
 *
 *  @since Oyranos: version 0.1.8
 *  @date  november 2007 (API 0.1.8)
 */
oyObject_s
oyObject_Copy ( oyObject_s      object )
{
  if(!object)
    return 0;

  oyObject_Ref( object );

  return object;
}

/** @brief   release an Oyranos object
 *  @memberof oyObject_s
 *
 *  @since Oyranos: version 0.1.8
 *  @date  november 2007 (API 0.1.8)
 */
int          oyObject_Release         ( oyObject_s      * obj )
{
  /* ---- start of common object destructor ----- */
  oyObject_s s = 0;

  if(!obj || !*obj)
    return 0;

  s = *obj;

  if( s->type_ != oyOBJECT_OBJECT_S)
  {
    WARNc_S("Attempt to release a non oyObject_s object.")
    return 1;
  }

  *obj = 0;

  if(oyObject_UnRef(s))
    return 0;
  /* ---- end of common object destructor ------- */

  oyName_release_( &s->name_, s->deallocateFunc_ );

#if OY_USE_OBJECT_POOL_
  {
  int i;
  for(i = 0; i < 100; ++i)
    if(oy_object_pool_[i] == 0)
    {
      if(s->hash_ptr_)
        memset( s->hash_ptr_, 0, OY_HASH_SIZE*2 );
      oy_object_pool_[i] = s;
      s->ref_ = 0;
      s->parent_ = 0;
      return 0;
    }
  }
#endif

  s->id_ = 0;

  if(s->deallocateFunc_)
  {
    oyDeAlloc_f deallocateFunc = s->deallocateFunc_;
    oyPointer lock = s->lock_;

    if(s->hash_ptr_)
      deallocateFunc(s->hash_ptr_); s->hash_ptr_ = 0;

    if(s->parent_types_)
      deallocateFunc( s->parent_types_ ); s->parent_types_ = 0;

    if(s->backdoor_)
      deallocateFunc( s->backdoor_ ); s->backdoor_ = 0;

    if(s->handles_ && s->handles_->release)
      s->handles_->release( (oyStruct_s**)&s->handles_ );

    deallocateFunc( s );
    oyLockReleaseFunc_( lock, __FILE__, __LINE__ );
  }

  return 0;
}

/* } oyObject common object Functions */


/** @brief   custom object memory managers
 *  @memberof oyObject_s
 *
 *  Normally a sub object should not know about its parent structure, but
 *  follow a hierarchical design. We break this here to allow the observation of
 *  all objects including the parents structures as a feature in oyObject_s.
 *  Then we just need a list of all oyObject_s objects and have an overview.
 *  This facility is intented to work even without debugging tools.
 *
 *  Each inheritent initialiser should call this function separately during
 *  normal object allocation and class initialisation.
 *
 *  @param[in]    o              the object to modify
 *  @param[in]    type           the parents struct type
 *  @param[in]    parent         a pointer to the parent struct
 *
 *  @version Oyranos: 0.3.0
 *  @data    2011/02/02
 *  @since   2007/11/00 (API 0.1.8)
 */
oyObject_s   oyObject_SetParent      ( oyObject_s        o,
                                       oyOBJECT_e        type,
                                       oyPointer         parent )
{
  int error = 0;
  if(error <= 0 && type)
  {
    oyOBJECT_e * tmp = 0;
    if(type != oyOBJECT_NONE)
    {
      tmp = o->allocateFunc_( sizeof(oyOBJECT_e) * (o->parent_types_[0] + 3 ));
      memset(tmp,0,sizeof(oyOBJECT_e) *  (o->parent_types_[0] + 3 ));
      memcpy(tmp, o->parent_types_, sizeof(oyOBJECT_e)*(o->parent_types_[0]+1));

      if(o->deallocateFunc_) o->deallocateFunc_( o->parent_types_ );
      o->parent_types_ = tmp;
      tmp = 0;

      o->parent_types_[0] += 1;
      o->parent_types_[o->parent_types_[0]] = type;
    }

    if(parent)
      o->parent_ = parent;
  }

  return o;
}

/** @brief   object naming
 *  @memberof oyObject_s
 *
 *  @param[in]    object         the object
 *  @param[in]    text           the name to set
 *  @param[in]    type           the kind of name 
 *
 *  @since Oyranos: version 0.1.8
 *  @date  november 2007 (API 0.1.8)
 */
int          oyObject_SetName         ( oyObject_s        object,
                                        const char      * text,
                                        oyNAME_e          type )
{
  if( object->type_ != oyOBJECT_OBJECT_S )
    return 0;

  object->name_ = oyName_set_( object->name_, text, type,
                               object->allocateFunc_, object->deallocateFunc_ );
  return (text && type && object && !object->name_);
}


/** @brief   object naming
 *  @memberof oyObject_s
 *
 *  @param[in]    object         the object
 *  @param[in]    nick           short name, about 1-4 letters
 *  @param[in]    name           the name should fit into usual labels
 *  @param[in]    description    the description 
 *
 *  @since Oyranos: version 0.1.8
 *  @date  november 2007 (API 0.1.8)
 */
int          oyObject_SetNames        ( oyObject_s        object,
                                        const char      * nick,
                                        const char      * name,
                                        const char      * description )
{
  int error = 0;
  

  if(!object) return 0;

  if( object->type_ != oyOBJECT_OBJECT_S )
    return 0;

  if(error <= 0)
    error = oyObject_SetName( object, nick, oyNAME_NICK );
  if(error <= 0)
    error = oyObject_SetName( object, name, oyNAME_NAME );
  if(error <= 0)
    error = oyObject_SetName( object, description, oyNAME_DESCRIPTION );

  return error;
}

/** @brief   object naming
 *  @memberof oyObject_s
 *
 *  @param[in]    dest           the object to modify
 *  @param[in]    src            the object to take names from
 *
 *  @version Oyranos: 0.3.0
 *  @since   2011/01/31 (Oyranos: 0.3.0)
 *  @date    2011/01/31
 */
int          oyObject_CopyNames       ( oyObject_s        dest,
                                        oyObject_s        src )
{
  int error = 0;

  if(!dest || !src) return 0;

  if( dest->type_ != oyOBJECT_OBJECT_S ||
      src->type_ != oyOBJECT_OBJECT_S )
    return 0;

  if(error <= 0)
    error = oyObject_SetNames( dest,
                               oyObject_GetName( src, oyNAME_NICK ),
                               oyObject_GetName( src, oyNAME_NAME ),
                               oyObject_GetName( src, oyNAME_DESCRIPTION ) );

  return error;
}

/** Function oyObject_GetName
 *  @memberof oyObject_s
 *  @brief   object get names
 *
 *  @param[in]    obj            the object
 *  @param[in]    type           name type
 *  @return                      the text
 *
 *  @since Oyranos: version 0.1.8
 *  @date  2007/11/00 (API 0.1.8)
 */
const oyChar * oyObject_GetName       ( const oyObject_s        obj,
                                        oyNAME_e                type )
{
  const char * text = 0;
  if(!obj)
    return 0;

  if( obj->type_ != oyOBJECT_OBJECT_S)
    return 0;

  if(!obj->name_)
    return 0;

  text = oyName_get_( obj->name_, type );

  return text;
}

/** Function oyObject_Lock
 *  @memberof oyObject_s
 *  @brief   Lock a object
 *
 *  @see   oyThreadLockingSet
 *
 *  @param[in]     object              the object
 *  @param[in]     marker              debug hints, e.g. __FILE__
 *  @param[in]     line                source line number, e.g. __LINE__
 *  @return                      error
 *
 *  @since Oyranos: version 0.1.8
 *  @date  2008/01/22 (API 0.1.8)
 */
int          oyObject_Lock             ( oyObject_s        object,
                                         const char      * marker,
                                         int               line )
{
  int error = !object;

  if(error <= 0)
  {
    if( object->type_ != oyOBJECT_OBJECT_S)
      return 0;

    if(!object->lock_)
      object->lock_ = oyStruct_LockCreateFunc_( object->parent_ );

    oyLockFunc_( object->lock_, marker, line );
  }

  return error;
}

/** Function oyObject_UnLock
 *  @memberof oyObject_s
 *  @brief   Unlock a object
 *
 *  @see   oyThreadLockingSet
 *
 *  @param[in]     object              the object
 *  @param[in]     marker              debug hints, e.g. __FILE__
 *  @param[in]     line                source line number, e.g. __LINE__
 *  @return                            error
 *
 *  @since Oyranos: version 0.1.8
 *  @date  2008/01/22 (API 0.1.8)
 */
int          oyObject_UnLock           ( oyObject_s        object,
                                         const char      * marker,
                                         int               line )
{
  int error = !object;

  if(error <= 0)
  {
    if( object->type_ != oyOBJECT_OBJECT_S)
      return 0;

    error = !object->lock_;
    if(error <= 0)
      oyUnLockFunc_( object->lock_, marker, line );
  }

  return error;
}

/** Function oyObject_UnSetLocking
 *  @memberof oyObject_s
 *  @brief   remove a object's lock pointer
 *
 *  @see     oyThreadLockingSet
 *
 *  @param[in]     object              the object
 *  @param[in]     marker              debug hints, e.g. __FILE__
 *  @param[in]     line                source line number, e.g. __LINE__
 *  @return                            error
 *
 *  @since Oyranos: version 0.1.8
 *  @date  2008/01/22 (API 0.1.8)
 */
int          oyObject_UnSetLocking   ( oyObject_s          object,
                                       const char        * marker,
                                       int                 line )
{
  int error = !object;

  if(error <= 0)
  {
    if( object->type_ != oyOBJECT_OBJECT_S)
      return 1;

    oyLockReleaseFunc_( object->lock_, marker, line );
  }

  return error;
}

/** Function oyObject_GetRefCount
 *  @memberof oyObject_s
 *  @brief   get the identification number of a object 
 *
 *  @version Oyranos: 0.1.10
 *  @since   2009/02/19 (Oyranos: 0.1.10)
 *  @date    2009/02/19
 */
int            oyObject_GetRefCount  ( oyObject_s          obj )
{
  if( obj && obj->type_ != oyOBJECT_OBJECT_S)
    return -1;

  if(obj)
    return obj->ref_;

  return -1;
}

/**
 *  Function oyObject_UnRef
 *  @memberof oyObject_s
 *  @brief   decrease the ref counter and return the above zero ref value
 *
 *  @version Oyranos: 0.1.8
 *  @date    2008/02/07
 *  @since   2008/02/07 (Oyranos: 0.1.8)
 */
int          oyObject_UnRef          ( oyObject_s          obj )
{
  int ref = 0;
  oyObject_s s = obj;
  int error = !s;

  if( s->type_ != oyOBJECT_OBJECT_S)
    return 1;

  if(error <= 0)
  {
    oyObject_Lock( s, __FILE__, __LINE__ );

    if(s->ref_ < 0)
      ref = 0;

    if(error <= 0 && --s->ref_ > 0)
      ref = s->ref_;

#   ifndef DEBUG_OBJECT
    if(s->ref_ < -1)
#   else
    if(s->id_ == 247)
#   endif
      WARNc3_S( "%s ID: %d refs: %d",
                oyStructTypeToText( s->parent_types_[s->parent_types_[0]] ),
                s->id_, s->ref_ )

    if((uintptr_t)obj->parent_types_ < (uintptr_t)oyOBJECT_MAX)
    {
      WARNc1_S( "non plausible inheritance pointer: %s", 
                oyStruct_GetInfo(obj,0) );
      return -1;
    }

    if(obj->parent_types_[obj->parent_types_[0]] == oyOBJECT_NAMED_COLOURS_S)
    {
      int e_a = error;
      error = pow(e_a,2.1);
      error = e_a;
    }

    oyObject_UnLock( s, __FILE__, __LINE__ );
  }

  return ref;
}


/* } Include "Object.public_methods_definitions.c" */


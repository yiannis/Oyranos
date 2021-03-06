/** @file oyranos_elektra.h
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  @par Copyright:
 *            2004-2009 (C) Kai-Uwe Behrmann
 *
 *  @brief    elektra dependent functions
 *  @internal
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2004/11/25
 */

#ifndef OYRANOS_ELEKTRA_H
#define OYRANOS_ELEKTRA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "oyranos.h"
#include "oyranos_debug.h"
#include "oyranos_helper.h"
#include "oyranos_internal.h"
#include "oyranos_sentinel.h"
#include "oyranos_xml.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* --- internal API definition --- */

/* separate from the external functions */
void  oyOpen_                   (void);
void  oyClose_                  (void);

int   oyPathsCount_             (void);
char* oyPathName_               (int           number,
                                 oyAlloc_f     allocate_func);
char* oyGetPathFromProfileName_ (const char*   profilename,
                                 oyAlloc_f     allocate_func);
int   oySetProfile_             (const char      * name,
                                 oyPROFILE_e       type,
                                 const char      * comment);


char*   oyGetDeviceProfile_               (const char* manufacturer,
                                           const char* model,
                                           const char* product_id,
                                           const char* host,
                                           const char* port,
                                           const char* attrib1,
                                           const char* attrib2,
                                           const char* attrib3,
                                           oyAlloc_f    );
char**  oyGetDeviceProfile_s              (const char* manufacturer,
                                           const char* model,
                                           const char* product_id,
                                           const char* host,
                                           const char* port,
                                           const char* attrib1,
                                           const char* attrib2,
                                           const char* attrib3,
                                           int** number);
int     oySetDeviceProfile_               (const char* manufacturer,
                                           const char* model,
                                           const char* product_id,
                                           const char* host,
                                           const char* port,
                                           const char* attrib1,
                                           const char* attrib2,
                                           const char* attrib3,
                                           const char* profileName,
                                           const void* mem,
                                           size_t size);
int     oyEraseDeviceProfile_             (const char* manufacturer,
                                           const char* model,
                                           const char* product_id,
                                           const char* host,
                                           const char* port,
                                           const char* attrib1,
                                           const char* attrib2,
                                           const char* attrib3);


void oyOpen  (void);
void oyClose (void);
/* better use oyExportReset_(EXPORT_SETTING) */
void oyCloseReal__(void);



const char* oySelectUserSys_   ();

int     oyGetBehaviour_        (oyBEHAVIOUR_e type);
int     oySetBehaviour_        (oyBEHAVIOUR_e type,
                                int         behaviour);

/* elektra key wrappers */
char*   oyGetKeyString_        (const char       *key_name,
                                oyAlloc_f         allocFunc );
oyPointer  oyGetKeyBinary_           ( const char        * key_name,
                                       size_t            * size,
                                       oyAlloc_f           allocate_func );
int     oyAddKey_valueComment_ (const char* keyName,
                                const char* value, const char* comment);
int     oyKeySetHasValue_      (const char* keyParentName,
                                const char* ask_value);
char*   oySearchEmptyKeyname_        ( const char        * keyParentName );
char **            oyKeySetGetNames_ ( const char        * key_parent_name,
                                       int               * n );
int                oyEraseKey_       ( const char        * key_name );


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* OYRANOS_ELEKTRA_H */

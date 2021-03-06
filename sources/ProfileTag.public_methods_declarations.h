OYAPI oyProfileTag_s * OYEXPORT
               oyProfileTag_Create   ( oyStructList_s    * list,
                                       icTagTypeSignature  tag_type,
                                       uint32_t            version,
                                       const char        * required_cmm,
                                       oyObject_s          object );
OYAPI oyProfileTag_s * OYEXPORT
               oyProfileTag_CreateFromText (
                                       const char        * text,
                                       icTagTypeSignature  tag_type,
                                       icTagSignature      tag_usage,
                                       oyObject_s          object );
OYAPI oyProfileTag_s * OYEXPORT
               oyProfileTag_CreateFromData ( 
                                       icTagSignature      sig,
                                       icTagTypeSignature  type,
                                       oySTATUS_e          status,
                                       size_t              tag_size,
                                       oyPointer           tag_block,
                                       oyObject_s          object );
OYAPI int  OYEXPORT
               oyProfileTag_Set      ( oyProfileTag_s    * tag,
                                       icTagSignature      sig,
                                       icTagTypeSignature  type,
                                       oySTATUS_e          status,
                                       size_t              tag_size,
                                       oyPointer           tag_block );
OYAPI oyStructList_s  * OYEXPORT
                oyProfileTag_Get      ( oyProfileTag_s    * tag );
OYAPI char **  OYEXPORT
                oyProfileTag_GetText  ( oyProfileTag_s    * tag,
                                       int32_t           * n,
                                       const char        * language,
                                       const char        * country,
                                       int32_t           * tag_size,
                                       oyAlloc_f           allocateFunc );
OYAPI int  OYEXPORT
                 oyProfileTag_GetBlock ( oyProfileTag_s    * tag,
                                       oyPointer         * tag_block,
                                       size_t            * tag_size,
                                       oyAlloc_f           allocateFunc );

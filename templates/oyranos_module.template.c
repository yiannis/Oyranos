{% include "source_file_header.txt" %}

/** Function oyCMMsGetFilterApis_
 *  @brief   Let a oyCMMapi5_s meta module open a set of modules
 *  @internal
 *
 *  The oyCMMapiLoadxxx_ function family loads a API from a external module.\n
 *  The module system shall support:
 *    - use of the desired CMM for the task at hand
 *    - provide fallbacks for incapabilities
 *    - process in different ways and by different modules through the same API
 *    - select complementing API's
 *
 *  We have modules with well defined capabilities and some with fuzzy ones.\n
 *  For instance the X11 API's is well defined and we can use it, once it is
 *  loaded.\n
 *  A CMM for colour conversion has often limitations or certain features,
 *  which makes the search for alternatives desireable. So we search
 *  for match to our automatic criteria.\n
 *
 *
 *  This function allowes to obtain API's for matching modules/CMM's.
 *  The modules are filtered for double occurencies. The highest rank or
 *  version is selected.
 *
 *  @param[in]   cmm_meta              the selected meta API will be used,
 *                                     can be Zero or at least 4 bytes long,
 *                                     e.g. "oyIM" for the library module loader
 *  @param[in]   cmm_required          if present take this or fail, the arg
 *                                     simplifies and speeds up the search
 *  @param[in]   registration          point'.' separated list of identifiers
 *  @param[in]   type                  CMM API
 *  @param[in]   flags                 supported is
 *                                     - oyFILTER_REG_MODE_STRIP_IMPLEMENTATION_ATTR
 *                                       for omitting implementation attributes
 *  @param[out]  rank_list             the ranks matching the returned list;
 *                                     without that only the most matching API
 *                                     is returned at position 0
 *  @param[out]  count                 count of returned modules
 *  @return                            a zero terminated list of modules
 *
 *  @version Oyranos: 0.1.11
 *  @since   2008/12/19 (Oyranos: 0.1.10)
 *  @date    2010/08/12
 */
oyCMMapiFilters_s * oyCMMsGetFilterApis_(const char        * cmm_meta,
                                         const char        * cmm_required,
                                         const char        * registration,
                                         oyOBJECT_e          type,
                                         uint32_t            flags,
                                         uint32_t         ** rank_list,
                                         uint32_t          * count )
{
  int error = !oyIsOfTypeCMMapiFilter( type );
  oyCMMapiFilters_s * apis = 0, * apis2 = 0;
  oyCMMapiFilter_s * api = 0,
                   * api2 = 0;
  oyCMMapiFilter_s_ ** api_ = (oyCMMapiFilter_s_**)&api;
  oyCMMapiFilter_s_ ** api2_ = (oyCMMapiFilter_s_**)&api2;
  uint32_t * rank_list_ = 0, * rank_list2_ = 0;
  int rank_list_n = 5, count_ = 0;
  oyObject_s object = oyObject_New();

  /*{
    if(!oy_module_cache_)
      oy_module_cache_ = oyStructList_New( 0 );

    entry = oyCacheListGetEntry_ ( oy_module_cache_, registration );

    api = (oyCMMapiFilter_s*) oyHash_GetPointer_( entry, type );

    if(api)
    {
      oyHash_Release_( &entry );
      return api;
    }
  }*/

  if(error <= 0)
  {
    oyCMMapi5_s_ * api5 = 0;
    oyCMMapis_s * meta_apis = oyCMMGetMetaApis_( cmm_meta );
    int meta_apis_n = 0;
    char ** files = 0;
    uint32_t  files_n = 0;
    int i, j, k = 0, match_j = -1, ret, match_i = -1, rank = 0, old_rank = 0,
        n, accept;
    char * match = 0, * reg = 0;
    oyCMMInfo_s_ * info = 0;

    meta_apis_n = oyCMMapis_Count( meta_apis );
    for(k = 0; k < meta_apis_n; ++k)
    {
      api5 = (oyCMMapi5_s_*) oyCMMapis_Get( meta_apis, k );

      files_n = 0;

      if(error <= 0)
      files = oyCMMsGetNames_(&files_n, api5->sub_paths, api5->ext,
                              cmm_required,
                              api5->data_type == 0 ? oyPATH_MODULE :
                                                     oyPATH_SCRIPT);
      else
        WARNc2_S("%s: %s", _("Could not open meta module API"),
                 oyNoEmptyString_m_( registration ));

      if(files)
      for( i = 0; i < files_n; ++i)
      {
        ret = 0; j = 0;
        ret = api5->oyCMMFilterScan( 0,0, files[i], type, j,
                                     0, 0, oyAllocateFunc_, (oyCMMInfo_s*)&info, object );
        while(!ret)
        {
          ret = api5->oyCMMFilterScan( 0,0, files[i], type, j,
                                       &reg, 0, oyAllocateFunc_, 0, 0 );
          if(!ret && reg)
          {
            rank = oyFilterRegistrationMatch( reg, registration, type );
            if(rank && OYRANOS_VERSION == info->oy_compatibility)
              ++rank;

            if(rank && rank_list)
            {

              if(!rank_list_ && !apis)
              {
                *rank_list = 0;
                oyAllocHelper_m_( *rank_list, uint32_t, rank_list_n+1, 0,
                                  goto clean );
                rank_list_ = *rank_list;
                apis = oyCMMapiFilters_New(0);
              } else
              if(count_ >= rank_list_n)
              {
                rank_list_n *= 2;
                rank_list_ = 0;
                oyAllocHelper_m_( rank_list_, uint32_t, rank_list_n+1, 0,
                                  goto clean );
                error = !memcpy( rank_list_, *rank_list,
                                 sizeof(uint32_t) * rank_list_n/2 );
                oyFree_m_(*rank_list);
                *rank_list = rank_list_;
              }

              rank_list_[k++] = rank;
              api = api5->oyCMMFilterLoad( 0,0, files[i], type, j);
              if(!api)
                continue;

              if(!(*api_)->id_)
                (*api_)->id_ = oyStringCopy_( files[i], oyAllocateFunc_ );
              (*api_)->api5_ = api5;
              if(!apis)
                apis = oyCMMapiFilters_New( 0 );
              oyCMMapiFilters_MoveIn( apis, &api, -1 );
              ++ count_;
              if(count)
                *count = count_;

            } else
            if(rank > old_rank)
            {
              match = reg;
              match_j = j;
              match_i = i;
              old_rank = rank;
            }
          }
          ++j;
          if(reg) oyFree_m_( reg );
        }
      }

      oyCMMInfo_Release( (oyCMMInfo_s**)&info );
#if 0
      if(api5->release)
        api5->release( (oyStruct_s**)&api5 );
#endif
    }

    oyCMMapis_Release( &meta_apis );

    if(match && !rank_list)
    {
      apis2 = oyCMMapiFilters_New( 0 );
      api = api5->oyCMMFilterLoad( 0,0, files[match_i], type, match_j );
      if(!(*api_)->id_)
        (*api_)->id_ = oyStringCopy_( files[match_i], oyAllocateFunc_ );
      (*api_)->api5_ = api5;
      oyCMMapiFilters_MoveIn( apis2, &api, -1 );
      if(count)
        *count = 1;
    }

    if(rank_list_)
    {
      /* filter doubled entries */
      n = oyCMMapiFilters_Count( apis );
      oyAllocHelper_m_( rank_list2_, uint32_t, rank_list_n+1, 0, goto clean );
      k = 0;
      for(i = 0 ; i < n; ++i)
      {
        char * apir = 0;

        api = oyCMMapiFilters_Get( apis, i );
        if(flags | oyFILTER_REG_MODE_STRIP_IMPLEMENTATION_ATTR)
          oyFilterRegistrationModify( (*api_)->registration,
                                    oyFILTER_REG_MODE_STRIP_IMPLEMENTATION_ATTR,
                                      &apir, 0 );
        accept = 1;

        for(j = i+1; j < n; ++j)
        {
          char * api2r = 0;

          api2 = oyCMMapiFilters_Get( apis, j );
          if(flags | oyFILTER_REG_MODE_STRIP_IMPLEMENTATION_ATTR)
            oyFilterRegistrationModify( (*api2_)->registration,
                                    oyFILTER_REG_MODE_STRIP_IMPLEMENTATION_ATTR,
                                        &api2r, 0 );

          /* for equal registration compare rank and version */
          if(oyStrcmp_( apir,  api2r ) == 0 &&
             rank_list_[i] <= rank_list_[j])
            accept = 0;

          oyFree_m_(api2r);

          if(api2->release)
            api2->release( (oyStruct_s**)&api2 );
        }

        if(!apis2)
          apis2 = oyCMMapiFilters_New( 0 );

        if(accept)
        {
          oyCMMapiFilters_MoveIn( apis2, &api, -1 );
          rank_list2_[k++] = rank_list_[i];
        }

        oyFree_m_(apir);

        if(api && api->release)
          api->release( (oyStruct_s**)&api );
      }

      if(rank_list)
        *rank_list = rank_list2_;
      if(rank_list_)
        oyFree_m_(rank_list_);
      if(count)
        *count = k;
    }

    oyStringListRelease_( &files, files_n, oyDeAllocateFunc_ );
  }

  /*if(api)
  {
    (*api_)->id_ = lib_name;
    error = oyHash_SetPointer_( entry, (oyStruct_s*) s );
  }*/

  clean:
    oyObject_Release( &object );

  return apis2;
}
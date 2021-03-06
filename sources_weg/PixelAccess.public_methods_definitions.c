/** Function  oyPixelAccess_ChangeRectangle
 *  @memberof oyConversion_s
 *  @brief    Change the ticket for a conversion graph
 *
 *  @param[in,out] pixel_access        optional pixel iterator configuration
 *  @param[in]     start_x             x position relative to virtual source
 *                                     image
 *  @param[in]     start_y             y position relative to virtual source
 *                                     image
 *  @param[in]     output_rectangle    the region in the output image, optional
 *  @return                            0 on success, else error
 *
 *  @version Oyranos: 0.3.0
 *  @since   2011/04/17 (Oyranos: 0.3.0)
 *  @date    2011/05/ß5
 */
int                oyPixelAccess_ChangeRectangle (
                                       oyPixelAccess_s   * pixel_access,
                                       double              start_x,
                                       double              start_y,
                                       oyRectangle_s     * output_rectangle )
{
  oyPixelAccess_s_ ** pixel_access_ = (oyPixelAccess_s_**)&pixel_access;
  int error = 0;
  oyRectangle_s_ * roi = (oyRectangle_s_*)oyRectangle_New(0);

  if(!pixel_access)
    error = 1;

  if(error <= 0 && output_rectangle)
    oyRectangle_SetByRectangle( (oyRectangle_s*)(*pixel_access_)->output_image_roi,
                                output_rectangle );

  if(error <= 0)
  {
    oyRectangle_SetByRectangle( (oyRectangle_s*)roi, (oyRectangle_s*)(*pixel_access_)->output_image_roi );
    (*pixel_access_)->start_xy[0] = roi->x = start_x;
    (*pixel_access_)->start_xy[1] = roi->y = start_y;
  }
  oyRectangle_Release( (oyRectangle_s**)&roi );

  return error;
}

/** Function  oyPixelAccess_Create
 *  @memberof oyPixelAccess_s
 *  @brief    Allocate iand initialise a basic oyPixelAccess_s object
 *
 *  @verbatim
  // conversion->out_ has to be linear, so we access only the first plug
  node = oyConversion_GetNode( conversion, OY_OUTPUT );
  plug = oyFilterNode_GetPlug( node, 0 );
  oyFilterNode_Release( &node );

  // create a very simple pixel iterator
  if(plug)
    pixel_access = oyPixelAccess_Create( 0,0, plug,
                                         oyPIXEL_ACCESS_IMAGE, 0 );
@endverbatim
 *
 *  @version Oyranos: 0.1.10
 *  @since   2008/07/07 (Oyranos: 0.1.8)
 *  @date    2009/06/10
 */
oyPixelAccess_s *  oyPixelAccess_Create (
                                       int32_t             start_x,
                                       int32_t             start_y,
                                       oyFilterPlug_s    * plug,
                                       oyPIXEL_ACCESS_TYPE_e type,
                                       oyObject_s          object )
{
  oyPixelAccess_s_ * s = (oyPixelAccess_s_*)oyPixelAccess_New( object );
  oyFilterSocket_s_ * sock = 0;
  oyFilterPlug_s_ ** plug_ = (oyFilterPlug_s_**)&plug;
  int error = !s || !plug || !(*plug_)->remote_socket_;
  int w = 0;
  oyImage_s * image = 0;
  int32_t n = 0;

  if(error <= 0)
  {
    sock = (*plug_)->remote_socket_;
    image = (oyImage_s*)sock->data;

    s->start_xy[0] = s->start_xy_old[0] = start_x;
    s->start_xy[1] = s->start_xy_old[1] = start_y;

    /* make shure the filter->image_ is set, e.g.
       error = oyFilterCore_ImageSet ( filter, image );

    s->data_in = filter->image_->data; */
    if(image)
      w = oyImage_Width( image );

    /** The filters have no obligation to pass end to end informations.
        The ticket must hold all pices of interesst.
     */
    s->output_image_roi->width = 1.0;
    if(image)
      s->output_image_roi->height = oyImage_Height( image ) / (double)oyImage_Width( image );
    s->output_image = oyImage_Copy( image, 0 );
    s->graph = (oyFilterGraph_s_*)oyFilterGraph_FromNode( (oyFilterNode_s*)sock->node, 0 );

    if(type == oyPIXEL_ACCESS_POINT)
    {
      s->array_xy = s->oy_->allocateFunc_(sizeof(int32_t) * 2);
      s->array_xy[0] = s->array_xy[1] = 0;
      s->array_n = 1;
      s->pixels_n = 1;
    } else
    if(type == oyPIXEL_ACCESS_LINE)
    {
      s->array_xy = s->oy_->allocateFunc_(sizeof(int32_t) * 2);
      /* set relative advancements from one pixel to the next */
      s->array_xy[0] = 1;
      s->array_xy[1] = 0;
      s->array_n = 1;
      s->pixels_n = w;       /* the total we want */
    } else
    /* if(type == oyPIXEL_ACCESS_IMAGE) */
    {
      /** @todo how can we know about the various module capabilities
       *  - back report the processed number of pixels in the passed pointer
       *  - restrict for a line interface only, would fit to oyArray2d_s
       *  - + handle inside an to be created function oyConversion_RunPixels()
       */
    }

    /* Copy requests, which where attached to the node, to the ticket. */
    if((*plug_)->node->core->options_)
      error = oyOptions_Filter( &s->request_queue, &n, 0,
                                oyBOOLEAN_INTERSECTION,
                                "////resolve", (*plug_)->node->core->options_ );
  }

  if(error)
    oyPixelAccess_Release ( (oyPixelAccess_s**)&s );

  return (oyPixelAccess_s*)s;
}

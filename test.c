/*
 * Oyranos is an open source Colour Management System 
 * 
 *  @par Copyright:
 *            2004-2009 (C) Kai-Uwe Behrmann
 *
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2004/11/25
 *
 * 
 */

#include <kdb.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <oyranos.h>
#include <oyranos_alpha.h>
#include <oyranos_texts.h> /* oyStringListRelease_ */
#include <oyranos_cmm.h>   /* for hacking into module API */

/* forward declaration for oyranos_alpha.c */
char ** oyCMMsGetNames_              ( int               * n,
                                       oyOBJECT_e        * types,
                                       int                 types_n );
char *           oyCMMInfoPrint_     ( oyCMMInfo_s       * cmm_info );
char**             oyStringSplit_    ( const char    * text,
                                       const char      delimiter,
                                       int           * count,
                                       oyAlloc_f       allocateFunc );
int                oyWriteMemToFile_ ( const char*, void*, size_t );


int
main(int argc, char** argv)
{
  oyPointer pixel = 0;
  oyPixelAccess_s * pixel_access = 0;
  oyConversion_s * conversion = 0;
  oyFilterNode_s   * in = 0, * out = 0;
  oyFilterSocket_s * sock = 0;
  oyOptions_s * options = 0;
  int32_t result = 0;
  oyImage_s * image_in = 0, * image_out = 0;
  double buf[24] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
  double buf2[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  double * d = 0, * dest = 0;
  oyProfile_s * prof = 0;
  int x,y,w,h, i,j;
  char * ptr = 0;
  uint32_t size = 0;
  int error = 0;

  w = 7;
  h = 32;
  size = sizeof(double)*w*h*3;
  d = malloc(size);
  dest = malloc(size);
  for(i = 0; i < w*h; ++i)
    for(j=0; j < 3; ++j)
      d[i*3+j] = (double)(i*3+j)/(double)size*sizeof(double);

  prof = oyProfile_FromStd( oyASSUMED_WEB, 0 );
  image_in = oyImage_Create( w, h, d, OY_TYPE_123_DBL, prof, 0 );
  image_out = oyImage_Create( w, h, dest, OY_TYPE_123_DBL, prof, 0 );
  conversion = oyConversion_CreateBasicPixels( image_in, image_out, 0, 0 );
  oyImage_Release( &image_in );
  oyImage_Release( &image_out );
  oyProfile_Release( &prof );
  /* create a very simple pixel iterator */
  pixel_access = oyPixelAccess_Create( 0,0,
                                 oyFilterNode_GetPlug( conversion->out_, 0 ),
                                       oyPIXEL_ACCESS_IMAGE, 0 );

  /* show the Oyranos graph with ghostview */
  ptr = oyConversion_ToText( conversion, "Oyranos simple Test Graph",0,malloc);
  oyWriteMemToFile_( "test.dot", ptr, strlen(ptr) );
#if 1
  system("dot -Tps test.dot -o test.ps; gv -spartan -antialias -magstep 0.7 test.ps &");
#endif
  free(ptr); ptr = 0;


  result = 0;
  x = y = 0;
  {
    double * p;

    pixel_access->start_xy[0] = x;
    pixel_access->start_xy[1] = y;
    result = oyConversion_RunPixels( conversion, pixel_access );

    p = (double*)pixel_access->array->array2d[0];

    if(result == 0)
    for(i = 0; i < pixel_access->array->height; ++i)
    for(j = 0; j < pixel_access->array->width; j += 3)
    {
      char br = (j != pixel_access->array->width - 3) ? ' ':'\n';
      p = (double*)pixel_access->array->array2d[i];
      fprintf( stdout, "%.03f %.03f %.03f %c",
               p[j+0], p[j+1], p[j+2], br );
    }
  }

  /* itereate in chunks */
  result = 0;
  x = y = 0;
  pixel_access->start_xy[0] = pixel_access->start_xy[1] = x;
  while(result == 0 && 0)
  {
    double * p = 0;

    result = oyConversion_RunPixels( conversion, pixel_access );
    p = pixel;

    if(result == 0)
    for(i = 0; i < pixel_access->pixels_n*3; i += 3)
      fprintf( stdout, "%.01f %.01f %.01f\n", p[i+0], p[i+1], p[i+2] );

    x += 2;
    /*if(x >= w)
    {
      pixel_access->start_xy[0] = 0;
      pixel_access->start_xy[1] = ++y;
      x = 0;
    }*/
  }


  if(conversion->input->core->api4_->oyCMMFilterNode_ContextToMem)
    ptr = conversion->input->core->api4_->oyCMMFilterNode_ContextToMem( conversion->input, &size, malloc );
  free(ptr); ptr = 0;

  if (0) /* dump the colour transformation */
  {
    oyFilterPlug_s * plug = oyFilterNode_GetPlug( conversion->out_, 0 );
    ptr = plug->remote_socket_->node->core->api4_->oyCMMFilterNode_ContextToMem(
             plug->remote_socket_->node, &size, malloc );
  }

  if(ptr)
    oyWriteMemToFile_( "test_dbg.icc", ptr, size );

  oyConversion_Release( &conversion );
  return 0;



  conversion = oyConversion_New( 0 );
  in = oyFilterNode_NewWith( "//" OY_TYPE_STD "/input_ppm", 0, 0 );
  oyConversion_Set( conversion, in, 0 );

  options = oyFilterNode_OptionsGet( in, OY_FILTER_GET_DEFAULT );
  error = oyOptions_SetFromText( &options, "//" OY_TYPE_STD "/input_ppm/filename",
                                 "oyranos_logo.ppm", OY_CREATE_NEW );
  oyOptions_Release( &options );

  image_in = oyConversion_GetImage( conversion, OY_INPUT );;
  prof = oyProfile_FromFile( "Lab.icc", 0, 0 );
  image_out = oyImage_Create( image_in->width, image_in->height, 0, 
                              oyImage_PixelLayoutGet( image_in ), prof, 0 );

  out = oyFilterNode_NewWith( "//" OY_TYPE_STD "/icc", 0, 0 );
  error = oyFilterNode_Connect( in, "//" OY_TYPE_STD "/data",
                                out, "//" OY_TYPE_STD "/data", 0 );
  if(error > 0)
    fprintf( stderr, "could not add  filter: %s\n", "//" OY_TYPE_STD );
  in = out; in = 0; 
 
  out = oyFilterNode_NewWith( "//" OY_TYPE_STD "/output_ppm", 0, 0 );
  options = oyFilterNode_OptionsGet( out, OY_FILTER_GET_DEFAULT );
  error = oyOptions_SetFromText( &options, "//" OY_TYPE_STD "/output_ppm/filename",
                                 "test_dbg.ppm", OY_CREATE_NEW );
  oyOptions_Release( &options );
  error = oyFilterNode_Connect( in, "//" OY_TYPE_STD "/data",
                                out, "//" OY_TYPE_STD "/data", 0 );

  pixel_access->start_xy[0] = 0;
  pixel_access->start_xy[1] = 0;
  result = oyConversion_RunPixels( conversion, pixel_access );

  oyPixelAccess_Release( &pixel_access );
  oyImage_Release( &image_in );
  oyImage_Release( &image_out );
  oyProfile_Release( &prof );
  ptr = oyConversion_ToText( conversion, "Oyranos simple Test Graph",0,malloc);
  oyWriteMemToFile_( "test.dot", ptr, strlen(ptr) );
#if 1
  system("dot -Tps test.dot -o test.ps; gv -spartan -antialias -magstep 0.7 test.ps &");
#endif
  free(ptr); ptr = 0;

  image_out = oyConversion_GetImage( conversion, OY_OUTPUT );
  fprintf( stdout, "image: %dx%d\n", image_out->width, image_out->height );
  oyImage_Release( &image_out );

  oyConversion_Release( &conversion );


  return 0;
}



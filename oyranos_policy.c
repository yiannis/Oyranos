/** @file oyranos_policy.c
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  @par Copyright:
 *            2006-2009 (C) Kai-Uwe Behrmann
 *
 *  @brief    policy loader - for usage during installation and on commandline
 *  @internal
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @par License:
 *            new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2006/09/14
 *
 *  The program takes a policy XML file as argument and sets the behaviour 
 *  accordingly.
 */


#include "oyranos.h"
#include "oyranos_debug.h"
#include "oyranos_elektra.h"
#include "oyranos_helper.h"
#include "oyranos_internal.h"
#include "oyranos_config.h"
#include "oyranos_string.h"
#include "oyranos_version.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void* oyAllocFunc(size_t size) {return malloc (size);}

void  printfHelp (int argc, char** argv)
{
  char * version = oyVersionString(1,0);
  char * id = oyVersionString(2,0);
  char * cfg_date =  oyVersionString(3,0);
  char * devel_time = oyVersionString(4,0);

  fprintf( stderr, "\n");
  fprintf( stderr, "oyranos-policy %s\n",
                                _("is a policy administration tool"));
  fprintf( stderr, "  Oyranos v%s config: %s devel period: %s\n",
                  oyNoEmptyName_m_(version),
                  oyNoEmptyName_m_(cfg_date), oyNoEmptyName_m_(devel_time) );
  if(id)
  fprintf( stderr, "  Oyranos git id %s\n", oyNoEmptyName_m_(id) );
  fprintf( stderr, "\n");
  fprintf( stderr, "  %s\n",
                                           _("Hint: search paths are influenced by the XDG_CONFIG_HOME shell variable."));
  fprintf( stderr, "\n");
  fprintf( stderr, "%s\n",                 _("Usage"));
  fprintf( stderr, "  %s\n",               _("Dump out the actual settings:"));
  fprintf( stderr, "      %s -d\n",        argv[0]);
  fprintf( stderr, "  %s\n",               _("Select active policy:"));
  fprintf( stderr, "      %s -i %s\n",     argv[0], _("policy or filename"));
  fprintf( stderr, "  %s\n",               _("List available policies:"));
  fprintf( stderr, "      %s -l\n",        argv[0]);
  fprintf( stderr, "  %s\n",               _("Currently active policy:"));
  fprintf( stderr, "      %s -c\n",        argv[0]);
  fprintf( stderr, "  %s\n",               _("List search paths:"));
  fprintf( stderr, "      %s -p\n",        argv[0]);
  fprintf( stderr, "  %s\n",               _("Save to a new policy:"));
  fprintf( stderr, "      %s -s %s\n",     argv[0], _("policy name"));
  fprintf( stderr, "  %s\n",               _("Print a help text:"));
  fprintf( stderr, "      %s -h\n",        argv[0]);
  fprintf( stderr, "\n");
  fprintf( stderr, "\n");

  if(version) oyDeAllocateFunc_(version);
  if(id) oyDeAllocateFunc_(id);
  if(cfg_date) oyDeAllocateFunc_(cfg_date);
  if(devel_time) oyDeAllocateFunc_(devel_time);
}

int main( int argc , char** argv )
{
  int error = 0;
  const char* save_policy = NULL,
            * import_policy = NULL;
  size_t size = 0;
  char *xml = NULL;
  char * import_policy_fn = NULL;
  int current_policy = 0, list_policies = 0, list_paths = 0,
      dump_policy = 0;

#ifdef USE_GETTEXT
  setlocale(LC_ALL,"");
#endif
  oyExportStart_(EXPORT_CHECK_NO);

  if(argc >= 2)
  {
    int pos = 1;
    char *wrong_arg = 0;
    DBG_PROG1_S("argc: %d\n", argc);
    while(pos < argc)
    {
      switch(argv[pos][0])
      {
        case '-':
            switch (argv[pos][1])
            {
              case 'c': current_policy = 1; break;
              case 'd': dump_policy = 1; break;
              case 'i': if( pos + 1 < argc )
                        { import_policy = argv[pos+1];
                          if( !oyStrlen_(import_policy) )
                            wrong_arg = "-i";
                        } else wrong_arg = "-i";
                        if(oy_debug) fprintf(stderr,"import_policy=0\n"); ++pos;
                        break;
              case 'l': list_policies = 1; break;
              case 'p': list_paths = 1; break;
              case 's': if( pos + 1 < argc )
                        { save_policy = argv[pos+1];
                          if( !oyStrlen_(save_policy) )
                            wrong_arg = "-s";
                        } else wrong_arg = "-s";
                        if(oy_debug) fprintf(stderr,"save_policy=0\n"); ++pos;
                        break;
              case 'v': oy_debug += 1; break;
              case 'h':
              default:
                        printfHelp(argc, argv);
                        exit (0);
                        break;
            }
            break;
        default:
                        printfHelp(argc, argv);
                        exit (0);
                        break;
      }
      if( wrong_arg )
      {
       fprintf(stderr, "%s %s\n", _("wrong argument to option:"), wrong_arg);
       printfHelp(argc, argv);
       exit(1);
      }
      ++pos;
    }
  } else
  {
                        printfHelp(argc, argv);
                        exit (0);
  }

  /* check the default paths */
  /*oyPathAdd( OY_PROFILE_PATH_USER_DEFAULT );*/


  /* load the policy file into memory */
  import_policy_fn = oyMakeFullFileDirName_(import_policy);
  if(oyIsFile_(import_policy_fn))
  {
    xml = oyReadFileToMem_( oyMakeFullFileDirName_(import_policy), &size,
                            oyAllocateFunc_ );
    oyDeAllocateFunc_( import_policy_fn );
  }
  /* parse and set policy */
  if(xml)
  {
    oyReadXMLPolicy( oyGROUP_ALL, xml );
    oyDeAllocateFunc_( xml );
  }
  else if ( import_policy )
  {
    error = oyPolicySet( import_policy, 0 );
    if(error)
      fprintf( stderr, "%s:%d could not read file: %s\n",__FILE__,__LINE__, import_policy);
    return 1;
  }

  if(save_policy)
  {
    error = oyPolicySaveActual( oyGROUP_ALL, save_policy );
    if(!error)
      fprintf( stdout, "%s \"%s\"\n",
               _("installed new policy"), save_policy);
    else
      fprintf( stdout, "\"%s\" %s %d\n", save_policy,
               _("installation of new policy file failed with error:"), error);

  } else
  if(current_policy || list_policies || list_paths)
  {
    const char ** names = NULL;
    int count = 0, i, current = -1;
    oyOptionChoicesGet( oyWIDGET_POLICY, &count, &names, &current );

    if(list_policies)
      for(i = 0; i < count; ++i)
        fprintf(stdout, "%s\n", names[i]);

    if(current_policy)
      fprintf( stdout, "%s \"%s\"\n",
               _("Currently active policy:"), current>=0?names[current]:"---");

    if(list_paths)
    {
      char ** path_names = oyDataPathsGet_( &count, "color/settings",
                                              oyALL, oyUSER_SYS,
                                              oyAllocateFunc_ );
      fprintf(stdout, "%s:\n", _("Policy search paths"));
      for(i = 0; i < count; ++i)
        fprintf(stdout, "%s\n", path_names[i]);

      oyStringListRelease_(&path_names, count, oyDeAllocateFunc_);
    }

  } else
  if(dump_policy)
  {
    size = 0;
    xml = oyPolicyToXML( oyGROUP_ALL, 1, oyAllocateFunc_ );
    DBG_PROG2_S("%s:%d new policy:\n\n",__FILE__,__LINE__);
    fprintf(stdout, "%s\n", xml);

    if(xml) oyDeAllocateFunc_( xml );
  }

  return error;
}

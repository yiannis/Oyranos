/** @file oyranos_io.c
 *
 *  Oyranos is an open source Colour Management System 
 *
 *  Copyright (C) 2004-2008  Kai-Uwe Behrmann
 *
 */

/**
 *  @brief    input / output  methods
 *  @internal
 *  @author   Kai-Uwe Behrmann <ku.b@gmx.de>
 *  @license: new BSD <http://www.opensource.org/licenses/bsd-license.php>
 *  @since    2004/11/25
 */

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "oyranos.h"
#include "oyranos_check.h"
#include "oyranos_cmms.h"
#include "oyranos_debug.h"
#include "oyranos_elektra.h"
#include "oyranos_helper.h"
#include "oyranos_internal.h"
#include "oyranos_io.h"
#include "oyranos_sentinel.h"
#include "oyranos_xml.h"

/* --- Helpers  --- */

/* --- static variables   --- */

/* --- structs, typedefs, enums --- */

/* --- internal API definition --- */

/* oyranos part */
/* check for the global and the users directory */
void oyCheckDefaultDirectories_ ();
/* search in profile path and in current path */
char* oyFindProfile_ (const char* name);



/* --- Helpers  --- */
/* small helpers */
#if 1
#define ERR if (rc<=0 && oy_debug) { printf("%s:%d %d\n", __FILE__,__LINE__,rc); perror("Error"); }
#else
#define ERR
#endif


/* --- function definitions --- */


size_t
oyReadFileSize_(const char* name)
{
  FILE *fp = 0;
  const char* filename = name;
  size_t size = 0;

  DBG_PROG_START

  {
    fp = fopen(filename, "r");
    DBG_PROG2_S ("fp = %d filename = %s\n", (int)(intptr_t)fp, filename)

    if (fp)
    {
      /* get size */
      fseek(fp,0L,SEEK_END); 
      size = ftell (fp);
      fclose (fp);

    } else
      WARNc1_S( "could not read %s", filename );
  }

  DBG_PROG_ENDE
  return size;
}

char*
oyReadFileToMem_(const char* name, size_t *size,
                 oyAllocFunc_t allocate_func)
{
  FILE *fp = 0;
  char* mem = 0;
  const char* filename = name;

  DBG_PROG_START

  DBG_PROG

  {
    fp = fopen(filename, "r");
    DBG_PROG2_S ("fp = %u filename = %s\n", (unsigned int)((intptr_t)fp), filename)

    if (fp)
    {
      /* get size */
      fseek(fp,0L,SEEK_END); 
      /* read file possibly partitial */
      if(!*size || *size > ftell(fp))
        *size = ftell (fp);
      rewind(fp);

      DBG_PROG1_S("%u\n",((unsigned int)((size_t)size)));

      /* allocate memory */
      oyAllocHelper_m_( mem, char, *size+1, oyAllocateFunc_, return 0);

      /* check and read */
      if ((fp != 0)
       && mem
       && *size)
      {
        int s = fread(mem, sizeof(char), *size, fp);

        DBG_PROG

        /* check again */
        if (s != *size)
        { *size = 0;
          oyFree_m_ (mem)
          mem = 0;
        } else {
          /* copy to external allocator */
          char* temp = mem;
          mem = oyAllocateWrapFunc_( *size+1, allocate_func );
          if(mem) {
            memcpy( mem, temp, *size );
            oyFree_m_ (temp)
          } else {
            oyFree_m_ (mem)
            *size = 0;
          }
        }
      }
    } else {
      WARNc1_S( "could not read %s\n", filename );
    }
  }
 
  /* clean up */
  if (fp) fclose (fp);

  DBG_PROG_ENDE
  return mem;
}

int
oyWriteMemToFile_(const char* name, void* mem, size_t size)
{
  FILE *fp = 0;
  /*int   pt = 0;
  char* block = mem;*/
  const char* filename;
  int r = 0;
  size_t written_n = 0;

  DBG_PROG_START

  DBG_PROG3_S("name = %s mem = %d size = %d\n", name, (int)(intptr_t)mem, (int)(intptr_t)size)

  filename = name;

  {
    fp = fopen(filename, "w");
    DBG_PROG2_S("fp = %d filename = %s", (int)(intptr_t)fp, filename)
    if ((fp != 0)
     && mem
     && size)
    { DBG_PROG
#if 0
      do {
        r = fputc ( block[pt++] , fp);
      } while (--size);
#else
      written_n = fwrite( mem, size, 1, fp );
#endif
    }

    if (fp) fclose (fp);
  }

  DBG_PROG_ENDE
  return r;
}

char*
oyGetHomeDir_ ()
{
# if (__WINDOWS__)
  DBG_PROG_START
  DBG_PROG_ENDE
  return "OS not supported yet";
# else
  char* name = (char*) getenv("HOME");

  DBG_PROG_START

  DBG_PROG_S(name)
  DBG_PROG_ENDE
  return name;
# endif
}

char*
oyGetParent_ (const char* name)
{
  char *parentDir = (char*) calloc ( MAX_PATH, sizeof(char)), *ptr;

  DBG_PROG_START

  oySprintf_ (parentDir, name);
  ptr = strrchr( parentDir, OY_SLASH_C);
  if (ptr)
  {
    if (ptr[1] == 0) /* ending dir separator */
    {
      ptr[0] = 0;
      if (strrchr( parentDir, OY_SLASH_C))
      {
        ptr = strrchr (parentDir, OY_SLASH_C);
        ptr[0] = 0;
      }
    }
    else
      ptr[0] = 0;
  }

  DBG_PROG_S(parentDir)

  DBG_PROG_ENDE
  return parentDir;
}

int
oyIsDir_ (const char* path)
{
  struct stat status;
  int r = 0;
  char* name = oyResolveDirFileName_ (path);

  DBG_PROG_START

  status.st_mode = 0;
  r = stat (name, &status);
  DBG_PROG1_S("status.st_mode = %d", (int)(status.st_mode&S_IFMT)&S_IFDIR)
  DBG_PROG1_S("status.st_mode = %d", (int)status.st_mode)
  DBG_PROG1_S("name = %s ", name)
  oyFree_m_ (name)
  r = !r &&
       ((status.st_mode & S_IFMT) & S_IFDIR);

  DBG_PROG_ENDE
  return r;
}

#include <errno.h>

int
oyIsFileFull_ (const char* fullFileName)
{
  struct stat status;
  int r = 0;
  const char* name = fullFileName;

  DBG_PROG_START

  DBG_NUM1_S("fullFileName = \"%s\"", fullFileName)
  status.st_mode = 0;
  r = stat (name, &status);

  DBG_NUM1_S("status.st_mode = %d", (int)(status.st_mode&S_IFMT)&S_IFDIR)
  DBG_NUM1_S("status.st_mode = %d", (int)status.st_mode)
  DBG_NUM1_S("name = %s", name)
  DBG_NUM_V( r )
  switch (r)
  {
    case EACCES:       WARNc1_S("EACCES = %d\n",r); break;
    case EIO:          WARNc1_S("EIO = %d\n",r); break;
    case ELOOP:        WARNc1_S("ELOOP = %d\n",r); break;
    case ENAMETOOLONG: WARNc1_S("ENAMETOOLONG = %d\n",r); break;
    case ENOENT:       WARNc1_S("ENOENT = %d\n",r); break;
    case ENOTDIR:      WARNc1_S("ENOTDIR = %d\n",r); break;
    case EOVERFLOW:    WARNc1_S("EOVERFLOW = %d\n",r); break;
  }

  r = !r &&
       (   ((status.st_mode & S_IFMT) & S_IFREG)
        || ((status.st_mode & S_IFMT) & S_IFLNK));

  DBG_NUM_V( r )
  if (r)
  {
    FILE* fp = fopen (name, "r"); DBG_PROG
    if (!fp) { DBG_PROG
      r = 0;
    } else { DBG_PROG
      fclose (fp);
    }
  } DBG_PROG

  DBG_PROG_ENDE
  return r;
}

int
oyIsFile_ (const char* fileName)
{
  int r = 0;
  char* name = oyResolveDirFileName_ (fileName);

  DBG_PROG_START

  r = oyIsFileFull_(name);

  oyFree_m_ (name) DBG_PROG

  DBG_PROG_ENDE
  return r;
}

int
oyMakeDir_ (const char* path)
{
  char *name = oyResolveDirFileName_ (path);
  int rc = 0;

  mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* 0755 */

  DBG_PROG_START

  DBG_PROG
  rc = mkdir (name, mode);
  oyFree_m_ (name)

  DBG_PROG_ENDE
  return rc;
}

char*
oyResolveDirFileName_ (const char* name)
{
  char* newName = (char*) calloc (MAX_PATH, sizeof(char)),
       *home = 0;
  int len = 0;

  DBG_PROG_START

  DBG_PROG_S(name)

  /* user directory */
  if (name[0] == '~')
  { DBG_PROG_S(("in home directory"))
    home = oyGetHomeDir_();
    len = strlen(name) + strlen(home) + 1;
    if (len >  FILENAME_MAX)
      WARNc2_S("%s %d", _("file name is too long"), len)

    oySprintf_ (newName, "%s%s", home, &name[0]+1);

  } else { DBG_PROG_S("resolve  directory")
    oySprintf_ (newName, name);

    /* relative names - where the first sign is no directory separator */
    if (newName[0] != OY_SLASH_C)
    { char* cn = (char*) calloc(MAX_PATH, sizeof(char)); DBG_PROG
      oySprintf_ (cn, "%s%s%s", getenv("PWD"), OY_SLASH, name);
      DBG_PROG1_S("canonoical %s ", cn)
      oySprintf_ (newName, cn);
    }
  }

  if(name)
    DBG_PROG1_S ("name %s", name);
  if(home)
    DBG_PROG1_S ("home %s", home);
  if(newName)
    DBG_PROG1_S ("newName = %s", newName);

  DBG_PROG_ENDE
  return newName;
}

char*
oyExtractPathFromFileName_ (const char* file_name)
{
  char *path_name = (char*) calloc (strlen(file_name)+1, sizeof(char));
  char *ptr;

  DBG_PROG_START

  oySprintf_( path_name, file_name );
  DBG_PROG1_S ("path_name = %s", path_name)
  ptr = strrchr (path_name, '/');
  ptr[0] = 0;
  DBG_PROG1_S ("path_name = %s", path_name)
  DBG_PROG1_S ("ptr = %s", ptr)
  DBG_PROG_ENDE
  return path_name;
}

char*
oyMakeFullFileDirName_ (const char* name)
{
  char *newName;
  char *dirName = 0;

  DBG_PROG_START

  DBG_PROG
  if(name &&
     strrchr( name, OY_SLASH_C ))
  { DBG_PROG
    /* substitute ~ with HOME variable from environment */
    newName = oyResolveDirFileName_ (name);
  } else
  { DBG_PROG
    /* create directory name */
    newName = (char*) calloc (MAX_PATH, sizeof(char)),
    dirName = (char*) getenv("PWD");
    oySprintf_ (newName, "%s%s", dirName, OY_SLASH);
    if (name)
      oySprintf_ (strrchr(newName,OY_SLASH_C)+1, "%s", name);
    DBG_PROG1_S("newName = %s", newName)
  }

  DBG_PROG1_S("newName = %s", newName)

  DBG_PROG_ENDE
  return newName;
}

void
oyCheckDefaultDirectories_ ()
{
  char* parentDefaultUserDir;

  DBG_PROG_START

  /* test dirName : existing in path, default dirs are existing */
  if (!oyIsDir_ (OY_PROFILE_PATH_SYSTEM_DEFAULT))
  { DBG_PROG
    DBG_PROG1_S( "no default system directory %s",OY_PROFILE_PATH_SYSTEM_DEFAULT );
  }

  if (!oyIsDir_ (OY_PROFILE_PATH_USER_DEFAULT))
  { DBG_PROG 
    parentDefaultUserDir = oyGetParent_ (OY_PROFILE_PATH_USER_DEFAULT);

    if (!oyIsDir_ (parentDefaultUserDir))
    {
      DBG_PROG1_S( "Try to create part of users default directory %s",
                 parentDefaultUserDir );
      oyMakeDir_( parentDefaultUserDir);
    }
    oyFree_m_ (parentDefaultUserDir)

    DBG_PROG1_S( "Try to create users default directory %s",
                 OY_PROFILE_PATH_USER_DEFAULT )
    oyMakeDir_( OY_PROFILE_PATH_USER_DEFAULT );
  }
  DBG_PROG_ENDE
}

char*
oyFindProfile_ (const char* fileName)
{
  char  *fullFileName = 0;

  DBG_PROG_START

  /*DBG_NUM_S((fileName)) */
  if (fileName && !strchr(fileName, OY_SLASH_C))
  {
    char* path_name = oyGetPathFromProfileName_(fileName, oyAllocateFunc_);

    DBG_PROG
    fullFileName = (char*) calloc (MAX_PATH, sizeof(char));
    if(path_name)
    {
      oySprintf_(fullFileName, "%s%s%s", path_name, OY_SLASH, fileName);
      oyFree_m_(path_name)
    } else
      oySprintf_(fullFileName, "%s", fileName);
    DBG_PROG_S( fullFileName )
  } else
  {
    if (oyIsFileFull_(fileName)) {
      fullFileName = (char*) calloc (MAX_PATH, sizeof(char));
      oySprintf_(fullFileName, fileName);
    } else
      fullFileName = oyMakeFullFileDirName_ (fileName);
  }

  DBG_PROG_ENDE
  return fullFileName;
}




/* public API implementation */

/* profile and other file lists API */

#define MAX_DEPTH 64
struct oyFileList_s_ {
  oyOBJECT_TYPE_e type;
  int hopp;
  const char* coloursig;
  int mem_count;
  int count_files;
  char** names;
};

int oyProfileListCb_ (void* data, const char* full_name, const char* filename)
{
  struct oyFileList_s_ *l = (struct oyFileList_s_*)data;

  if(l->type != oyOBJECT_TYPE_FILE_LIST_S_)
    WARNc_S("Could not find a oyFileList_s_ objetc.");

      if (!oyCheckProfile_(full_name, l->coloursig))
      {
        if(l->count_files >= l->mem_count)
        {
          char** temp = l->names;
          l->names = (char**) calloc (sizeof(char*), l->mem_count+l->hopp);
          memcpy(l->names, temp, sizeof(char*) * l->mem_count);
          l->mem_count += l->hopp;
        }

        l->names[l->count_files] = (char*) calloc (sizeof(char)*2,
                                                   strlen(filename));
        strcpy(l->names[l->count_files], filename);
        ++l->count_files;
      } /*else */
        /*WARNc_S(("%s in %s is not a valid profile", file_name, path)); */
  return 0;
}

int oyPolicyListCb_ (void* data, const char* full_name, const char* filename)
{
  struct oyFileList_s_ *l = (struct oyFileList_s_*)data;
  /* last 4 chars */
  const char * end = NULL;

  if(l->type != oyOBJECT_TYPE_FILE_LIST_S_)
    WARNc_S("Could not find a oyFileList_s_ objetc.");

  if(strlen(full_name) > 4)
    end = full_name + strlen(full_name) - 4;

      if( (end[0] == '.') &&
          (end[1] == 'x' || end[1] == 'X') &&
          (end[2] == 'm' || end[2] == 'M') &&
          (end[3] == 'l' || end[3] == 'L') &&
          !oyCheckPolicy_(full_name) )
      {
        if(l->count_files >= l->mem_count)
        {
          char** temp = l->names;
          l->names = (char**) calloc (sizeof(char*), l->mem_count+l->hopp);
          memcpy(l->names, temp, sizeof(char*) * l->mem_count);
          l->mem_count += l->hopp;
        }

        oyAllocString_m_( l->names[l->count_files], oyStrblen_(full_name),
                          oyAllocateFunc_, return 1 );
        
        oyStrcpy_(l->names[l->count_files], full_name);
        ++l->count_files;
      } /*else */
        /*WARNc_S(("%s in %s is not a valid profile", file_name, path)); */
  return 0;
}

int oyFileListCb_ (void* data, const char* full_name, const char* filename)
{
  struct oyFileList_s_ *l = (struct oyFileList_s_*)data;
  /* last 4 chars */
  const char * end = NULL;

  if(l->type != oyOBJECT_TYPE_FILE_LIST_S_)
    WARNc_S("Could not find a oyFileList_s_ objetc.");

  if(strlen(full_name) > 4)
    end = full_name + strlen(full_name) - 4;

      /*if( (end[0] == '.') &&
          (end[1] == 'x' || end[1] == 'X') &&
          (end[2] == 'm' || end[2] == 'M') &&
          (end[3] == 'l' || end[3] == 'L') &&
          !oyCheckPolicy_(full_name) )*/
      {
        if(l->count_files >= l->mem_count)
        {
          char** temp = l->names;
          l->names = (char**) calloc (sizeof(char*), l->mem_count+l->hopp);
          memcpy(l->names, temp, sizeof(char*) * l->mem_count);
          l->mem_count += l->hopp;
        }

        oyAllocString_m_( l->names[l->count_files], oyStrblen_(full_name),
                          oyAllocateFunc_, return 1 );
        
        oyStrcpy_(l->names[l->count_files], full_name);
        ++l->count_files;
      } /*else */
        /*WARNc_S(("%s in %s is not a valid profile", file_name, path)); */
  return 0;
}

char **  oyProfileListGet_           ( const char        * coloursig,
                                       uint32_t          * size )
{
  struct oyFileList_s_ l = {oyOBJECT_TYPE_FILE_LIST_S_, 128, NULL, 128, 0, 0};
  int32_t count = 0;/*oyPathsCount_();*/
  char ** path_names = NULL;

  path_names = oyProfilePathsGet_( &count, oyAllocateFunc_ );

  l.coloursig = coloursig;

  DBG_PROG_START
 
  oy_warn_ = 0;

  l.names = 0;
  l.mem_count = l.hopp;
  l.count_files = 0;

  oyAllocHelper_m_(l.names, char*, l.mem_count, oyAllocateFunc_, return 0);

  /* add a none existant profile */
  oyAllocString_m_(l.names[l.count_files], 48, oyAllocateFunc_, return 0);
  oySnprintf1_( l.names[l.count_files++] , 48, "%s", OY_PROFILE_NONE );

  oyRecursivePaths_( oyProfileListCb_, (void*) &l,
                     (const char**)path_names, count );

  oyOptionChoicesFree( oyWIDGET_POLICY, &path_names, count );

  *size = l.count_files;
  oy_warn_ = 1;
  DBG_PROG_ENDE
  return l.names;
}

char**
oyPolicyListGet_                  (int * size)
{
  struct oyFileList_s_ l = {oyOBJECT_TYPE_FILE_LIST_S_, 128, NULL, 128, 0, 0};
  int count = 0;
  char ** path_names = NULL;
 
  DBG_PROG_START
 
  path_names = oyConfigPathsGet_( &count, "settings", oyALL, oyUSER_SYS,
                                  oyAllocateFunc_ );

  oy_warn_ = 0;

  l.names = 0;
  l.mem_count = l.hopp;
  l.count_files = 0;

  oyAllocHelper_m_(l.names, char*, l.mem_count, oyAllocateFunc_, return 0);

  oyRecursivePaths_(oyPolicyListCb_, (void*)&l,(const char**)path_names, count);

  oyStringListRelease_(&path_names, count, oyDeAllocateFunc_);

  *size = l.count_files;
  oy_warn_ = 1;
  DBG_PROG_ENDE
  return l.names;
}

/** @internal
 *  @brief get files out of the Oyranos default paths
 *
 *  Since: 0.1.8
 */
char**
oyFileListGet_                  (const char * subpath,
                                 int        * size,
                                 int          data,
                                 int          owner)
{
  struct oyFileList_s_ l = {oyOBJECT_TYPE_FILE_LIST_S_, 128, NULL, 128, 0, 0};
  int count = 0;
  char ** path_names = NULL;
 
  DBG_PROG_START
 
  path_names = oyConfigPathsGet_( &count, subpath, oyALL, oyUSER_SYS,
                                  oyAllocateFunc_ );

  oy_warn_ = 0;

  l.names = 0;
  l.mem_count = l.hopp;
  l.count_files = 0;

  oyAllocHelper_m_(l.names, char*, l.mem_count, oyAllocateFunc_, return 0);

  oyRecursivePaths_(oyFileListCb_, (void*) &l, (const char**)path_names, count);

  oyStringListRelease_(&path_names, count, oyDeAllocateFunc_);

  *size = l.count_files;

  if(!l.count_files && l.names)
  {
    oyDeAllocateFunc_(l.names);
    l.names = 0;
  }

  oy_warn_ = 1;
  DBG_PROG_ENDE
  return l.names;
}

/** @internal
 *  @brief get files out of the Oyranos default paths
 *
 *  Since: 0.1.8
 */
char**
oyLibListGet_                   (const char * subpath,
                                 int        * size,
                                 int          owner)
{
  struct oyFileList_s_ l = {oyOBJECT_TYPE_FILE_LIST_S_, 128, NULL, 128, 0, 0};
  int count = 0;
  char ** path_names = NULL;
 
  DBG_PROG_START
 
  path_names = oyLibPathsGet_( &count, subpath, oyUSER_SYS,
                                  oyAllocateFunc_ );

  oy_warn_ = 0;

  l.names = 0;
  l.mem_count = l.hopp;
  l.count_files = 0;

  oyAllocHelper_m_(l.names, char*, l.mem_count, oyAllocateFunc_, return 0);

  oyRecursivePaths_(oyFileListCb_, (void*) &l, (const char**)path_names, count);

  oyStringListRelease_(&path_names, count, oyDeAllocateFunc_);

  *size = l.count_files;
  oy_warn_ = 1;
  DBG_PROG_ENDE
  return l.names;
}

typedef int (*pathSelectFunc_t_)(void*,const char*,const char*);

/** @internal
 *  doInPath and data must fit, doInPath can operate on data and after finishing
 *  oyRecursivePaths_ data can be processed further

 * TODO: move specifying paths out as arguments

 */
int
oyRecursivePaths_  ( pathSelectFunc_t_ doInPath,
                     void* data,
                     const char ** path_names,
                     int count )
{
 
  int r = 0;
  int i;

  static int war = 0;

  DBG_PROG_START

  ++war;
  if(war >= 413)
    ;/* WARNc_S(("schon %d mal\n", war)); */

  for(i = 0; i < count ; ++i)
  {
    struct stat statbuf;
    struct dirent *entry[MAX_DEPTH];
    DIR *dir[MAX_DEPTH];
    const char *path = path_names[i];
    int l = 0; /* level */
    int run = !r;
    int path_is_double = 0;
    int j;

    /* check for doubling of paths, without checking recursively */
    {
      for( j = 0; j < i; ++j )
      { const char *p  = path_names[j];
        char *pp = oyMakeFullFileDirName_( p );
        if( p && pp &&
            (strcmp( path, pp ) == 0) )
          path_is_double = 1;
        oyFree_m_( pp );
      }
      for( j = 0; j < MAX_DEPTH; ++j )
      {
        dir[j] = NULL;
        entry[j] = NULL;
      }
    }
    if( path_is_double )
      continue;

    if ((stat (path, &statbuf)) != 0) {
      switch (errno)
      {
        case EACCES:       WARNc2_S("Permission denied: %s %d", path, i); break;
        case EIO:          WARNc2_S("EIO : %s %d", path, i); break;
        case ELOOP:        WARNc2_S("Too many symbolic links encountered while traversing the path: %s %d", path, i); break;
        case ENAMETOOLONG: WARNc2_S("ENAMETOOLONG : %s %d", path, i); break;
        case ENOENT:       DBG_PROG2_S("A component of the path file_name does not exist, or the path is an empty string: \"%s\" %d", path, i); break;
        case ENOTDIR:      WARNc2_S("ENOTDIR : %s %d", path, i); break;
        case EOVERFLOW:    WARNc2_S("EOVERFLOW : %s %d", path, i); break;
      }
      continue;
    }
    if (!S_ISDIR (statbuf.st_mode)) {
      WARNc3_S("%d. %s %s", i, path, _("path is not a directory"));
      continue;
    }
    if (S_ISLNK (statbuf.st_mode)) {
      WARNc3_S("%d. %s %s", i, path, _("path is a link: ignored"));
      continue;
    }
    dir[l] = opendir (path);
    if (!dir[l]) {
      WARNc3_S("%d. %s %s", i, path, _("%d. path %s is not readable"));
      continue;
    }

    while(run)
    {
      char name[256];
      int k;

      if(l>=64) WARNc_S("max path depth reached: 64");
      if(dir[l] == NULL)
      {
        WARNc_S("NULL");
        --l;
        if(l<0) run = 0;
        goto cont;
      }
      if(!(entry[l] = readdir (dir[l]))) {
        closedir(dir[l]);
        dir[l] = NULL;
        --l;
        if(l<0)
          run = 0;
        goto cont;
      }

      oySprintf_(name,path);
      for (k=0; k <= l; ++k) {
        int len = strlen(name);
        if(len+strlen(entry[k]->d_name) < 256)
          oySprintf_(&name[strlen(name)],"/%s", entry[k]->d_name);
        else {
          DBG_PROG3_S("%d. %s/%s ignored", l, name, entry[k]->d_name)
          goto cont;
        }
      }

      if ((strcmp (entry[l]->d_name, "..") == 0) ||
          (strcmp (entry[l]->d_name, ".") == 0)) {
        DBG_PROG2_S("%d. %s ignored", l, name)
        goto cont;
      }
      if ((stat (name, &statbuf)) != 0) {
        DBG_PROG2_S("%d. %s does not exist", l, name)
        goto cont;
      }
      if (!S_ISLNK(statbuf.st_mode)){/*((statbuf.st_mode & S_IFMT) & S_IFLNK))  */
        DBG_PROG5_S("%d. %s is a link: ignored %d %d %d", l, name, (int)statbuf.st_mode , S_IFLNK, 0120000);
        /*goto cont; */
      }
      if (S_ISDIR (statbuf.st_mode) &&
          l < MAX_DEPTH ) {

        dir[l+1] = opendir (name);
        ++l;
        DBG_PROG2_S("%d. %s directory", l, name);
        goto cont;
      }
      if(!S_ISREG (statbuf.st_mode)) {
        DBG_PROG2_S("%d. %s is a non regular file", l, name);
        goto cont;
      }
      DBG_PROG2_S( "%d. a valid file %s", l, name )

      /* use all file extensions */
      /* go recursively without following links, due to security */
      if( !r ) {
        r = doInPath(data, name, entry[l]->d_name);
        run = !r;
        if(r)
          DBG3_S("%d. %d %d found", i, r, run);
      }

      cont:
        ;
    }

    for( j = 0; j < MAX_DEPTH; ++j )
      {
        if(dir[j]) closedir(dir[j]);;
        dir[j] = NULL;
      }
  }

  DBG_PROG_ENDE
  return r;
}

/* profile handling API */

size_t
oyGetProfileSize_                  (const char* profilename)
{
  size_t size = 0;
  char* fullFileName = oyFindProfile_ (profilename);

  DBG_PROG_START

  size = oyReadFileSize_ (fullFileName);

  DBG_PROG_ENDE
  return size;
}

void*
oyGetProfileBlock_                 (const char* profilename, size_t *size,
                                    oyAllocFunc_t allocate_func)
{
  char* fullFileName = 0;
  char* block = 0;

  DBG_PROG_START

  fullFileName = oyFindProfile_ (profilename);

  if(fullFileName)
  {
    block = oyReadFileToMem_ (fullFileName, size, allocate_func);
    oyFree_m_( fullFileName );
  }

  DBG_PROG_ENDE
  return block;
}




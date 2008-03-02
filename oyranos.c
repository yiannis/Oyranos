/*
 * Oyranos is an open source Colour Management System 
 * 
 * Copyright (C) 2004  Kai-Uwe Behrmann
 *
 * Autor: Kai-Uwe Behrmann <ku.b@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * -----------------------------------------------------------------------------
 *
 * sorting
 * 
 */

/* Date:      25. 11. 2004 */

#define DEBUG 1
int oy_debug = 0;

#include <kdb.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "oyranos_helper.h"
#include "oyranos_definitions.h"

/* ---  Helpers  --- */
int level_PROG = 0;
clock_t _oyranos_clock = 0;
#define ERR if (rc) { printf("%s:%d\n", __FILE__,__LINE__); perror("Error"); }



/* --- internal API definition --- */

/* separate from the external functions */
int	_oyPathsCount         (void);
char*	_oyPathName           (int number);
int	_oyPathAdd            (char* pathname);
void	_oyPathRemove         (char* pathname);
void	_oyPathSleep          (char* pathname);
void	_oyPathActivate       (char* pathname);

int	_oySetDefaultImageProfile          (char* name);
int	_oySetDefaultImageProfileBlock     (char* name, void* mem, size_t size);
int	_oySetDefaultWorkspaceProfile      (char* name);
int	_oySetDefaultWorkspaceProfileBlock (char* name, void* mem, size_t size);
int	_oySetDefaultCmykProfile           (char* name);
int	_oySetDefaultCmykProfileBlock      (char* name, void* mem, size_t size);

char*	_oyGetDefaultImageProfileName      ();
char*	_oyGetDefaultWorkspaceProfileName  ();
char*	_oyGetDefaultCmykProfileName       ();

int	_oyCheckProfile                    (char* name);
int	_oyCheckProfileMem                 (void* mem, size_t size);

size_t	_oyGetProfileSize                  (char* profilename);
void*	_oyGetProfileBlock                 (char* profilename, size_t* size);

char*   _oyGetDeviceProfile               (char* manufacturer,
                                           char* model,
                                           char* product_id,
                                           char* host,
                                           char* port,
                                           char* attrib1,
                                           char* attrib2,
                                           char* attrib3);
char**  _oyGetDeviceProfiles              (char* manufacturer,
                                           char* model,
                                           char* product_id,
                                           char* host,
                                           char* port,
                                           char* attrib1,
                                           char* attrib2,
                                           char* attrib3,
                                           int** number);
int     _oySetDeviceProfile               (char* manufacturer,
                                           char* model,
                                           char* product_id,
                                           char* host,
                                           char* port,
                                           char* attrib1,
                                           char* attrib2,
                                           char* attrib3,
                                           char* profileName,
                                           void* mem,
                                           size_t size);



/* elektra key wrapper */
int _oyAddKey_valueComment (char* keyName, char* value, char* comment);
int _oyAddKey_value (char* keyName, char* value);

/* elektra key list handling */
char* _oySearchEmptyKeyname (char* keyParentName, char* keyBaseName);
KeySet* _oyReturnChildrenList (char* keyParentName, int* rc);

/* complete an name from file including oyResolveDirFileName */
char* _oyMakeFullFileDirName (char* name);
/* find an file/dir and do corrections on  ~ ; ../  */
char* _oyResolveDirFileName (char* name);
char* _oyGetHomeDir ();
char* _oyGetParent (char* name);

int _oyIsDir (char* path);
int _oyIsFile (char* fileName);
int _oyIsFileFull (char* fullFileName);
int _oyMakeDir (char* path);

int   _oyWriteMemToFile(char* name, void* mem, size_t size);
char* _oyReadFileToMem(char* fullFileName, size_t *size);

/* oyranos part */
/* check for the global and the users directory */
void _oyCheckDefaultDirectories ();
/* search in profile path and in current path */
char* _oyFindProfile (char* name);

/* Profile registring */
int _oySetProfile      (char* name, char* typ, char* comment);
int _oySetProfileBlock (char* name, void* mem, size_t size, char* typ, char* comnt);

/* small search engine
 *
 * for one simple, single list, dont mix lists!!
 * name and val are not alloced or freed 
 */

struct Comp {
  struct Comp *next;
  struct Comp *begin;
  char* name;
  char* val;
  int   hits;
};

typedef struct Comp comp;

comp* initComp (comp *compare, comp *top);
comp* appendComp (comp *list, comp *new);
void destroyCompList (comp* list);

comp*   _oyGetDeviceProfilesList          (char* manufacturer,
                                           char* model,
                                           char* product_id,
                                           char* host,
                                           char* port,
                                           char* attrib1,
                                           char* attrib2,
                                           char* attrib3,
                                           KeySet* profilesList,
                                           int   rc);


/* --- function definitions --- */

KeySet*
_oyReturnChildrenList (char* keyParentName, int* rc)
{ DBG_PROG_START
  KeySet *myKeySet = (KeySet*) malloc (sizeof(KeySet) * 1);
  ksInit(myKeySet);

  *rc = kdbGetChildKeys(keyParentName, myKeySet,KDB_O_RECURSIVE);

  DBG_PROG_ENDE
  return myKeySet;
}

char*
_oySearchEmptyKeyname (char* keyParentName, char* keyBaseName)
{ DBG_PROG_START
  char* keyName = (char*)     calloc (strlen(keyParentName)
                                    + strlen(keyBaseName) + 24, sizeof(char));
  char* pathkeyName = (char*) calloc (strlen(keyBaseName) + 24, sizeof(char));
  int nth = 0, i = 1, rc;
  Key key;

  rc=keyInit(&key); ERR

    /* search for empty keyname */
    while (!nth)
    { sprintf (pathkeyName , "%s%d", keyBaseName, i);
      rc=kdbGetKeyByParent (keyParentName, pathkeyName, &key);
      if (rc != KDB_RET_OK)
        nth = i;
      i++;
    }
    sprintf (keyName, ("%s/%s"), OY_USER_PATHS, pathkeyName);

  DBG_PROG_ENDE
  return keyName;
} 

int
_oyAddKey_valueComment (char* keyName, char* value, char* comment)
{ DBG_PROG_START
  int rc;
  Key key;

    rc=keyInit(&key); ERR
    rc=keySetName (&key, keyName);
    rc=kdbGetKey(&key);
    rc=keySetString (&key, value);
    rc=keySetComment (&key, comment);
    rc=kdbSetKey(&key);

  DBG_PROG_ENDE
  return rc;
}

int
_oyAddKey_value (char* keyName, char* value)
{ DBG_PROG_START
  int rc;
  Key key;

    rc=keyInit(&key); ERR
    rc=keySetName (&key, keyName);
    rc=kdbGetKey(&key);
    rc=keySetString (&key, value);
    rc=kdbSetKey(&key);

  DBG_PROG_ENDE
  return rc;
}

char*
_oyReadFileToMem(char* name, size_t *size)
{ DBG_PROG_START
  FILE *fp = 0;
  char* mem = 0;
  char* filename = name;

  DBG_PROG

  {
    fp = fopen(filename, "r");
    DBG_PROG_S (("fp = %d filename = %s\n", (int)fp, filename))

    if (fp)
    {
      /* get size */
      fseek(fp,0L,SEEK_END); 
      *size = ftell (fp);
      rewind(fp);

      /* allocate memory */
      mem = (char*) calloc (*size, sizeof(char));

      /* check and read */
      if ((fp != 0)
       && mem
       && *size)
      { DBG_PROG
        int s = fread(mem, sizeof(char), *size, fp);
        /* check again */
        if (s != *size)
        { *size = 0;
          free (mem);
          mem = 0;
        }
      }
    } else {
      printf ("could not read %s\n", filename);
    }
  }
 
  /* clean up */
  if (fp) fclose (fp);

  DBG_PROG_ENDE
  return mem;
}

int
_oyWriteMemToFile(char* name, void* mem, size_t size)
{ DBG_PROG_START
  FILE *fp = 0;
  int   pt = 0;
  char* block = mem;
  char* filename;
  int r = 0;

  DBG_PROG_S(("name = %s mem = %d size = %d\n", name, (int)mem, size))

  filename = name;

  {
    fp = fopen(filename, "w");
    DBG_PROG_S(("fp = %d filename = %s", (int)fp, filename))
    if ((fp != 0)
     && mem
     && size)
    { DBG_PROG
      do {
        r = fputc ( block[pt++] , fp);
      } while (--size);
    }

    if (fp) fclose (fp);
  }

  DBG_PROG_ENDE
  return r;
}

char*
_oyGetHomeDir ()
{ DBG_PROG_START
  #if (__unix__ || __APPLE__)
  char* name = (char*) getenv("HOME");
  DBG_PROG_S((name))
  DBG_PROG_ENDE
  return name;
  #else
  DBG_PROG_ENDE
  return "OS not supported yet";
  #endif
}

char*
_oyGetParent (char* name)
{ DBG_PROG_START
  char *parentDir = (char*) calloc ( MAX_PATH, sizeof(char)), *ptr;

  sprintf (parentDir, name);
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

  DBG_PROG_S((parentDir))

  DBG_PROG_ENDE
  return parentDir;
}

int
_oyIsDir (char* path)
{ DBG_PROG_START
  struct stat status;
  int r = 0;
  char* name = _oyResolveDirFileName (path);
  status.st_mode = 0;
  r = stat (name, &status);
  DBG_PROG_S(("status.st_mode = %d", (status.st_mode&S_IFMT)&S_IFDIR))
  DBG_PROG_S(("status.st_mode = %d", status.st_mode))
  DBG_PROG_S(("name = %s ", name))
  if (name) free (name);
  r = !r &&
       ((status.st_mode & S_IFMT) & S_IFDIR);

  DBG_PROG_ENDE
  return r;
}

int
_oyIsFileFull (char* fullFileName)
{ DBG_PROG_START
  struct stat status;
  int r = 0;
  char* name = fullFileName;
  status.st_mode = 0;
  r = stat (name, &status);
  DBG_NUM_S(("status.st_mode = %d", (status.st_mode&S_IFMT)&S_IFDIR))
  DBG_NUM_S(("status.st_mode = %d", status.st_mode))
  DBG_NUM_S(("name = %s", name))
  r = !r &&
       (   ((status.st_mode & S_IFMT) & S_IFREG)
        || ((status.st_mode & S_IFMT) & S_IFLNK));

  if (r)
  {
    FILE* fp = fopen (name, "r"); DBG_PROG
    if (!fp)
      r = 0;
    else
      fclose (fp);
  } DBG_PROG

  DBG_PROG_ENDE
  return r;
}

int
_oyIsFile (char* fileName)
{ DBG_PROG_START
  int r = 0;
  char* name = _oyResolveDirFileName (fileName);

  r = _oyIsFileFull(name);

  if (name) free (name); DBG_PROG

  DBG_PROG_ENDE
  return r;
}

int
_oyMakeDir (char* path)
{ DBG_PROG_START
  char *name = _oyResolveDirFileName (path);
  int rc = 0;
  mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* 0755 */
  DBG_PROG
  rc = mkdir (name, mode);
  free (name);

  DBG_PROG_ENDE
  return rc;
}

char*
_oyResolveDirFileName (char* name)
{ DBG_PROG_START
  char* newName = (char*) calloc (MAX_PATH, sizeof(char)),
       *home = 0;
  int len = 0;

  DBG_PROG_S((name))

  /* user directory */
  if (name[0] == '~')
  { DBG_PROG_S(("in home directory"))
    home = _oyGetHomeDir();
    len = strlen(name) + strlen(home) + 1;
    if (len >  FILENAME_MAX)
      WARN_S(("file name is too long %d\n", len))

    sprintf (newName, "%s%s", home, &name[0]+1);

  } else { DBG_PROG_S(("resolve  directory"))
    sprintf (newName, name);

    /* relative names - where the first sign is no directory separator */
    if (newName[0] != OY_SLASH_C)
    { char* cn = (char*) calloc(MAX_PATH, sizeof(char)); DBG_PROG
      sprintf (cn, "%s%s%s", getenv("PWD"), OY_SLASH, name);
      DBG_PROG_S(("canonoical %s ", cn))
      sprintf (newName, cn);
    }
  }

  DBG_PROG_S (("name %s", name))
  DBG_PROG_S (("home %s", home))
  DBG_PROG_S (("newName = %s", newName))

  DBG_PROG_ENDE
  return newName;
}

char*
_oyMakeFullFileDirName (char* name)
{ DBG_PROG_START
  char *newName;
  char *dirName = 0;

  DBG_PROG
  if(name &&
     strrchr( name, OY_SLASH_C ))
  { DBG_PROG
    /* substitute ~ with HOME variable from environment */
    newName = _oyResolveDirFileName (name);
  } else
  { DBG_PROG
    /* create directory name */
    newName = (char*) calloc (MAX_PATH, sizeof(char)),
    dirName = (char*) getenv("PWD");
    sprintf (newName, "%s%s", dirName, OY_SLASH);
    if (name)
      sprintf (strrchr(newName,OY_SLASH_C)+1, "%s", name);
    DBG_PROG_S(("newName = %s", newName))
  }

  DBG_PROG_S(("newName = %s", newName))

  DBG_PROG_ENDE
  return newName;
}

void
_oyCheckDefaultDirectories ()
{ DBG_PROG_START
  char* parentDefaultUserDir;

  /* test dirName : existing in path, default dirs are existing */
  if (!_oyIsDir (OY_DEFAULT_SYSTEM_PROFILE__PATH))
  { DBG_PROG
    printf ("no default system directory %s\n",OY_DEFAULT_SYSTEM_PROFILE__PATH);
  }

  if (!_oyIsDir (OY_DEFAULT_USER_PROFILE_PATH))
  { DBG_PROG 
    parentDefaultUserDir = _oyGetParent (OY_DEFAULT_USER_PROFILE_PATH);

    if (!_oyIsDir (parentDefaultUserDir))
    { DBG_PROG 
      printf ("Try to create part of users default directory %s %d \n",
                 parentDefaultUserDir,
      _oyMakeDir( parentDefaultUserDir)); DBG_PROG
    }
    free (parentDefaultUserDir);

    printf ("Try to create users default directory %s %d \n",
               OY_DEFAULT_USER_PROFILE_PATH,
    _oyMakeDir( OY_DEFAULT_USER_PROFILE_PATH )); DBG_PROG
  }
  DBG_PROG_ENDE
}

char*
_oyFindProfile (char* fileName)
{ DBG_PROG_START
  char  *fullFileName = 0;
  int    success = 0;
  char  *header = 0;
  size_t size;

  DBG_NUM_S((fileName))
  /* test for pure file without dir; search in configured paths only */
  if (fileName && !strchr(fileName, OY_SLASH_C))
  { DBG_PROG
    char* pathName = (char*) calloc (MAX_PATH, sizeof(char)); DBG_PROG
    int   n_paths = _oyPathsCount (),
          i;

    DBG_PROG_S(("pure filename found"))
    fullFileName = (char*) calloc (MAX_PATH, sizeof(char));

    for (i = 0; i < n_paths; i++)
    { /* test profile */
      char* ptr = _oyPathName (i);
      pathName = _oyMakeFullFileDirName (ptr);
      sprintf (fullFileName, "%s%s%s", pathName, OY_SLASH, fileName);

      DBG_PROG_S((pathName))
      DBG_PROG_S((fullFileName))

      if (_oyIsFileFull(fullFileName))
      {
        header = _oyReadFileToMem (fullFileName, &size);
        if (size >= 128)
          success = !_oyCheckProfileMem (header, 128);
      }

      if (ptr) free (ptr);
      if (header) free (header);

      if (success) /* found */
        break;
    }

    if (!success)
      WARN_S( ("profile %s not found in colour path\n", fileName))

    if (pathName) free (pathName);

  } else
  {/* else use fileName as an full qualified name, check name and test profile*/
    DBG_PROG_S(("dir/filename found"))
    fullFileName = _oyMakeFullFileDirName (fileName);

    if (_oyIsFileFull(fullFileName))
    {
      header = _oyReadFileToMem (fullFileName, &size);

      if (size >= 128)
        success = !_oyCheckProfileMem (header, 128);
    }

    if (!success)
      WARN_S (("profile %s not found\n", fileName))

    if (header) free (header);
  }

  if (!success)
  { free (fullFileName);
    fullFileName = 0;
  }

  DBG_PROG_ENDE
  return fullFileName;
}

int
_oySetProfile      (char* name, char* typ, char* comment)
{ DBG_PROG_START
  int r = 1;
  char *fileName = 0, *com = comment;

  if (strrchr(name , OY_SLASH_C))
  {
    fileName = strrchr(name , OY_SLASH_C);
    fileName++;
  }
  else
    fileName = name;

  DBG_PROG_S(("name = %s typ %s", name, typ))

  if ( !_oyCheckProfile (fileName) )
  { DBG_PROG_S(("set fileName = %s as %s profile\n",fileName, typ))
           if (strstr (typ , "Image"))
        r = _oyAddKey_valueComment (OY_DEFAULT_IMAGE_PROFILE, fileName, com);
      else if (strstr (typ , "Workspace"))
        r = _oyAddKey_valueComment (OY_DEFAULT_WORKSPACE_PROFILE, fileName, com);
      else if (strstr (typ , "Cmyk"))
        r = _oyAddKey_valueComment (OY_DEFAULT_CMYK_PROFILE, fileName, com);
      else if (strstr (typ , "Device"))
      {
        int len = strlen(OY_USER OY_SLASH OY_REGISTRED_PROFILES)
                  + strlen(fileName);
        char* keyName = (char*) calloc (len +10, sizeof(char)); DBG_PROG
        sprintf (keyName, "%s%s%s%s", OY_USER, OY_SLASH, OY_REGISTRED_PROFILES OY_SLASH, fileName); DBG_PROG
        r = _oyAddKey_valueComment (keyName, com, 0); DBG_PROG
        //DBG_PROG_V(("%s %d", keyName, len))
        if (keyName) free(keyName);
      }
      else
        printf ("%s:%d !!! ERROR typ %s type does not exist for default profiles",__FILE__,__LINE__, typ);
  }

  DBG_PROG_ENDE
  return r;
}


/* public API implementation */

/* path names API */


int
_oyPathsCount ()
{ DBG_PROG_START
  int rc, n = 0;
  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR
  n = myKeySet->size;

  ksClose (myKeySet);
  kdbClose();

  DBG_PROG_ENDE
  return n;
}

char*
_oyPathName (int number)
{ DBG_PROG_START
  int rc, n = 0;
  Key *current;
  char* value = (char*) calloc (sizeof(char), MAX_PATH);

  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR

  if (number <= myKeySet->size)
    for (current=myKeySet->start; current; current=current->next)
    {
      if (number == n) {
        keyGetComment (current, value, MAX_PATH);
        if (strstr(value, OY_SLEEP) == 0)
          keyGetString(current, value, MAX_PATH);
      }
      n++;
    }

  ksClose (myKeySet);
  kdbClose();

  DBG_PROG_ENDE
  return value;
}

int
_oyPathAdd (char* pfad)
{ DBG_PROG_START
  int rc, n = 0;
  Key *current;
  char* keyName = (char*) calloc (sizeof(char), MAX_PATH);
  char* value = (char*) calloc (sizeof(char), MAX_PATH);

  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR

  /* search for allready included path */
  for (current=myKeySet->start; current; current=current->next)
  {
    keyGetString(current, value, MAX_PATH);
    if (strcmp (value, pfad) == 0)
      n++;		
  }

  if (n) printf ("Key was allready %d times there\n",n);

  /* erase double occurencies of this path */
  if (n > 1)
  { for (current=myKeySet->start; current; current=current->next)
    {
      rc=keyGetString(current,value, MAX_PATH); ERR

      if (strcmp (value, pfad) == 0
       && n)
      {
        rc=keyGetFullName(current,keyName, MAX_PATH); ERR
        rc=kdbRemove(keyName); ERR
        n--;
      }
    }
  }

  /* create new key */
  if (!n)
  {
    /* search for empty keyname */
    keyName = _oySearchEmptyKeyname (OY_USER_PATHS, OY_USER_PATH);

    /* write key */
    rc = _oyAddKey_valueComment (keyName, pfad, "");
  }

  ksClose (myKeySet);
  kdbClose();
  free (keyName);
  free (value);

  _oyCheckDefaultDirectories();

  DBG_PROG_ENDE
  return rc;
}

void
_oyPathRemove (char* pfad)
{ DBG_PROG_START
  int rc;
  Key *current;
  char* value = (char*) calloc (sizeof(char), MAX_PATH);
  char* keyName = (char*) calloc (sizeof(char), MAX_PATH);

  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR

  /* compare and erase if matches */
  for (current=myKeySet->start; current; current=current->next)
  {
    keyGetString(current, value, MAX_PATH);
    if (strcmp (value, pfad) == 0)
    {
      keyGetFullName(current,keyName, MAX_PATH); ERR
      kdbRemove (keyName);
      DBG_NUM_S(( "remove" ))
    }
  }

  ksClose (myKeySet);
  kdbClose();
  free (keyName);
  free (value);

  _oyPathAdd (OY_DEFAULT_USER_PROFILE_PATH);

  DBG_PROG_ENDE
}

void
_oyPathSleep (char* pfad)
{ DBG_PROG_START
  int rc;
  Key *current;
  char* value = (char*) calloc (sizeof(char), MAX_PATH);

  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR

  /* set "SLEEP" in comment */
  for (current=myKeySet->start; current; current=current->next)
  {
    keyGetString(current, value, MAX_PATH);
    if (strcmp (value, pfad) == 0)
    {
      keySetComment (current, OY_SLEEP);
      kdbSetKey (current);
      DBG_NUM_S(( "sleep" ))
    }
  }

  ksClose (myKeySet);
  kdbClose();
  free (value);
  DBG_PROG_ENDE
}

void
_oyPathActivate (char* pfad)
{ DBG_PROG_START
  int rc;
  Key *current;
  char* value = (char*) calloc (sizeof(char), MAX_PATH);

  kdbOpen();

  /* take all keys in the paths directory */
  KeySet* myKeySet = _oyReturnChildrenList(OY_USER_PATHS, &rc ); ERR

  /* erase "SLEEP" from comment */
  for (current=myKeySet->start; current; current=current->next)
  {
    keyGetString(current, value, MAX_PATH);
    if (strcmp (value, pfad) == 0)
    {
      keySetComment (current, "");
      kdbSetKey (current);
      DBG_NUM_S(( "wake up" ))
    }
  }

  ksClose (myKeySet);
  kdbClose();
  free (value);
  DBG_PROG_ENDE
}

/* default profiles API */

int
_oySetDefaultImageProfile          (char* name)
{ DBG_PROG_START
  int r = _oySetProfile (name, "Image", 0);
  DBG_PROG_ENDE
  return r;
}

int
_oySetDefaultWorkspaceProfile      (char* name)
{ DBG_PROG_START
  int r = _oySetProfile (name, "Workspace", 0);
  DBG_PROG_ENDE
  return r;
}

int
_oySetDefaultCmykProfile           (char* name)
{ DBG_PROG_START
  int r = _oySetProfile (name, "Cmyk", 0);
  DBG_PROG_ENDE
  return r;
}

int
_oySetDefaultImageProfileBlock     (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int r = _oySetProfileBlock (name, mem, size, "Image", 0);
  DBG_PROG_ENDE
  return r;
}

int
_oySetDefaultWorkspaceProfileBlock (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int r = _oySetProfileBlock (name, mem, size, "Workspace", 0);
  DBG_PROG_ENDE
  return r;
}

int
_oySetDefaultCmykProfileBlock      (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int r = _oySetProfileBlock (name, mem, size, "Cmyk", 0);
  DBG_PROG_ENDE
  return r;
}

char*
_oyGetDefaultImageProfileName      ()
{ DBG_PROG_START
  char* name = (char*) calloc (MAX_PATH, sizeof(char));
  
  kdbGetValue (OY_DEFAULT_IMAGE_PROFILE, name, MAX_PATH);

  DBG_PROG_S((name))
  DBG_PROG_ENDE
  return name;
}

char*
_oyGetDefaultWorkspaceProfileName  ()
{ DBG_PROG_START
  char* name = (char*) calloc (MAX_PATH, sizeof(char));

  kdbGetValue (OY_DEFAULT_WORKSPACE_PROFILE, name, MAX_PATH);

  DBG_PROG_S((name))
  DBG_PROG_ENDE
  return name;
}

char*
_oyGetDefaultCmykProfileName       ()
{ DBG_PROG_START
  char* name = (char*) calloc (MAX_PATH, sizeof(char));

  kdbGetValue (OY_DEFAULT_CMYK_PROFILE, name, MAX_PATH);

  DBG_PROG_S((name))
  DBG_PROG_ENDE
  return name;
}


/* profile check API */

int
_oyCheckProfile                    (char* name)
{ DBG_PROG_START
  char *fullName = 0;
  char* header; 
  size_t size = 0;
  int r = 1;

  DBG_NUM_S((name))
  fullName = _oyFindProfile(name);
  if (!fullName)
    WARN_S(("%s not found",name))
  DBG_NUM_S((fullName))

  /* do check */
  if (_oyIsFileFull(fullName))
  {
    header = _oyReadFileToMem (fullName, &size); DBG_PROG
    if (size >= 128)
      r = _oyCheckProfileMem (header, 128);
  }

  DBG_NUM_S(("oyCheckProfileMem = %d",r))

  DBG_PROG_ENDE
  return r;
}

int
_oyCheckProfileMem                 (void* mem, size_t size)
{ DBG_PROG_START
  char* block = (char*) mem;
  int offset = 36;
  if (size >= 128 &&
      block[offset+0] == 'a' &&
      block[offset+1] == 'c' &&
      block[offset+2] == 's' &&
      block[offset+3] == 'p' )
  {
    DBG_PROG_ENDE
    return 0;
  } else
  {
    WARN_S (("False profile - size = %d pos = %lu ", size, (long int)block))
    if (size >= 128)
      printf(" sign: %c%c%c%c ", (char)block[offset+0], (char)block[offset+1], (char)block[offset+2], (char)block[offset+3] );

    DBG_PROG_ENDE
    return 1;
  }
}

/* profile handling API */

size_t
_oyGetProfileSize                  (char* profilename)
{ DBG_PROG_START
  size_t size = 0;
  char* fullFileName = _oyFindProfile (profilename);
  char* dummy;

  dummy = _oyReadFileToMem (fullFileName, &size);
  if (dummy) free (dummy);

  DBG_PROG_ENDE
  return size;
}

void*
_oyGetProfileBlock                 (char* profilename, size_t* size)
{ DBG_PROG_START
  char* fullFileName = _oyFindProfile (profilename);
  char* block = _oyReadFileToMem (fullFileName, size);

  DBG_PROG_ENDE
  return block;
}

int
_oySetProfileBlock (char* name, void* mem, size_t size, char* typ, char* comnt)
{ DBG_PROG_START
  int r = 0;
  char *fullFileName, *fileName, *resolvedFN;

  if (strrchr (name, OY_SLASH_C))
    fileName = strrchr (name, OY_SLASH_C);
  else
    fileName = name;

  fullFileName = (char*) calloc (sizeof(char),
                  strlen(OY_DEFAULT_USER_PROFILE_PATH) + strlen (fileName) + 4);

  sprintf (fullFileName, "%s%s%s",
           OY_DEFAULT_USER_PROFILE_PATH, OY_SLASH, fileName);

  resolvedFN = _oyResolveDirFileName (fullFileName);
  free (fullFileName);
  fullFileName = resolvedFN;

  if (!_oyCheckProfileMem( mem, size))
  {
    DBG_PROG_S((fullFileName))
    if ( _oyIsFile(fullFileName) )
      WARN_S (("file %s exist , please remove befor installing new profile\n", fullFileName))
    else
    { r = _oyWriteMemToFile (fullFileName, mem, size);
      _oySetProfile ( name, typ, comnt);
    }
  }

  DBG_PROG_S(("%s", name))
  DBG_PROG_S(("%s", fileName))
  DBG_PROG_S(("%ld %d", (long int)&((char*)mem)[0] , size))
  free (fullFileName);

  DBG_PROG_ENDE
  return r;
}

/* small search engine */

comp*
initComp (comp *compare, comp *top)
{ DBG_PROG_START
  if (!compare)
    compare = (comp*) calloc (1, sizeof(comp));

  compare->next = 0;

  if (top)
    compare->begin = top;
  else
    compare->begin = compare;
  compare->name = 0;
  compare->val = 0;
  compare->hits = 0;
  DBG_PROG_ENDE

  return compare;
}

comp*
appendComp (comp *list, comp *new)
{ DBG_PROG_START

  if (!list)
    list = initComp(list,0);

  list = list->begin;
  while (list->next)
    list = list->next;

  if (!new)
    new = initComp(new, list->begin);

  new->begin = list->begin;
  list->next = new;

  DBG_PROG_ENDE
  return new;
}

void
destroyCompList (comp *list)
{ DBG_PROG_START
  comp *before;

  list = list->begin;
  while (list->next)
  {
    before = list;
    list = list->next;
    free (before);
  }
  free (list);

  DBG_PROG_ENDE
}

char*
printComp (comp* entry)
{ DBG_PROG_START
  #ifdef DEBUG
  static char text[MAX_PATH] = {0};
  DBG_PROG_S(("%d",entry))
  sprintf( text, "begin %d next %d\nname %s val %s hits %d\n",
           entry->begin, entry->next, entry->name, entry->val, entry->hits);

  DBG_PROG_ENDE
  return text;
  #else
  return 0;
  #endif
}


/* device profiles API */

char*
_oyGetDeviceProfile                (char* manufacturer,
                                    char* model,
                                    char* product_id,
                                    char* host,
                                    char* port,
                                    char* attrib1,
                                    char* attrib2,
                                    char* attrib3)
{ DBG_PROG_START
  char* profileName = 0;
  int rc;

  comp *matchList = 0,
       *testEntry = 0,
       *foundEntry = 0;
  KeySet* profilesList;

  kdbOpen();

  // TODO merge User and System KeySets in _oyReturnChildrenList
  profilesList = _oyReturnChildrenList(OY_USER OY_REGISTRED_PROFILES, &rc ); ERR

  matchList = _oyGetDeviceProfilesList (manufacturer, model, product_id,
                                        host, port, attrib1, attrib2, attrib3,
                                        profilesList, rc);

  /* 6. select the profile from the match list with the most hits */
  if (matchList)
  {
    int max_hits = 0;
    foundEntry = 0;
    for (testEntry=matchList->begin; testEntry; testEntry=testEntry->next)
    {
      if (testEntry->hits > max_hits)
      {
        foundEntry = testEntry;
        max_hits = testEntry->hits;
      }
    }
    DBG_PROG_S ((printComp (foundEntry)))

    /* 7. tell about the profile and its hits */
    {
      char *fileName = 0;

      if (strrchr(foundEntry->name , OY_SLASH_C))
      {
        fileName = strrchr(foundEntry->name , OY_SLASH_C);
        fileName++;
      }
      else
        fileName = foundEntry->name;

      profileName = (char*) calloc (strlen (fileName)+1, sizeof(char));
      sprintf (profileName, fileName);

      DBG_PROG_S((foundEntry->name))
      DBG_PROG_S((profileName))
      destroyCompList (matchList);
    }
  }

  ksClose (profilesList);
  kdbClose();

  DBG_PROG_ENDE
  return profileName;
}

char**
_oyGetDeviceProfiles               (char* manufacturer,
                                    char* model,
                                    char* product_id,
                                    char* host,
                                    char* port,
                                    char* attrib1,
                                    char* attrib2,
                                    char* attrib3,
                                    int** number)
{ DBG_PROG_START
  char** profileNames = 0;
  char*  profileName = 0;
  int    rc;

  comp *matchList = 0,
       *testEntry = 0,
       *foundEntry = 0;
  KeySet* profilesList;

  kdbOpen();

  // TODO merge User and System KeySets in _oyReturnChildrenList
  profilesList = _oyReturnChildrenList(OY_USER OY_REGISTRED_PROFILES, &rc ); ERR

  matchList = _oyGetDeviceProfilesList (manufacturer, model, product_id,
                                        host, port, attrib1, attrib2, attrib3,
                                        profilesList, rc);

  /* 6. select the profile from the match list with the most hits */
  if (matchList)
  {
    int max_hits = 0;
    foundEntry = 0;
    for (testEntry=matchList->begin; testEntry; testEntry=testEntry->next)
    {
      if (testEntry->hits > max_hits)
      {
        foundEntry = testEntry;
        max_hits = testEntry->hits;
      }
    }
    DBG_PROG_S ((printComp (foundEntry)))

    /* 7. tell about the profile and its hits */
    {
      char *fileName = 0;

      if (strrchr(foundEntry->name , OY_SLASH_C))
      {
        fileName = strrchr(foundEntry->name , OY_SLASH_C);
        fileName++;
      }
      else
        fileName = foundEntry->name;

      profileName = (char*) calloc (strlen (fileName)+1, sizeof(char));
      sprintf (profileName, fileName);

      DBG_PROG_S((foundEntry->name))
      DBG_PROG_S((profileName))
      destroyCompList (matchList);
    }
  }

  ksClose (profilesList);
  kdbClose();

  DBG_PROG_ENDE
  return profileNames;
}

comp*
_oyGetDeviceProfilesList           (char* manufacturer,
                                    char* model,
                                    char* product_id,
                                    char* host,
                                    char* port,
                                    char* attrib1,
                                    char* attrib2,
                                    char* attrib3,
                                    KeySet *profilesList,
                                    int   rc)
{ DBG_PROG_START
  /* Search description
   *
   * This routine describes the A approach
   *   - registred profiles with assigned devices
   *
   * 1. take all arguments and walk through the named devices list
   *    //  named devices consist of an key with the profile name + attributes
   *    //  it is not allowed to have two profiles with the same name
   *    //  it is allowed to have different profiles for the same attributes :(
   *    //  specify more attributes to make an decission presumable
   *    //   or maintain profiles, erasing older and invalid ones
   * 2. test if attributes matches the value of the key, count the hits
   * 3. search the profile in an match list
   * 4. add the profile to the match list if not found
   * 5. increase the hits counter in the macht list for that profile
   * 6. select the profile from the match list with the most hits
   * 7. tell about the profile and its hits
   *
   * approach B: TODO
   * no attributes are assigned beside certain keyword ("monitor", "scanner")
   * scan profile tags for manufacturer, device descriptions ...
   * - When to start an automatic registration run?
   * - include profile tag editing?
   * 
   * other things: TODO
   * - spread weighting? 3 degrees are sufficient How to merge in the on string 
   *   approach?
   */

  /* 1. take all arguments and walk through the named devices list */
  int i = 0, n = 0;
  char* value = (char*) calloc (MAX_PATH, sizeof(char));
  char **attributs = (char**) calloc (8, sizeof (char*));
  Key *current;
  comp *matchList = 0,
       *testEntry = 0,
       *foundEntry = 0;

  attributs[0] = manufacturer;
  attributs[1] = model;
  attributs[2] = product_id;
  attributs[3] = host;
  attributs[4] = port;
  attributs[5] = attrib1;
  attributs[6] = attrib2;
  attributs[7] = attrib3;

  #if 0
  for (i = 0; i < 8; i++)
    DBG_PROG_S (("%ld %ld", (long int)attributs[i], (long int)model))
  #endif


  if (profilesList && !rc)
  {
    for (current=profilesList->start; current; current=current->next)
    {
      foundEntry=0;
      n = 0;
      keyGetString (current, value, MAX_PATH);

      /* 2. test if attributes matches the value of the key, count the hits */
      for (i = 0; i < 8; i++)
      {
        //DBG_PROG_S (("%d %d",value, attributs[i]))
        if (value && attributs[i] &&
            (strstr(value, attributs[i]) != 0))
        { DBG_PROG
          n++;
        }
      }
      if (n)
      { /* 3. search the profile in an match list */
        int found = 0; DBG_PROG
        if (matchList)
        {
          for (testEntry=matchList->begin; testEntry; testEntry=testEntry->next)
          {
            DBG_PROG_S(( "%s", strstr(testEntry->name, current->key) ))
            if (strstr(testEntry->name, current->key) != 0)
            { DBG_PROG
              found = 1;
              WARN_S(("double occurency of profile %s", testEntry->name))
              /* anyway increase the hits counter if attributes fits better */
              if (testEntry->hits < n)
                testEntry->hits = n;
            }
          }
        }
        /* 4. add the profile to the match list if not found (normal case) */
        if (!found)
        {
          DBG_PROG_S(( "new matching profile found %s",current->key ))
          if (!matchList)
            matchList = initComp(0,0);
          foundEntry = appendComp (matchList, 0);
          DBG_PROG_S ((printComp (foundEntry)))
          foundEntry->name = current->key; 
          foundEntry->val = current->data;
          /* 5. increase the hits counter in the match list for that profile */
          foundEntry->hits = n; 
          DBG_PROG_S ((printComp (foundEntry)))
        }
      }
    }
  } else
    WARN_S (("No profiles yet registred to devices"))

  DBG_PROG_ENDE
  return matchList;
}

int
_oySetDeviceProfile                (char* manufacturer,
                                    char* model,
                                    char* product_id,
                                    char* host,
                                    char* port,
                                    char* attrib1,
                                    char* attrib2,
                                    char* attrib3,
                                    char* profileName,
                                    void* mem,
                                    size_t size)
{ DBG_PROG_START
  int rc = 0;
  char* comment = 0;

  if (mem && size && profileName)
  {
    rc = _oyCheckProfileMem (mem, size); ERR
  }

  if (!rc)
  { DBG_PROG
    if (manufacturer || model || product_id || host || port || attrib1
        || attrib2 || attrib3)
    { int len = 0;
      DBG_PROG
      if (manufacturer) len += strlen(manufacturer);
      if (model) len += strlen(model);
      if (product_id) len += strlen(product_id);
      if (host) len += strlen(host);
      if (port) len += strlen(port);
      if (attrib1) len += strlen(attrib1);
      if (attrib2) len += strlen(attrib2);
      if (attrib3) len += strlen(attrib3);
      comment = (char*) calloc (len+10, sizeof(char)); DBG_PROG
      if (manufacturer) sprintf (comment, "%s", manufacturer); DBG_PROG
      if (model) sprintf (&comment[strlen(comment)], "%s", model); DBG_PROG
      if (product_id) sprintf (&comment[strlen(comment)], "%s", product_id);
      if (host) sprintf (&comment[strlen(comment)], "%s", host);
      if (port) sprintf (&comment[strlen(comment)], "%s", port);
      if (attrib1) sprintf (&comment[strlen(comment)], "%s", attrib1);
      if (attrib2) sprintf (&comment[strlen(comment)], "%s", attrib2);
      if (attrib3) sprintf (&comment[strlen(comment)], "%s", attrib3);
    } DBG_PROG

    rc =  _oySetProfile (profileName, "Device", comment); ERR
  }

  DBG_PROG_ENDE
  return rc;
}



/* --- internal API hidding --- */

#include "oyranos.h"

int
oyPathsCount         (void)
{ DBG_PROG_START
  int n = _oyPathsCount();
  DBG_PROG_ENDE
  return n;
}

char*
oyPathName           (int number)
{ DBG_PROG_START
  char* name = _oyPathName (number);
  DBG_PROG_ENDE
  return name;
}

int
oyPathAdd            (char* pathname)
{ DBG_PROG_START
  int n = _oyPathAdd (pathname);
  DBG_PROG_ENDE
  return n;
}

void
oyPathRemove         (char* pathname)
{ DBG_PROG_START
  _oyPathRemove (pathname);
  DBG_PROG_ENDE
}

void
oyPathSleep          (char* pathname)
{ DBG_PROG_START
  _oyPathSleep (pathname);
  DBG_PROG_ENDE
}

void
oyPathActivate       (char* pathname)
{ DBG_PROG_START
  _oyPathActivate (pathname);
  DBG_PROG_ENDE
}


int
oySetDefaultImageProfile          (char* name)
{ DBG_PROG_START
  int n =  _oySetDefaultImageProfile (name);
  DBG_PROG_ENDE
  return n;
}

int
oySetDefaultImageProfileBlock     (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int n = _oySetDefaultImageProfileBlock (name, mem, size);
  DBG_PROG_ENDE
  return n;
}

int
oySetDefaultWorkspaceProfile      (char* name)
{ DBG_PROG_START
  int n = _oySetDefaultWorkspaceProfile (name);
  DBG_PROG_ENDE
  return n;
}

int
oySetDefaultWorkspaceProfileBlock (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int n = _oySetDefaultWorkspaceProfileBlock (name, mem, size);
  DBG_PROG_ENDE
  return n;
}

int
oySetDefaultCmykProfile           (char* name)
{ DBG_PROG_START
  int n = _oySetDefaultCmykProfile (name);
  DBG_PROG_ENDE
  return n;
}

int
oySetDefaultCmykProfileBlock      (char* name, void* mem, size_t size)
{ DBG_PROG_START
  int n = _oySetDefaultCmykProfileBlock (name, mem, size);
  DBG_PROG_ENDE
  return n;
}

char*
oyGetDefaultImageProfileName      ()
{ DBG_PROG_START
  char* name = _oyGetDefaultImageProfileName ();
  DBG_PROG_ENDE
  return name;
}

char*
oyGetDefaultWorkspaceProfileName  ()
{ DBG_PROG_START
  char* name = _oyGetDefaultWorkspaceProfileName ();
  DBG_PROG_ENDE
  return name;
}

char*
oyGetDefaultCmykProfileName       ()
{ DBG_PROG_START
  char* name = _oyGetDefaultCmykProfileName ();
  DBG_PROG_ENDE
  return name;
}


int
oyCheckProfile (char* name, int flag)
{ DBG_PROG_START
  /* flag is currently ignored */
  int n = _oyCheckProfile (name);
  DBG_PROG_ENDE
  return n;
}

int
oyCheckProfileMem (void* mem, size_t size,int flag)
{ DBG_PROG_START
  /* flag is currently ignored */
  int n = _oyCheckProfileMem (mem, size);
  DBG_PROG_ENDE
  return n;
}

size_t
oyGetProfileSize                  (char* profilename)
{ DBG_PROG_START
  size_t size = _oyGetProfileSize (profilename);
  DBG_PROG_ENDE
  return size;
}

void*
oyGetProfileBlock                 (char* profilename, size_t *size)
{ DBG_PROG_START
  char* block = _oyGetProfileBlock (profilename, size);
  DBG_PROG_S( ("%s %d %d", profilename, (int)block, *size) )
  DBG_PROG

  DBG_PROG_ENDE
  return block;
}

char*
oyGetDeviceProfile                (DEVICETYP typ,
                                   char* manufacturer,
                                   char* model,
                                   char* product_id,
                                   char* host,
                                   char* port,
                                   char* attrib1,
                                   char* attrib2,
                                   char* attrib3)
{ DBG_PROG_START
  char* profile_name = _oyGetDeviceProfile (manufacturer, model, product_id,
                                    host, port, attrib1, attrib2, attrib3);
  DBG_PROG_S( (profile_name) )

  DBG_PROG_ENDE
  return profile_name;
}

int
oySetDeviceProfile                (DEVICETYP typ,
                                   char* manufacturer,
                                   char* model,
                                   char* product_id,
                                   char* host,
                                   char* port,
                                   char* attrib1,
                                   char* attrib2,
                                   char* attrib3,
                                   char* profileName,
                                   void* mem,
                                   size_t size)
{ DBG_PROG_START
  int rc =     _oySetDeviceProfile (manufacturer, model, product_id,
                                    host, port, attrib1, attrib2, attrib3,
                                    profileName, mem, size);
  DBG_PROG_ENDE
  return rc;
}




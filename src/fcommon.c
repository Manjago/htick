/*****************************************************************************
 * HTICK --- FTN Ticker / Request Processor
 *****************************************************************************
 * Copyright (C) 1999 by
 *
 * Gabriel Plutzar
 *
 * Fido:     2:31/1
 * Internet: gabriel@hit.priv.at
 *
 * Vienna, Austria, Europe
 *
 * This file is part of HTICK, which is based on HPT by Matthias Tichy,
 * 2:2432/605.14 2:2433/1245, mtt@tichy.de
 *
 * HTICK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * HTICK is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HTICK; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <smapi/compiler.h>

#ifdef HAS_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAS_DIRECT_H
#  include <direct.h>
#endif

#ifdef HAS_PROCESS_H
#  include <process.h>
#endif

#ifdef HAS_DIR_H
#  include <dir.h>
#endif

/*
#include <smapi/typedefs.h>
#include <smapi/stamp.h>
#include <smapi/progprot.h>
#include <smapi/ffind.h>
#include <smapi/patmat.h>
*/

#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <fidoconf/dirlayer.h>
#include <fidoconf/adcase.h>
#include <fidoconf/xstr.h>
#include <fidoconf/recode.h>

#include <fcommon.h>
#include <global.h>
#include <toss.h>
#include <add_desc.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

void exit_htick(char *logstr, int print) {

    w_log(LL_FUNC,"exit_htick()");
    w_log(LL_CRIT, logstr);
    if (!config->logEchoToScreen && print) fprintf(stderr, "%s\n", logstr);

    doneCharsets();
    w_log(LL_STOP, "Exit");
    closeLog();
    if (config->lockfile) {
       FreelockFile(config->lockfile ,lock_fd);
    }
    disposeConfig(config);
    exit(EX_SOFTWARE);
}

int fileNameAlreadyUsed(char *pktName, char *packName) {
   unsigned int i;

   for (i=0; i < config->linkCount; i++) {
      if ((config->links[i].pktFile != NULL) && (pktName != NULL))
         if ((stricmp(pktName, config->links[i].pktFile)==0)) return 1;
      if ((config->links[i].packFile != NULL) && (packName != NULL))
         if ((stricmp(packName, config->links[i].packFile)==0)) return 1;
   }

   return 0;
}

int createOutboundFileNameAka(s_link *link, e_flavour prio, e_pollType typ, hs_addr *aka)
{
   int nRet = NCreateOutboundFileNameAka(config,link,prio,typ,aka);
   if(nRet == -1) 
      exit_htick("cannot create *.bsy file!",0);
   return nRet;
}

int createOutboundFileName(s_link *link, e_flavour prio, e_pollType typ)
{
  return createOutboundFileNameAka(link, prio, typ, &(link->hisAka));
}

int removeFileMask(char *directory, char *mask)
{
    DIR           *dir;
    struct dirent *file;
    char          *removefile = NULL;
    char          *descr_file_name = NULL;
    unsigned int  numfiles = 0;

   if (directory == NULL) return(0);

   dir = opendir(directory);
   if (dir != NULL) {
      while ((file = readdir(dir)) != NULL) {
         if (stricmp(file->d_name,".")==0 || stricmp(file->d_name,"..")==0) continue;
         if (patimat(file->d_name, mask) == 1) {

            /* remove file */
            xstrscat(&removefile, directory, file->d_name, NULL);
            if(removefile) remove(removefile);
            w_log('6',"Removed file: %s",removefile);
            numfiles++;
            nfree(removefile);

            /* remove description for file */
            xstrscat(&descr_file_name,directory, "files.bbs",NULL);
            adaptcase(descr_file_name);
            removeDesc(descr_file_name,file->d_name);
            nfree(descr_file_name);
         }
      }
      closedir(dir);
   }
   return(numfiles);
}

#ifdef _WIN32_WINNT
#define _WINUSER_
#define _WINUSER_H
#	ifdef __MINGW32__
#		define CreateHardLink CreateHardLinkA
#	endif
#endif

int link_file(const char *from, const char *to)
{
   int rc = FALSE;

#if _WIN32_WINNT == 0x0400
   
   WCHAR  FileLink[ MAX_PATH + 1 ];
   WCHAR  wto[ MAX_PATH + 1 ];
   LPWSTR FilePart;

   HANDLE hFileSource;

   WIN32_STREAM_ID StreamId;
   DWORD dwBytesWritten;
   LPVOID lpContext;
   DWORD cbPathLen;
   DWORD StreamHeaderSize;

   BOOL bSuccess;
#endif

   /* Test parameters: prevent read from NULL[]  */
   if (!from || !to) {
     if(!from) w_log(LL_ERR, __FILE__ "::link_file(): source file name is NULL");
     if(!to)   w_log(LL_ERR, __FILE__ "::link_file(): destination file name is NULL");
     return FALSE;
   }

#if   _WIN32_WINNT >= 0x0500

   rc = CreateHardLink(to, from, NULL);

#elif _WIN32_WINNT == 0x0400
   
   /*   */
   /*  open existing file that we link to */
   /*   */
   hFileSource = CreateFile(
                           from,
                           FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE
                           | FILE_SHARE_DELETE,
                           NULL, /*  sa */
                           OPEN_EXISTING,
                           0,
                           NULL
                           );

   if (hFileSource == INVALID_HANDLE_VALUE)
   {
      return rc;
   }

   /*   */
   /*  validate and sanitize supplied link path and use the result */
   /*  the full path MUST be Unicode for BackupWrite */
   /*   */
   MultiByteToWideChar( CP_ACP, 0, to,
       strlen(to)+1, wto,   
       sizeof(wto)/sizeof(wto[0]) );
   
   cbPathLen = GetFullPathNameW( wto, MAX_PATH, FileLink, &FilePart);

   if (cbPathLen == 0)
   {
      return rc;
   }

   cbPathLen = (cbPathLen + 1) * sizeof(WCHAR); /*  adjust for byte count */

   /*   */
   /*  it might also be a good idea to verify the existence of the link, */
   /*  (and possibly bail), as the file specified in FileLink will be */
   /*  overwritten if it already exists */
   /*   */

   /*   */
   /*  prepare and write the WIN32_STREAM_ID out */
   /*   */
   lpContext = NULL;

   StreamId.dwStreamId = BACKUP_LINK;
   StreamId.dwStreamAttributes = 0;
   StreamId.dwStreamNameSize = 0;
   StreamId.Size.HighPart = 0;
   StreamId.Size.LowPart = cbPathLen;

   /*   */
   /*  compute length of variable size WIN32_STREAM_ID */
   /*   */
   StreamHeaderSize = (LPBYTE)&StreamId.cStreamName - (LPBYTE)&
                      StreamId+ StreamId.dwStreamNameSize ;

   bSuccess = BackupWrite(
                         hFileSource,
                         (LPBYTE)&StreamId,  /*  buffer to write */
                         StreamHeaderSize,   /*  number of bytes to write */
                         &dwBytesWritten,
                         FALSE,              /*  don't abort yet */
                         FALSE,              /*  don't process security */
                         &lpContext
                         );

   if (bSuccess)
   {

      /*   */
      /*  write out the buffer containing the path */
      /*   */
      bSuccess = BackupWrite(
                            hFileSource,
                            (LPBYTE)FileLink,   /*  buffer to write */
                            cbPathLen,          /*  number of bytes to write */
                            &dwBytesWritten,
                            FALSE,              /*  don't abort yet */
                            FALSE,              /*  don't process security */
                            &lpContext
                            );

      /*   */
      /*  free context */
      /*   */
      BackupWrite(
                 hFileSource,
                 NULL,               /*  buffer to write */
                 0,                  /*  number of bytes to write */
                 &dwBytesWritten,
                 TRUE,               /*  abort */
                 FALSE,              /*  don't process security */
                 &lpContext
                 );
   }

   CloseHandle( hFileSource );

   if (!bSuccess)
   {
      return rc;
   }
   return TRUE;

#elif defined (_UNISTD_H) && !defined(OS2)

   rc = (link(from, to) == 0);

#endif
   return rc;
}


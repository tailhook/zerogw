/* Accept a connection on a socket, with specific opening flags.
   Copyright (C) 2009-2015 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include "binary-io.h"
//#include "msvc-nothrow.h"

#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
#endif

int
accept4 (int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  int fd;

#if HAVE_ACCEPT4
# undef accept4
  /* Try the system call first, if it exists.  (We may be running with a glibc
     that has the function but with an older kernel that lacks it.)  */
  {
    /* Cache the information whether the system call really exists.  */
    static int have_accept4_really; /* 0 = unknown, 1 = yes, -1 = no */
    if (have_accept4_really >= 0)
      {
        int result = accept4 (sockfd, addr, addrlen, flags);
        if (!(result < 0 && errno == ENOSYS))
          {
            have_accept4_really = 1;
            return result;
          }
        have_accept4_really = -1;
      }
  }
#endif
#define O_TEXT 0x4000
  /* Check the supported flags.  */
  if ((flags & ~(SOCK_CLOEXEC | O_TEXT | O_BINARY)) != 0)
    {
      errno = EINVAL;
      return -1;
    }

  fd = accept (sockfd, addr, addrlen);
  if (fd < 0)
    return -1;

#if SOCK_CLOEXEC
# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
/* Native Windows API.  */
  if (flags & SOCK_CLOEXEC)
    {
      HANDLE curr_process = GetCurrentProcess ();
      HANDLE old_handle = (HANDLE) _get_osfhandle (fd);
      HANDLE new_handle;
      int nfd;

      if (!DuplicateHandle (curr_process,           /* SourceProcessHandle */
                            old_handle,             /* SourceHandle */
                            curr_process,           /* TargetProcessHandle */
                            (PHANDLE) &new_handle,  /* TargetHandle */
                            (DWORD) 0,              /* DesiredAccess */
                            FALSE,                  /* InheritHandle */
                            DUPLICATE_SAME_ACCESS)) /* Options */
        {
          close (fd);
          errno = EBADF; /* arbitrary */
          return -1;
        }

      /* Closing fd before allocating the new fd ensures that the new fd will
         have the minimum possible value.  */
      close (fd);
      nfd = _open_osfhandle ((intptr_t) new_handle,
                             O_NOINHERIT | (flags & (O_TEXT | O_BINARY)));
      if (nfd < 0)
        {
          CloseHandle (new_handle);
          return -1;
        }
      return nfd;
    }
# else
/* Unix API.  */
  if (flags & SOCK_CLOEXEC)
    {
      int fcntl_flags;

      if ((fcntl_flags = fcntl (fd, F_GETFD, 0)) < 0
          || fcntl (fd, F_SETFD, fcntl_flags | FD_CLOEXEC) == -1)
        {
          int saved_errno = errno;
          close (fd);
          errno = saved_errno;
          return -1;
        }
    }
# endif
#endif

#if O_BINARY
  if (flags & O_BINARY)
    set_binary_mode (fd, O_BINARY);
  else if (flags & O_TEXT)
    set_binary_mode (fd, O_TEXT);
#endif

  return fd;
}

#ifndef SRA_TYPES_H
#define SRA_TYPES_H

#ifdef _WIN32
#  include <windows.h>
#  include <winuser.h>
#  include <mmsystem.h>
#else
#  include <stdint.h>
#  include <unistd.h>
#  include <pthread.h>
#  include <time.h>
#  include <sched.h>
#  include <alsa/asoundlib.h>
   typedef unsigned char  BYTE;
   typedef unsigned short WORD;
   typedef unsigned int   DWORD;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int BYTE4;

#endif /* SRA_TYPES_H */

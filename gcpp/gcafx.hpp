#ifndef __unused
#   if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#       define __unused         __attribute__((__unused__))
#   else
#       define __unused
#   endif
#endif

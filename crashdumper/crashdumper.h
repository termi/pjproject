#pragma once

//#include <stddef.h>

# define CD_DECL_EXPORT     __declspec(dllexport)
# define CD_DECL_IMPORT     __declspec(dllimport)

#if defined(BUILD_CD_LIB)
#  define CD_API CD_DECL_EXPORT
#else
#  define CD_API CD_DECL_IMPORT
#endif


#ifdef __cplusplus
extern "C" {  // only need to export C interface if
              // used by C++ source code
#endif

void initCrashHandling(const char* dump_dir);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdlib.h>

#if defined(__MINGW32__) && defined(_UCRT)
// The pinned MinGW-w64 headers omit this documented, linked UCRT export.
extern "C" _CRTIMP _invalid_parameter_handler __cdecl
_set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#endif

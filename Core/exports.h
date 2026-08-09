//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#pragma once

// Macros
//

#ifdef CORE_EXPORTS
#define WPG_CORE_API __declspec(dllexport)
#else
#define WPG_CORE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
#define WPG_CORE_EXTERN_C extern "C"
#else
#define WPG_CORE_EXTERN_C extern
#endif

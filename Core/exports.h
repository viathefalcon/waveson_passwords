//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#pragma once

// Macros
//

#ifdef CORE_EXPORTS
#define WPG_CORE_API extern "C" __declspec(dllexport)
#else
#define WPG_CORE_API extern "C" __declspec(dllimport)
#endif

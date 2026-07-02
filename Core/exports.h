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

/*
// Returns the exported API version for compatibility checks.
WPG_CORE_API int __stdcall WpgCoreGetApiVersion();

// Applies an XOR operation to each byte from input into output.
// Returns 0 on success, negative values on invalid arguments.
WPG_CORE_API int __stdcall WpgCoreXorBytes(
    const std::uint8_t* input,
    std::uint8_t* output,
    std::uint32_t length,
    std::uint8_t key);

// Lightweight health check for consumers loading the DLL.
WPG_CORE_API int __stdcall WpgCoreIsReady();
*/
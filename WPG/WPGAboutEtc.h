// WPGAboutEtc.h: declares the interface for functions for the 'About' dialog.
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

#if !defined(__WPG_ABOUT_ETC_H__)
#define __WPG_ABOUT_ETC_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Functions
//

// Fetches the identified string resource into the process heap
LPTSTR LoadStringProcessHeap(HINSTANCE, UINT);

// Gives the callback procedure for the 'About' dialog box
INT_PTR CALLBACK AboutDialogProc(HWND, UINT, WPARAM, LPARAM);

#endif // !defined(__WPG_ABOUT_ETC_H__)
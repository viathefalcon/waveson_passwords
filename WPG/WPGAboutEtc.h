// WPGAboutEtc.h: declares the interface for functions for the 'About' dialog.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#if !defined(__WPG_ABOUT_ETC_H__)
#define __WPG_ABOUT_ETC_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Local Project Headers
#include "WPGGenerator.h"

// Types
//

typedef struct _UIState {

	WPGCaps wpgCaps;
	XORVex wpgVex;

	BOOL fClipboardOpen;

	DWORD dwTooltips;
	HWND* phTooltips;

	WPG_H wpgHandle;

} UIState, *UIStatePtr;

// Functions
//

// Fetches the identified string resource into the process heap
LPTSTR LoadStringProcessHeap(HINSTANCE, UINT);

// Gives the callback procedure for the 'About' dialog box
INT_PTR CALLBACK AboutDialogProc(HWND, UINT, WPARAM, LPARAM);

#endif // !defined(__WPG_ABOUT_ETC_H__)
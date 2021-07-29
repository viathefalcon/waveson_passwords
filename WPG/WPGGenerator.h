// WPGGenerator.h: declares the interface to the password-generating thread
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

#if !defined(__WPG_H__)
#define __WPG_H__

// Includes
//

// Local Project Headers
#include "WPGGenerators.h"

// Macros
//

DECLARE_HANDLE(WPG_H);

#define AWM_WPG_STARTED		(WM_APP+1)
#define AWM_WPG_GENERATED	(AWM_WPG_STARTED+1)
#define AWM_WPG_STOPPED		(AWM_WPG_GENERATED+1)

// Types
//

typedef struct _WPGGenerated {

	WPGCaps wpgCapsFailed;

	BYTE cchPwd;
	LPCTSTR pszPwd;

} WPGGenerated, *PWPGGenerated;

// Functions
//

// Starts the thread
WPG_H StartWPGGenerator(HWND);

// Generates a password in the given output buffer; returns an enumeration of the generators which failed
VOID WPGPwdGenAsync(__in WPG_H, __in BYTE, __in WPGCaps, __in_z LPCTSTR, __in BYTE, __in BOOL);

// Stops the thread
VOID StopWPGGenerator(WPG_H);

#endif // __WPG_H__

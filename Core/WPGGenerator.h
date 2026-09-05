// WPGGenerator.h: declares the interface to the password-generating thread
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
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

#define AWM_WPG_STARTED			(WM_APP+1)
#define AWM_WPG_GENERATED		(AWM_WPG_STARTED+1)
#define AWM_WPG_FAILED			(AWM_WPG_GENERATED+1)
#define AWM_WPG_GET_HOTKEY_STR	(AWM_WPG_FAILED+1)
#define AWM_WPG_STOPPED			(AWM_WPG_GET_HOTKEY_STR+1)

// Types
//

typedef struct _WPG_BUFFER {

	size_t cch;
	TCHAR szBuf[1];

	inline size_t Cb(void) const {
		return sizeof( WPG_BUFFER ) + (sizeof( TCHAR ) * cch);
	}

} WPG_BUFFER, * PWPG_BUFFER;

// Functions
//

// Starts the thread
WPG_CORE_EXTERN_C WPG_CORE_API WPG_H StartWPGGenerator(HWND, HWND, BYTE);

// Generates a password in the given output buffer; returns an enumeration of the generators which failed
WPG_CORE_EXTERN_C WPG_CORE_API VOID WPGPwdGenAsync(__in WPG_H, __in BYTE, __in WPGCaps);

// Sets the alphabet to be used for subsequently-generated passwords
WPG_CORE_EXTERN_C WPG_CORE_API VOID SetPwdAlphabetAsync(__in WPG_H, __in LPCTSTR, __in BYTE);

// Enables/disables the use of duplicate characters in subsequently-genreated passwords
WPG_CORE_EXTERN_C WPG_CORE_API VOID EnablePwdDuplicatesAsync(__in WPG_H, __in BOOL);

// Stops the thread
WPG_CORE_EXTERN_C WPG_CORE_API VOID StopWPGGenerator(WPG_H);

// Cleans up after the thread
WPG_CORE_EXTERN_C WPG_CORE_API VOID CleanupWPGGenerator(WPG_H);

#endif // __WPG_H__

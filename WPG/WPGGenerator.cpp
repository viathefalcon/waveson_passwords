// WPGGenerator.cpp: gives the implementation of the password-generating thread.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// Declarations
#include "WPGGenerator.h"

// Macros
//

#define AWM_WPG_ALPHABET		(AWM_WPG_STOPPED+1)
#define AWM_WPG_DUPLICATES		(AWM_WPG_ALPHABET+1)
#define AWM_WPG_GENERATE		(AWM_WPG_DUPLICATES+1)
#define AWM_WPG_STOP			(AWM_WPG_GENERATE+1)

// Types
//

typedef struct _WPG_INSTANCE {

	HANDLE hEvent;
	HANDLE hThread;
	DWORD dwThreadId;

} WPG_INSTANCE, *PWPG_INSTANCE;

typedef struct _WPG_THREAD_PROPS {

	HWND hWnd;
	HANDLE hEvent;

	std::shared_ptr<wpg_t> wpg;

	BYTE cchAlphabet;
	LPCTSTR pszAlphabet;

	BOOL fDuplicatesAllowed;

} WPG_THREAD_PROPS, *PWPG_THREAD_PROPS;

typedef struct _WPG_ARGS {

	BYTE cchLength;
	WPGCaps wpgCaps;

} WPG_ARGS, *PWPG_ARGS;

// Constants
//

// Message window "class" name
static LPCTSTR c_pszWindowClass = TEXT( "WPGGeneratorWindowClass" );

// Message window "title"
static LPCTSTR c_pszWindowTitle = TEXT( "WPGGenerator" );

// Prototypes
//

// Gives the starting address for the worker thread
DWORD WINAPI WPGGeneratorThreadProc(__in LPVOID);

// Releases the resources associated with the given instance
VOID CleanUpWPGGenerator(__in PWPG_INSTANCE);

// Processes messages for the message window
LRESULT CALLBACK WPGGeneratorWindowProcedure(HWND, UINT, WPARAM, LPARAM);

// Called when a new password is to be generated
HRESULT OnGeneratePassword(HWND, WPARAM, LPARAM);

// Called when the alphabet to use for password generation changes
HRESULT OnSetPwdAlphabet(HWND, WPARAM, LPARAM);

// Called when the 'allow duplicates' toggle is flipped
HRESULT OnEnablePwdDuplicates(HWND, WPARAM, LPARAM);

// Functions
//

WPG_H StartWPGGenerator(HWND hWnd) {

	// Allocate the structure
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		PH_ALLOC( sizeof( WPG_INSTANCE ) )
	);
	if (pInstance == NULL){
		return NULL;
	}

	// Create the event
	pInstance->hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	// Spin the thread
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		PH_ALLOC( sizeof( WPG_THREAD_PROPS ) )
	);
	pThreadProps->hWnd = hWnd;
	pThreadProps->hEvent = pInstance->hEvent;
	pInstance->hThread = CreateThread(
		NULL,
		0,
		WPGGeneratorThreadProc,
		static_cast<LPVOID>( pThreadProps ),
		0,
		&(pInstance->dwThreadId)
	);

	return reinterpret_cast<WPG_H>( pInstance );
}

VOID WPGPwdGenAsync(__in WPG_H wpgHandle, __in BYTE cchLength, __in WPGCaps wpgCaps) {

	// Wrap up the arguments
	PWPG_ARGS pArgs = static_cast<PWPG_ARGS>(
		PH_ALLOC( sizeof( WPG_ARGS ) )
	);
	pArgs->cchLength = cchLength;
	pArgs->wpgCaps = wpgCaps;

	// Post them to the thread
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);
	WPARAM wParam = reinterpret_cast<WPARAM>( pArgs );
	PostThreadMessage( pInstance->dwThreadId, AWM_WPG_GENERATE, wParam, 0U );
}

VOID SetPwdAlphabetAsync(__in WPG_H wpgHandle, __in LPCTSTR pszAlphabet, __in BYTE cchAlphabet) {

	// Take a (null-terminated) copy of the string
	LPTSTR pszCopy = (cchAlphabet > 0)
		? static_cast<LPTSTR>( PH_ALLOC( sizeof( TCHAR ) * (cchAlphabet + 1) ) )
		: NULL;
	if (pszCopy){
		CopyMemory( pszCopy, pszAlphabet, sizeof( TCHAR ) * cchAlphabet );
	}

	// Post it to the thread
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);
	PostThreadMessage(
		pInstance->dwThreadId,
		AWM_WPG_ALPHABET,
		reinterpret_cast<WPARAM>( pszCopy ),
		static_cast<LPARAM>( cchAlphabet )
	);
}

VOID EnablePwdDuplicatesAsync(__in WPG_H wpgHandle, __in BOOL fEnabled) {

	// Post it to the thread
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
		);
	PostThreadMessage(
		pInstance->dwThreadId,
		AWM_WPG_DUPLICATES,
		static_cast<WPARAM>( fEnabled ),
		0U
	);
}

VOID StopWPGGenerator(WPG_H wpgHandle) {

	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);

	// Signal the thread to stop, and wait for it
	PostThreadMessage( pInstance->dwThreadId, AWM_WPG_STOP, 0, 0 );
	WaitForSingleObject( pInstance->hEvent, INFINITE );

	// Cleanup
	CloseHandle( pInstance->hEvent );
	CloseHandle( pInstance->hThread );
	PH_FREE( pInstance );
}

DWORD WINAPI WPGGeneratorThreadProc(__in LPVOID lpParameter) {

	// Capture the parameters
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		lpParameter
	);
	HINSTANCE hInstance = static_cast<HINSTANCE>( GetModuleHandle( NULL ) );

	// Create the message window
	HWND hWnd = NULL;
	WNDCLASSEX wcx = { 0 };
	wcx.cbSize = sizeof( WNDCLASSEX );
	wcx.lpfnWndProc = WPGGeneratorWindowProcedure;
	wcx.hInstance = hInstance;
	wcx.lpszClassName = c_pszWindowClass;
	if (RegisterClassEx( &wcx )){
		hWnd = CreateWindowEx(
			0,
			c_pszWindowClass,
			c_pszWindowTitle,
			0,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			HWND_MESSAGE,
			NULL,
			hInstance,
			lpParameter 
		);
	}
	if (hWnd){
		// Kick into the message loop
		MSG msg = { 0L };
		do {
			// Get the next message
			if (GetMessage( &msg, NULL, 0U, 0U )){
				// Route anything not bound for any window to the window..?
				if (!msg.hwnd){
					msg.hwnd = hWnd;
				}

				// Translate the message and dispatch it
				TranslateMessage( &msg ); 
				DispatchMessage( &msg );
			}
		} while (msg.message != AWM_WPG_STOP);
	}else{
		// Signal failure
		;
	}

	// Notify the host that we're done
	PostMessage(
		pThreadProps->hWnd,
		AWM_WPG_STOPPED,
		0,
		0
	);
	HANDLE hEvent = pThreadProps->hEvent;

	// Cleanup
	if (pThreadProps->pszAlphabet){
		PH_FREE( const_cast<LPTSTR>( pThreadProps->pszAlphabet ) );
		pThreadProps->pszAlphabet = NULL;
	}
	PH_FREE( lpParameter );

	// Signal that we're done
	SetEvent( hEvent );
#if defined (_DEBUG)
	OutputDebugString( TEXT( "Generator thread finishing..\x0A"  ) );
#endif
	return 0;
}

LRESULT CALLBACK WPGGeneratorWindowProcedure(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) {

	LRESULT lResult = 0;
	switch (uMessage){
		case WM_CREATE:
		{
#if defined (_DEBUG)
			OutputDebugString( TEXT( "WPGGenerator: WM_CREATE\x0D" ) );
#endif
			// Signal the spawning thread that we've started
			LPCREATESTRUCT lpCreate = reinterpret_cast<LPCREATESTRUCT>( lParam );
			PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
				lpCreate->lpCreateParams
			);
			if (pThreadProps){
				// Create and capture the password generator
				pThreadProps->wpg = GetWPG( );

				PostMessage(
					pThreadProps->hWnd,
					AWM_WPG_STARTED,
					0, 0
				);
				SetWindowLongPtr(
					hWnd,
					GWLP_USERDATA,
					reinterpret_cast<LONG_PTR>( lpCreate->lpCreateParams )
				);
			}
		}
			break;

		case AWM_WPG_ALPHABET:
			OnSetPwdAlphabet( hWnd, wParam, lParam );
			break;

		case AWM_WPG_DUPLICATES:
			OnEnablePwdDuplicates( hWnd, wParam, lParam );
			break;

		case AWM_WPG_GENERATE:
			OnGeneratePassword( hWnd, wParam, lParam );
			break;

		case AWM_WPG_STOP:
		{
#if defined (_DEBUG)
			OutputDebugString( TEXT( "AWM_WPG_STOP\x0D" ) );
#endif
			// Cleanup the password generator
			PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
				GetWindowLongPtr( hWnd, GWLP_USERDATA )
			);
			if (pThreadProps){
				pThreadProps->wpg.reset( );
			}
		}
			break;

		// All other messages
		default:
			// Pass onto the default handler
			lResult = DefWindowProc( hWnd, uMessage, wParam, lParam );
			break;
	}
	return lResult;
}

HRESULT OnGeneratePassword(HWND hWnd, WPARAM wParam, LPARAM lParam) {

	// Unpack the arguments
	PWPG_ARGS pArgs = reinterpret_cast<PWPG_ARGS>( wParam );
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		GetWindowLongPtr( hWnd, GWLP_USERDATA )
	);
	if (pThreadProps && pThreadProps->wpg){
		const BOOL fEmpty = (pThreadProps->pszAlphabet == NULL) || (pThreadProps->cchAlphabet < 1);

		// Allocate the output structure
		PWPGGenerated pWPGGenerated = static_cast<PWPGGenerated>( PH_ALLOC( sizeof( WPGGenerated ) ) );
		pWPGGenerated->cchPwd = fEmpty ? 0 : pArgs->cchLength;
		pWPGGenerated->pszPwd = static_cast<LPCTSTR>( PH_ALLOC( sizeof( TCHAR ) * (pWPGGenerated->cchPwd + 1) ) );

		// Do the password generation
		auto wpg = pThreadProps->wpg;
		pWPGGenerated->wpgCapsFailed = wpg->Generate(
			const_cast<LPTSTR>( pWPGGenerated->pszPwd ),
			pWPGGenerated->cchPwd,
			pArgs->wpgCaps,
			&(pWPGGenerated->cchPwd),
			pThreadProps->pszAlphabet,
			pThreadProps->fDuplicatesAllowed
		);

#if defined (_DEBUG)
		if (pWPGGenerated->wpgCapsFailed == WPGCapNONE){
			OutputDebugString( TEXT( "Generator thread generated password: " ) );
			OutputDebugString( pWPGGenerated->pszPwd );
			OutputDebugString( TEXT( "\x0A" ) );
		}else{
			OutputDebugString( TEXT( "Failed to generate password in generator thread\x0A" ) );
		}
#endif

		// Send the output to the main thread
		PostMessage(
			pThreadProps->hWnd,
			AWM_WPG_GENERATED,
			reinterpret_cast<WPARAM>( pWPGGenerated ),
			lParam
		);
	}
	PH_FREE( pArgs );
	return S_OK;
}

HRESULT OnSetPwdAlphabet(HWND hWnd, WPARAM wParam, LPARAM lParam) {

	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		GetWindowLongPtr( hWnd, GWLP_USERDATA )
	);
	if (pThreadProps){
		// Release the existing alphabet, if any
		if (pThreadProps->pszAlphabet){
			PH_FREE( const_cast<LPTSTR>( pThreadProps->pszAlphabet ) );
		}

		// Just accept the pointer
		pThreadProps->pszAlphabet = reinterpret_cast<LPTSTR>( wParam );
		pThreadProps->cchAlphabet = static_cast<BYTE>( lParam );
#if defined (_DEBUG)
		OutputDebugString( TEXT( "Password alphabet set to: " ) );
		OutputDebugString( pThreadProps->pszAlphabet );
		OutputDebugString( TEXT( "\x0A" ) );
#endif
		return S_OK;
	}
	return S_FALSE;
}

HRESULT OnEnablePwdDuplicates(HWND hWnd, WPARAM wParam, LPARAM lParam) {

	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		GetWindowLongPtr( hWnd, GWLP_USERDATA )
	);
	if (pThreadProps){
		pThreadProps->fDuplicatesAllowed = static_cast<BOOL>( wParam );
#if defined (_DEBUG)
		OutputDebugString( TEXT( "Duplicates enabled: " ) );
		if (pThreadProps->fDuplicatesAllowed){
			OutputDebugString( TEXT( "TRUE" ) );
		}else{
			OutputDebugString( TEXT( "FALSE" ) );
		}
		OutputDebugString( TEXT( "\x0A" ) );
#endif
		return S_OK;
	}
	return S_FALSE;
}

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

// Constants
//

constexpr LONG WPG_NON_STOP = 0;

// Types
//

typedef struct _WPG_INSTANCE {

	HANDLE hThread;
	DWORD dwThreadId;
	LONG* plStop;

} WPG_INSTANCE, *PWPG_INSTANCE;

typedef struct _WPG_THREAD_PROPS {

	HWND hWnd;
	LONG* plStop;

	BYTE cchMax;
	LPTSTR pszBuffer;

	std::shared_ptr<wpg_t> wpg;

	BYTE cchAlphabet;
	LPCTSTR pszAlphabet;

	BOOL fDuplicatesAllowed;

} WPG_THREAD_PROPS, *PWPG_THREAD_PROPS;

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

WPG_H StartWPGGenerator(HWND hWnd, BYTE cchMax) {

	// Allocate the structure
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		PH_ALLOC( sizeof( WPG_INSTANCE ) )
	);
	if (pInstance == NULL){
		return NULL;
	}
	pInstance->plStop = reinterpret_cast<LONG*>( _aligned_malloc( sizeof( LONG ), sizeof( LONG ) ) );
	*(pInstance->plStop) = WPG_NON_STOP;

	// Allocate the control structure
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		PH_ALLOC( sizeof( WPG_THREAD_PROPS ) )
	);
	pThreadProps->plStop = pInstance->plStop;
	pThreadProps->hWnd = hWnd;
	pThreadProps->cchMax = cchMax;

	// Spin the thread
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

	// Post to the thread
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);
	WPARAM wParam = static_cast<WPARAM>( cchLength );
	LPARAM lParam = static_cast<LPARAM>( wpgCaps );
	PostThreadMessage( pInstance->dwThreadId, AWM_WPG_GENERATE, wParam, lParam );
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

	// Raise the stop sign
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);
	InterlockedIncrement( pInstance->plStop );
	PostThreadMessage( pInstance->dwThreadId, AWM_WPG_STOP, 0, 0 );
}

VOID CleanupWPGGenerator(WPG_H wpgHandle) {

	if (wpgHandle == NULL){
		return;
	}
	PWPG_INSTANCE pInstance = reinterpret_cast<PWPG_INSTANCE>(
		wpgHandle
	);
	if (pInstance->plStop){
		_aligned_free( pInstance->plStop );
		pInstance->plStop = NULL;
	}
	CloseHandle( pInstance->hThread );
	PH_FREE( pInstance );
}

// Indicates whether the given generator thread should stop
static inline BOOL WPGGeneratorShouldStop(__in PWPG_THREAD_PROPS pThreadProps) {
	const LONG value = InterlockedCompareExchange( pThreadProps->plStop, WPG_NON_STOP, WPG_NON_STOP );
	return (value > WPG_NON_STOP);
}

DWORD WINAPI WPGGeneratorThreadProc(__in LPVOID lpParameter) {

	// Capture the parameters
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		lpParameter
	);
	pThreadProps->pszBuffer = static_cast<LPTSTR>(
		PH_ALLOC( sizeof( TCHAR ) * (static_cast<SIZE_T>( pThreadProps->cchMax ) + 1U) )
	);
	pThreadProps->wpg = wpg_t::New( );

	// Create the message window
	HINSTANCE hInstance = static_cast<HINSTANCE>( GetModuleHandle( NULL ) );
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
				// Look for an early out
				if (WPGGeneratorShouldStop( pThreadProps )){
#if defined (_DEBUG)
					OutputDebugString( TEXT( "WPGGeneratorShouldStop\x0A"  ) );
#endif
					break;
				}

				// Route anything not bound for any window to the window..?
				if (!msg.hwnd){
					msg.hwnd = hWnd;
				}

				// Translate the message and dispatch it
				TranslateMessage( &msg ); 
				DispatchMessage( &msg );
			}
		} while (msg.message != AWM_WPG_STOP);
		DestroyWindow( hWnd );		
	}else{
		// Signal failure
		;
	}

	// Cleanup
	HWND hWndHost = pThreadProps->hWnd;
	if (pThreadProps->pszBuffer){
		PH_FREE( pThreadProps->pszBuffer );
		pThreadProps->pszBuffer = NULL;
	}
	if (pThreadProps->pszAlphabet){
		PH_FREE( const_cast<LPTSTR>( pThreadProps->pszAlphabet ) );
		pThreadProps->pszAlphabet = NULL;
	}
	pThreadProps->plStop = NULL;
	pThreadProps->wpg.reset( );
	PH_FREE( lpParameter );

	// Signal that we're done
	PostMessage( hWndHost, AWM_WPG_STOPPED, 0, 0 );
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
			LPCREATESTRUCT lpCreate = reinterpret_cast<LPCREATESTRUCT>( lParam );
			PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
				lpCreate->lpCreateParams
			);
			if (pThreadProps){
				// Attach the thread properties to the window
				SetWindowLongPtr(
					hWnd,
					GWLP_USERDATA,
					reinterpret_cast<LONG_PTR>( lpCreate->lpCreateParams )
				);

				// Signal the spawning thread that we've started
				const auto caps = pThreadProps->wpg->Caps( );
				const auto vex = pThreadProps->wpg->Vex( );
				PostMessage(
					pThreadProps->hWnd,
					AWM_WPG_STARTED,
					static_cast<WPARAM>( caps ),
					static_cast<LPARAM>( vex )
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
#if defined (_DEBUG)
			OutputDebugString( TEXT( "AWM_WPG_STOP\x0D" ) );
#endif
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
	PWPG_THREAD_PROPS pThreadProps = reinterpret_cast<PWPG_THREAD_PROPS>(
		GetWindowLongPtr( hWnd, GWLP_USERDATA )
	);
	if (pThreadProps && pThreadProps->wpg){
		// Setup
		const BYTE cchLength = static_cast<BYTE>( wParam );
		const WPGCaps wpgCaps = static_cast<WPGCaps>( lParam );
		const BOOL fEmpty = (pThreadProps->pszAlphabet == NULL) || (pThreadProps->cchAlphabet < 1);
		BYTE cch = fEmpty ? 0 : min( cchLength, pThreadProps->cchMax );

		// Do the password generation
		WPGCaps wpgCapsFailed = pThreadProps->wpg->Generate(
			pThreadProps->pszBuffer,
			cch,
			wpgCaps,
			&(cch),
			pThreadProps->pszAlphabet,
			pThreadProps->fDuplicatesAllowed
		);

#if defined (_DEBUG)
		if (wpgCapsFailed == WPGCapNONE){
			OutputDebugString( TEXT( "Generator thread generated password: " ) );
			OutputDebugString( pThreadProps->pszBuffer );
			OutputDebugString( TEXT( "\x0A" ) );
		}else{
			OutputDebugString( TEXT( "Failed to generate password in generator thread\x0A" ) );
		}
#endif

		// Send to the target window (synchronously)
		SendMessage(
			pThreadProps->hWnd,
			AWM_WPG_GENERATED,
			reinterpret_cast<WPARAM>( pThreadProps->pszBuffer ),
			static_cast<LPARAM>( wpgCapsFailed )
		);
		SecureZeroMemory( pThreadProps->pszBuffer, pThreadProps->cchMax );
		return S_OK;
	}
	return E_POINTER;
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

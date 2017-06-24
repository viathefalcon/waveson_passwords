// WPGMain.cpp : Defines the entry point for the application.
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// Local Project Headers
#include "WPGGenerators.h"
#include "WPGRegistry.h"

// Macros
//

// Defines a Windows message which, when sent to the main window, causes it to generate a new password
#define UWM_REFRESH				(WM_APP+1L)

// Constants
//

// Identifies the timer
const UINT c_uTimer = 123L;

// Specifies the minimum, maximum and default lengths of generated passwords
const BYTE c_cbMinLength = 0x01;
const BYTE c_cbMaxLength = 0xFF;
const BYTE c_cbDefaultLength = 0x10;

// Specifies the intervals for the timer
const UINT c_uTimerElapseDefault = 1000U;
const UINT c_uTimerElapseMinimum = 1U;

const DWORD c_dwRDRANDCheck = 0x80000000;
const DWORD c_dwTPMCheck = 0x00800000;
const DWORD c_dwAutoCopyCheck = 0x00008000;
const DWORD c_dwDefaultChecks = (c_dwRDRANDCheck | c_dwTPMCheck);

// Globals
//

// Gives the icon handle
HICON g_hIcon = nullptr;

// Gives the identifier of the message recieved when a taskbar button has been created by the system
UINT g_uwmTaskbarButtonCreated = 0;

// Prototypes
//

// Gives the callback procedure for the main dialog box
INT_PTR CALLBACK MainDialogProc(HWND, UINT, WPARAM, LPARAM);

// Called when the dialog is initialised
HRESULT OnInitDialog(HWND);

// Called when the user clicks the 'Reset' button
HRESULT OnReset(HWND);

// Called when the user selects to copy the generated password to the clipboard
HRESULT OnCopy(HWND);

// Called when a new password is to be generated
HRESULT OnRefresh(HWND);

// Called when the main dialog is closed
HRESULT OnClose(HWND);

// Returns TRUE if the contents of the input control are the default; FALSE otherwise
LPTSTR NonDefaultAlphabet(HWND);

// Retrieves and sets the given error message
VOID SetErrorMsg(HWND, WPGCaps);

// Functions
//

// Gives the entry-point
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {

	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( nCmdShow );

	// Load the icon
	g_hIcon = reinterpret_cast<HICON>( LoadImage( hInstance, MAKEINTRESOURCE( IDI_WAVESON ), IMAGE_ICON, GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), 0 ) );

	// Initialise the Common Controls Library; ensure that the progress control is available
	INITCOMMONCONTROLSEX icex = { 0L };
	icex.dwICC = ICC_BAR_CLASSES;
	icex.dwSize = static_cast<DWORD>( sizeof( INITCOMMONCONTROLSEX ) );
	if (!InitCommonControlsEx( &icex )){
		MessageBox( HWND_DESKTOP, TEXT( "Failed to initialise common controls. Aborting." ), TEXT( "Error" ), MB_OK | MB_ICONERROR );
		return 1;
	}

	// Register the message
	g_uwmTaskbarButtonCreated = RegisterWindowMessage( L"TaskbarButtonCreated" ); 

	// Display the dialog box
	DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_MAIN ), HWND_DESKTOP, MainDialogProc, 0 );

	// Cleanup, return
	if (g_hIcon){
		DestroyIcon( g_hIcon );
	}
	return 0;
}

// Gives the callback procedure for the main dialog box
INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	static UINT uTimer = 0;
	static WORD wScroll = 0;

	BOOL bResult = TRUE;
	switch (uMsg){

		// The dialog was initialised
		case WM_INITDIALOG:
			bResult = SUCCEEDED( OnInitDialog( hDlg ) );
			if (bResult){
				// Initialise scroll-tracking
				HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
				wScroll = static_cast<WORD>( SendMessage( hSlider, TBM_GETPOS, 0, 0 ) );
			}
			break;

		case WM_HSCROLL:
		{
			WORD w = wScroll;
			switch (LOWORD( wParam )){
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					wScroll = HIWORD( wParam );
					break;

				default:
					wScroll = static_cast<WORD>( SendMessage( reinterpret_cast<HWND>( lParam ), TBM_GETPOS, 0, 0 ) );
					break;
			}

			// Check that the position has actually changed
			if (w != wScroll){
				PostMessage( hDlg, UWM_REFRESH, 0, 0 );
			}
		}
			break;

		case WM_COMMAND:
			switch (LOWORD( wParam )){
				case IDC_BUTTON_RESET:
					bResult = SUCCEEDED( OnReset( hDlg ) );
					if (bResult){
						PostMessage( hDlg, UWM_REFRESH, 0, 0 );
					}
					break;

				case IDC_BUTTON_REFRESH:
					PostMessage( hDlg, UWM_REFRESH, 0, 0 );
					break;

				case IDC_BUTTON_COPY:
					bResult = SUCCEEDED( OnCopy( hDlg ) );
					break;

				case IDC_CHECK_RDRAND:
				case IDC_CHECK_TPM:
					if (HIWORD( wParam ) == BN_CLICKED){
						PostMessage( hDlg, UWM_REFRESH, 0, 0 );
					}
					break;

				default:
					bResult = FALSE;
					break;
			}
			break;

		// The window's 'X' was clicked
		case WM_CLOSE:
		{
			OnClose( hDlg );

			// Pause (however briefly) before killing the dialog
			if (uTimer != 0L){
				KillTimer( hDlg, uTimer );
			}
			const UINT uElapse = c_uTimerElapseMinimum;
			uTimer = SetTimer( hDlg, c_uTimer, uElapse, NULL );
		}
			break;

		// A timer fired
		case WM_TIMER:
			switch (wParam){
				case c_uTimer:
					// Kill the timer
					KillTimer( hDlg, uTimer );
					uTimer = 0L;

					// Kill the dialog
					EndDialog( hDlg, TRUE );
					break;

				default:
					bResult = FALSE;
					break;
			}
			break;

		case UWM_REFRESH:
			OnRefresh( hDlg );
			break;

		default:
			if (bResult = (uMsg == g_uwmTaskbarButtonCreated)){
				// TODO?
			}
			break;
	}
	return bResult;
}

LPTSTR LoadStringProcessHeap(HINSTANCE hInstance, UINT uID) {

	LPTSTR pszTmp = NULL;
	const int nLength = LoadString( hInstance, uID, reinterpret_cast<LPTSTR>( &pszTmp ), 0 ); // On its own, this just returns a pointer to the entire contents of the string table..?
	if (!nLength){
		return NULL;
	}

	// Allocate a null-terminated buffer and copy into it
	LPTSTR pszText = static_cast<LPWSTR>( PH_ALLOC( sizeof( TCHAR ) * (nLength+1) ) );
	StringCchCopy( pszText, (nLength + 1), pszTmp );
	return pszText;
}

HRESULT OnCreateTooltips(HWND hDlg) {

	const int nDlgItems[] = { IDC_CHECK_RDRAND, IDC_CHECK_TPM, IDC_EDIT_INPUT, IDC_SLIDER_OUTPUT, IDC_CHECK_AUTO_COPY };
	const UINT uStringIDs[] = { IDS_CHECK_RDRAND, IDS_CHECK_TPM, IDS_EDIT_INPUT, IDS_SLIDER_OUTPUT, IDS_CHECK_AUTO_COPY };

	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
	unsigned int count = min( ARRAYSIZE( nDlgItems ), ARRAYSIZE( uStringIDs ) );
	for (decltype(count) i = 0; i < count; ++i){
		HWND hItem = GetDlgItem( hDlg, nDlgItems[i] );
		LPTSTR pszText = LoadStringProcessHeap( hInstance, uStringIDs[i] );
		if (!pszText){
			continue;
		}

		// Create the tooltip, and associate it with the dialog item
		HWND hTooltip = CreateWindowEx( NULL, TOOLTIPS_CLASS, NULL,
										WS_POPUP | TTS_ALWAYSTIP,
										CW_USEDEFAULT, CW_USEDEFAULT,
										CW_USEDEFAULT, CW_USEDEFAULT,
										hDlg, NULL, hInstance, NULL );
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = hDlg;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = reinterpret_cast<UINT_PTR>( hItem );
		toolInfo.lpszText = pszText;
		SendMessage( hTooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>( &toolInfo ) );
		PH_FREE( pszText );
	}
	return S_OK;
}

HRESULT OnInitDialog(HWND hDlg) {

	// Set the icon, c.f. https://support.microsoft.com/en-us/help/179582/how-to-set-the-title-bar-icon-in-a-dialog-box
	if (g_hIcon){
		SendMessage( hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( g_hIcon ) );
	}

	LPTSTR pszAlphabet = NULL;
	DWORD dwLength = c_cbDefaultLength, dwChecks = c_dwDefaultChecks;
	HKEY hKey = NULL;
	if (SUCCEEDED( WPGRegOpenKey( &hKey ) )){
		if (FAILED( WPGRegGetDWORD( &dwLength, hKey, WPGRegLength ) )){
			dwLength = c_cbDefaultLength;
		}
		if (FAILED( WPGRegGetDWORD( &dwChecks, hKey, WPGRegChecks ) ) || (dwChecks == 0)){
			dwChecks = c_dwDefaultChecks;
		}
		if (FAILED( WPGRegGetString( &pszAlphabet, hKey, WPGRegAlphabet ) )){
			pszAlphabet = NULL;
		}
		WPGRegCloseKey( hKey );
	}

	// Configure the slider
	HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
	SendMessage( hSlider, TBM_SETRANGE, TRUE, MAKELPARAM( c_cbMinLength, c_cbMaxLength ) );
	SendMessage( hSlider, TBM_SETTICFREQ, (c_cbDefaultLength - 1), 0 );
	SendMessage( hSlider, TBM_SETPOS, TRUE, dwLength );

	// Get the 'capabilities' of the password generator
	const WPGCaps caps = WPGGetCaps( );
	UINT uCheckRDRAND = BST_UNCHECKED;
	if (caps & WPGCapRDRAND){
		EnableWindow( GetDlgItem( hDlg, IDC_CHECK_RDRAND ), TRUE );
		uCheckRDRAND = (dwChecks & c_dwRDRANDCheck) ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton( hDlg, IDC_CHECK_RDRAND, uCheckRDRAND );
	}
	UINT uCheckTPM = BST_UNCHECKED;
	if ((caps & WPGCapTPM12) || (caps & WPGCapTPM20)){
		EnableWindow( GetDlgItem( hDlg, IDC_CHECK_TPM ), TRUE );
		uCheckTPM = (dwChecks & c_dwTPMCheck) ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton( hDlg, IDC_CHECK_TPM, uCheckTPM );
	}
	SetWindowLongPtr( hDlg, GWLP_USERDATA, static_cast<LONG_PTR>( caps ) );

	const UINT uCheck = (dwChecks & c_dwAutoCopyCheck) ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton( hDlg, IDC_CHECK_AUTO_COPY, uCheck );

	// Seed the input from registry, above, (or resources) and do the initial 'refresh'
	if (pszAlphabet){
		HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
		SetWindowText( hInput, pszAlphabet );
		PH_FREE( pszAlphabet );
	}else{
		OnReset( hDlg );
	}
	const BOOL bEnable = (uCheckRDRAND == BST_CHECKED) || (uCheckTPM == BST_CHECKED);
	if (bEnable){
		PostMessage( hDlg, UWM_REFRESH, 0, 0 );
	}
	EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_REFRESH ), bEnable );
	return OnCreateTooltips( hDlg );
}

HRESULT OnReset(HWND hDlg) {

	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
	LPTSTR p = LoadStringProcessHeap( hInstance, IDS_INPUT_DEFAULT );
	if (p){
		HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
		SetWindowText( hInput, p );

		HeapFree( GetProcessHeap( ), 0, p );
		return S_OK;
	}
	return HRESULT_FROM_WIN32( GetLastError( ) );
}

HRESULT OnCopy(HWND hDlg) {

	// Get a shareable copy of the contents of the output control
	HWND hOutput = GetDlgItem( hDlg, IDC_EDIT_OUTPUT );
	const int cchOutput = 1 + GetWindowTextLength( hOutput );
	const SIZE_T cbOutput = sizeof( TCHAR ) * cchOutput;
	HGLOBAL hGlobal = GlobalAlloc( GMEM_MOVEABLE, cbOutput );
	LPTSTR pszShared = (LPTSTR) GlobalLock( hGlobal );
	SecureZeroMemory( pszShared, cbOutput );
	GetWindowText( hOutput, pszShared, cchOutput );

	// Put the text on the clipboard
	HRESULT hResult = E_FAIL;
	GlobalUnlock( hGlobal );
	BOOL bOpened = OpenClipboard( hDlg );
	if (bOpened){
		EmptyClipboard( );
#if defined (UNICODE)
		const UINT uFormat = CF_UNICODETEXT;
#else
		const UINT uFormat = CF_TEXT;
#endif
		HANDLE hHandle = SetClipboardData( uFormat, hGlobal );
		if (hHandle){
			hResult = S_OK;

			// The clipboard now owns the memory?
			hGlobal = NULL;
		}else{
#if defined (_DEBUG)
			OutputDebugString( TEXT( "Copy failed? " ) );

			TCHAR szDebug[MAX_PATH] = { 0 };
			StringCchPrintf( szDebug, MAX_PATH, TEXT( "(%u)\x0A" ), GetLastError( ) );
			OutputDebugString( szDebug );
#endif
			hResult = HRESULT_FROM_WIN32( GetLastError( ) );
		}
		CloseClipboard( );
	}else{
		hResult = HRESULT_FROM_WIN32( GetLastError( ) );
		MessageBox( HWND_DESKTOP, TEXT( "Failed to open clipboard." ), TEXT( "Error" ), MB_OK | MB_ICONERROR );
	}
	if (hGlobal){
		GlobalFree( hGlobal );
	}
	return hResult;
}

HRESULT OnRefresh(HWND hDlg) {

	// Find the intersection between the available generators and the ones the user has selected
	WPGCaps caps = static_cast<WPGCaps>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
	if (!IsDlgButtonChecked( hDlg, IDC_CHECK_RDRAND )){
		caps &= ~WPGCapRDRAND;
	}
	if (!IsDlgButtonChecked( hDlg, IDC_CHECK_TPM )){
		caps &= ~(WPGCapTPM12 | WPGCapTPM20);
	}

	// Look for an early out
	HWND hOut = GetDlgItem( hDlg, IDC_EDIT_OUTPUT );
	if (caps == WPGCapNONE){
		SetWindowText( hOut, TEXT( "" ) );
		return S_OK;
	}

	// Allocate a buffer for the output
	HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
	BYTE cchPwd = static_cast<BYTE>( SendMessage( hSlider, TBM_GETPOS, 0, 0 ) );
	HANDLE hProcessHeap = GetProcessHeap( );
	LPTSTR pszPwd = static_cast<LPTSTR>( HeapAlloc( hProcessHeap, HEAP_ZERO_MEMORY, sizeof( TCHAR ) * (cchPwd+1) ) );

	// Get a copy of the input alphabet
	HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
	const int cchAlphabet = 1 + GetWindowTextLength( hInput );
	LPTSTR pszAlphabet = static_cast<LPTSTR>( HeapAlloc( hProcessHeap, HEAP_ZERO_MEMORY, sizeof( TCHAR ) * (cchAlphabet+1) ) );
	GetWindowText( hInput, pszAlphabet, cchAlphabet );

	// Use this to generate the password
	caps = WPGPwdGen( pszPwd, cchPwd, caps, &cchPwd, pszAlphabet );
	HRESULT hResult = (caps == WPGCapNONE) ? S_OK : E_FAIL;
	if (SUCCEEDED( hResult )){
		// Set the output
		SetWindowText( hOut, pszPwd );
		if (IsDlgButtonChecked( hDlg, IDC_CHECK_AUTO_COPY )){
			hResult = OnCopy( hDlg );
		}
	}else{
		SetErrorMsg( hDlg, caps );
	}

	// Cleanup and return
	HeapFree( hProcessHeap, 0, pszAlphabet );
	HeapFree( hProcessHeap, 0, pszPwd );
	return hResult;
}

HRESULT OnClose(HWND hDlg) {

	HKEY hKey = NULL;
	HRESULT hResult = WPGRegOpenKey( &hKey );
	if (SUCCEEDED( hResult )){
		DWORD dwChecks = 1;
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_RDRAND ) ? c_dwRDRANDCheck : 0);
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_TPM ) ? c_dwTPMCheck : 0);
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_AUTO_COPY ) ? c_dwAutoCopyCheck : 0);
		WPGRegSetDWORD( hKey, WPGRegChecks, dwChecks );

		LPTSTR pszAlphabet = NonDefaultAlphabet( hDlg );
		if (pszAlphabet){
			WPGRegSetString( hKey, WPGRegAlphabet, pszAlphabet );
			PH_FREE( pszAlphabet );
		}else{
			WPGRegDelete( hKey, WPGRegAlphabet );
		}

		HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
		const DWORD dwLength = SendMessage( hSlider, TBM_GETPOS, 0, 0 );
		WPGRegSetDWORD( hKey, WPGRegLength, dwLength );

		WPGRegCloseKey( hKey );
	}
	return S_OK;
}

LPTSTR NonDefaultAlphabet(HWND hDlg) {

	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
	LPTSTR pszInputDefault = LoadStringProcessHeap( hInstance, IDS_INPUT_DEFAULT );

	// Get a copy of the input alphabet
	HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
	const int cchAlphabet = 1 + GetWindowTextLength( hInput );
	LPTSTR pszAlphabet = static_cast<LPTSTR>( PH_ALLOC( sizeof( TCHAR ) * (cchAlphabet+1) ) );
	GetWindowText( hInput, pszAlphabet, cchAlphabet );

	// Do the comparison
	LPTSTR pszResult = NULL;
	if (CSTR_EQUAL == CompareStringEx( LOCALE_NAME_USER_DEFAULT, 0, pszInputDefault, -1, pszAlphabet, -1, NULL, NULL, 0 )){
		PH_FREE( pszAlphabet );
	}else{
		pszResult = pszAlphabet;
	}

	// Cleanup, return
	PH_FREE( pszInputDefault );
	return pszResult;
};

VOID SetErrorMsg(HWND hDlg, WPGCaps caps){

	UINT u = IDS_ERROR_UNKNOWN;
	switch (WPGCapsFirst( caps )){
		case WPGCapRDRAND:
			u = IDS_ERROR_RDRAND;
			break;

		case WPGCapTPM12:
			u = IDS_ERROR_TPM12;
			break;

		case WPGCapTPM20:
			u = IDS_ERROR_TPM20;
			break;
	}

	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
	LPTSTR pszString = LoadStringProcessHeap( hInstance, u );
	if (pszString){
		SetWindowText( GetDlgItem( hDlg, IDC_EDIT_OUTPUT ), pszString );
		PH_FREE( pszString );
	}
}

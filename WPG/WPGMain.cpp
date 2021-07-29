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
#include "WPGAboutEtc.h"
#include "WPGGenerator.h"
#include "WPGRegistry.h"

// Macros
//

// Defines a Windows message which, when sent to the main window, causes it to generate a new password
#define UWM_REFRESH				(AWM_WPG_STOPPED+1L)

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
const DWORD c_dwDuplicatesCheck = 0x00000800;
const DWORD c_dwDefaultChecks = (c_dwRDRANDCheck | c_dwTPMCheck | c_dwDuplicatesCheck);

// Types
//

typedef struct _UIState {

	WPGCaps wpgCaps;
	WPG_H wpgHandle;

	DWORD dwTooltips;
	HWND* phTooltips;

} UIState, *UIStatePtr;

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

// Called when a new password has been generated
HRESULT OnPwdGenerated(HWND, UINT_PTR);

// Called when the main dialog is closed
HRESULT OnClose(HWND);

// Called when the generator thread has stopped
HRESULT OnGeneratorStopped(HWND);

// Returns TRUE if the contents of the input control are the default; FALSE otherwise
LPTSTR NonDefaultAlphabet(HWND);

// Retrieves and sets the given error message
VOID SetErrorMsg(HWND, WPGCaps);

// Scales the given input to the horizontal DPI
INT WPGScaleX(INT);

// Automatically ticks the "Allow duplicates.." checkbox if the desired password length is greater than the input alphabet
VOID AutoCheckDuplicatesAndRefresh(HWND);

// Functions
//

// Gives the entry-point
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {

	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( nCmdShow );

	// Initialise the Common Controls Library; ensure that the progress control is available
	INITCOMMONCONTROLSEX icex = { 0L };
	icex.dwICC = ICC_BAR_CLASSES | ICC_LINK_CLASS;
	icex.dwSize = static_cast<DWORD>( sizeof( INITCOMMONCONTROLSEX ) );
	if (!InitCommonControlsEx( &icex )){
		MessageBox( HWND_DESKTOP, TEXT( "Failed to initialise common controls. Aborting." ), TEXT( "Error" ), MB_OK | MB_ICONERROR );
		return 1;
	}

	// Initialise COM
	int result = 1;
	HRESULT hResult = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
	if (SUCCEEDED( hResult )){
		// Load the icon
		g_hIcon = reinterpret_cast<HICON>( LoadImage( hInstance, MAKEINTRESOURCE( IDI_WAVESON ), IMAGE_ICON, GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), 0 ) );

		// Register the message
		g_uwmTaskbarButtonCreated = RegisterWindowMessage( L"TaskbarButtonCreated" ); 

		// Display the dialog box
		result = DialogBoxParam( hInstance, MAKEINTRESOURCE( IDD_MAIN ), HWND_DESKTOP, MainDialogProc, 0 );

		// Cleanup, return
		if (g_hIcon){
			DestroyIcon( g_hIcon );
		}
		CoUninitialize( );
	}
	return result;
}

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
			const WORD w = wScroll;
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
#if defined (_DEBUG)
				const size_t cchBuf = 256;
				TCHAR szBuf[cchBuf] = { 0 };
				StringCchPrintf( szBuf, cchBuf, TEXT( "Slider changed to %d\x0A" ), (int) wScroll );
				OutputDebugString( szBuf );
#endif
				AutoCheckDuplicatesAndRefresh( hDlg );
			}
		}
			break;

		case WM_COMMAND:
			switch (LOWORD( wParam )){
				case IDC_BUTTON_RESET:
					bResult = SUCCEEDED( OnReset( hDlg ) );
					break;

				case IDC_BUTTON_REFRESH:
					PostMessage( hDlg, UWM_REFRESH, 0, 0 );
					break;

				case IDC_BUTTON_COPY:
					bResult = SUCCEEDED( OnCopy( hDlg ) );
					break;

				case IDC_CHECK_ALLOW_DUPLICATES:
				case IDC_CHECK_RDRAND:
				case IDC_CHECK_TPM:
					if (HIWORD( wParam ) == BN_CLICKED){
						PostMessage( hDlg, UWM_REFRESH, 0, 0 );
					}
					break;

				case IDC_EDIT_INPUT:
					// If the contents of the input have changed,
					// then check whether the 'Allow duplicates...'
					// checkbox should/should not be ticked
					switch (HIWORD( wParam )){
						case EN_CHANGE:
							AutoCheckDuplicatesAndRefresh( hDlg );
							break;

						default:
							bResult = FALSE;
							break;
					}
					break;

				default:
					bResult = FALSE;
					break;
			}
			break;

		// The window's 'X' was clicked
		case WM_CLOSE:
			OnClose( hDlg );
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

		case WM_NOTIFY:
		{
			LPNMHDR lpNmHdr = reinterpret_cast<LPNMHDR>( lParam );
			switch (lpNmHdr->code){
				case NM_CLICK:
				case NM_RETURN:
				{
					HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
					DialogBox( hInstance, MAKEINTRESOURCE( IDD_ABOUT ), hDlg, AboutDialogProc );
				}
					bResult = TRUE;
					break;

				default:
					bResult = FALSE;
					break;
			}
		}
			break;

		case AWM_WPG_STARTED:
			OutputDebugString( TEXT( "AWM_WPG_STARTED\x0D" ) );

			// Fall through
			;

		case UWM_REFRESH:
			OnRefresh( hDlg );
			break;

		case AWM_WPG_GENERATED:
			OnPwdGenerated( hDlg, wParam );
			break;

		case AWM_WPG_STOPPED:
		{
			OnGeneratorStopped( hDlg );

			// Pause (however briefly) before killing the dialog
			if (uTimer != 0L){
				KillTimer( hDlg, uTimer );
			}
			uTimer = SetTimer( hDlg, c_uTimer, c_uTimerElapseMinimum, NULL );
		}
			break;

		default:
			if (bResult = (uMsg == g_uwmTaskbarButtonCreated)){
				// TODO?
			}
			break;
	}
	return bResult;
}

HRESULT OnCreateTooltips(HWND hDlg) {

	// Use the dialog's width as the maximum width
	RECT r = { 0 };
	GetWindowRect( hDlg, &r );
	const LPARAM lWidth = static_cast<LPARAM>( WPGScaleX( r.right - r.left ) );

	const int nDlgItems[] = { IDC_CHECK_RDRAND, IDC_CHECK_TPM, IDC_EDIT_INPUT, IDC_SLIDER_OUTPUT, IDC_CHECK_AUTO_COPY, IDC_CHECK_ALLOW_DUPLICATES };
	const UINT uStringIDs[] = { IDS_CHECK_RDRAND, IDS_CHECK_TPM, IDS_EDIT_INPUT, IDS_SLIDER_OUTPUT, IDS_CHECK_AUTO_COPY, IDS_CHECK_ALLOW_DUPLICATES };
	const unsigned count = min( ARRAYSIZE( nDlgItems ), ARRAYSIZE( uStringIDs ) );

	UIStatePtr uiStatePtr = reinterpret_cast<UIStatePtr>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
	uiStatePtr->dwTooltips = static_cast<DWORD>( count );
	uiStatePtr->phTooltips = reinterpret_cast<HWND*>( PH_ALLOC( sizeof( HWND ) * count ) );

	HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
	for (unsigned i = 0; i < count; ++i){
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

		SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, lWidth );
		*(uiStatePtr->phTooltips + i) = hTooltip;
	}
	return S_OK;
}

HRESULT OnInitDialog(HWND hDlg) {

	// Kick off the generator thread
	WPG_H wpgHandle = StartWPGGenerator( hDlg );

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
	if (caps & (WPGCapTPM12 | WPGCapTPM20)){
		EnableWindow( GetDlgItem( hDlg, IDC_CHECK_TPM ), TRUE );
		uCheckTPM = (dwChecks & c_dwTPMCheck) ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton( hDlg, IDC_CHECK_TPM, uCheckTPM );
	}
	CheckDlgButton(
		hDlg,
		IDC_CHECK_AUTO_COPY,
		(dwChecks & c_dwAutoCopyCheck) ? BST_CHECKED : BST_UNCHECKED
	);
	CheckDlgButton(
		hDlg,
		IDC_CHECK_ALLOW_DUPLICATES,
		(dwChecks & c_dwDuplicatesCheck) ? BST_CHECKED : BST_UNCHECKED
	);

	// Seed the input from registry, above, (or resources) and do the initial 'refresh'
	if (pszAlphabet){
		HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
		SetWindowText( hInput, pszAlphabet );
		PH_FREE( pszAlphabet );
	}else{
		OnReset( hDlg );
	}

	const BOOL bEnable = (uCheckRDRAND == BST_CHECKED) || (uCheckTPM == BST_CHECKED);
	EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_REFRESH ), bEnable );

	// Initialise the UI state
	UIStatePtr uiStatePtr = reinterpret_cast<UIStatePtr>( PH_ALLOC( sizeof( UIState ) ) );
	uiStatePtr->wpgCaps = caps;
	uiStatePtr->wpgHandle = wpgHandle;
	SetWindowLongPtr( hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( uiStatePtr ) );
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

	// Setup
	HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
	const int cchAlphabet = GetWindowTextLength( hInput );

	// Find the intersection between the available generators and the ones the user has selected
	UIStatePtr uiStatePtr = reinterpret_cast<UIStatePtr>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
	WPGCaps caps = uiStatePtr->wpgCaps;
	if (!IsDlgButtonChecked( hDlg, IDC_CHECK_RDRAND )){
		caps &= ~WPGCapRDRAND;
	}
	if (!IsDlgButtonChecked( hDlg, IDC_CHECK_TPM )){
		caps &= ~(WPGCapTPM12 | WPGCapTPM20);
	}

	// Look for an early out
	HWND hOut = GetDlgItem( hDlg, IDC_EDIT_OUTPUT );
	if ((caps == WPGCapNONE) || (cchAlphabet == 0)){
		SetWindowText( hOut, TEXT( "" ) );
		return S_OK;
	}

	// Allocate a buffer for the output
	HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
	BYTE cchPwd = static_cast<BYTE>( SendMessage( hSlider, TBM_GETPOS, 0, 0 ) );
	HANDLE hProcessHeap = GetProcessHeap( );
	LPTSTR pszPwd = static_cast<LPTSTR>( HeapAlloc( hProcessHeap, HEAP_ZERO_MEMORY, sizeof( TCHAR ) * (cchPwd+1) ) );

	// Get a copy of the input alphabet
	LPTSTR pszAlphabet = static_cast<LPTSTR>( HeapAlloc( hProcessHeap, HEAP_ZERO_MEMORY, sizeof( TCHAR ) * (cchAlphabet+1) ) );
	GetWindowText( hInput, pszAlphabet, cchAlphabet );

	// Send to the generator thread
	WPG_H wpgHandle = (uiStatePtr) ? uiStatePtr->wpgHandle : NULL;
	if (wpgHandle){
		const BOOL fDuplicatesAllowed = IsDlgButtonChecked( hDlg, IDC_CHECK_ALLOW_DUPLICATES );
		WPGPwdGenAsync( wpgHandle, cchPwd, caps, pszAlphabet, cchAlphabet, fDuplicatesAllowed );
		return S_OK;
	}
	return E_POINTER;
}

HRESULT OnPwdGenerated(HWND hDlg, UINT_PTR uPtr) {

	PWPGGenerated pWPGGenerated = reinterpret_cast<PWPGGenerated>( uPtr );
	if (pWPGGenerated){
		const WPGCaps wpgCapsFailed = pWPGGenerated->wpgCapsFailed;
		HRESULT hResult = (wpgCapsFailed == WPGCapNONE) ? S_OK : E_FAIL;
		if (SUCCEEDED( hResult )){
			// Set the output
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT_OUTPUT ), pWPGGenerated->pszPwd );
			if (IsDlgButtonChecked( hDlg, IDC_CHECK_AUTO_COPY )){
				hResult = OnCopy( hDlg );
			}
		}else{
			SetErrorMsg( hDlg, wpgCapsFailed );
		}

		// Cleanup
		PH_FREE( const_cast<LPTSTR>( pWPGGenerated->pszPwd ) );
		PH_FREE( pWPGGenerated );
		return hResult;
	}
	return E_INVALIDARG;
}

HRESULT OnClose(HWND hDlg) {

	UIStatePtr uiStatePtr = reinterpret_cast<UIStatePtr>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
	WPG_H wpgHandle = (uiStatePtr) ? uiStatePtr->wpgHandle : NULL;
	if (wpgHandle){
		StopWPGGenerator( wpgHandle );
		return S_OK;
	}
	return OnGeneratorStopped( hDlg );
}

HRESULT OnGeneratorStopped(HWND hDlg) {

	OutputDebugString( TEXT( "AWM_WPG_STOPPED\x0D" ) );

	HKEY hKey = NULL;
	HRESULT hResult = WPGRegOpenKey( &hKey );
	if (SUCCEEDED( hResult )){
		DWORD dwChecks = 1;
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_RDRAND ) ? c_dwRDRANDCheck : 0);
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_TPM ) ? c_dwTPMCheck : 0);
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_AUTO_COPY ) ? c_dwAutoCopyCheck : 0);
		dwChecks |= (IsDlgButtonChecked( hDlg, IDC_CHECK_ALLOW_DUPLICATES ) ? c_dwDuplicatesCheck : 0);
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

	// Cleanup the UI state
	UIStatePtr uiStatePtr = reinterpret_cast<UIStatePtr>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
	if (uiStatePtr){
		for (DWORD dw = 0; dw < uiStatePtr->dwTooltips; ++dw){
			DestroyWindow( *(uiStatePtr->phTooltips + dw) );
		}
		PH_FREE( uiStatePtr );
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

INT WPGScaleX(INT x) {

	static const INT c_nBaseDPI = 96;

	HDC hdc = GetDC( NULL );
	const INT dpi = (hdc) ? GetDeviceCaps( hdc, LOGPIXELSX ) : c_nBaseDPI;
	if (hdc) {
		ReleaseDC( NULL, hdc );
	}
	return MulDiv( x, c_nBaseDPI, dpi );
}

VOID AutoCheckDuplicatesAndRefresh(HWND hDlg) {

	// Retrieve the respective sizes of the input alphabet and the desired output password
	HWND hInput = GetDlgItem( hDlg, IDC_EDIT_INPUT );
	const int cchAlphabet = GetWindowTextLength( hInput );
	HWND hSlider = GetDlgItem( hDlg, IDC_SLIDER_OUTPUT );
	const int cchPwd = static_cast<int>( SendMessage( hSlider, TBM_GETPOS, 0, 0 ) );

	// If the latter is greater than the former, then duplicates MUST be allowed, so..
	BOOL fEnabled = TRUE;
	if (cchPwd > cchAlphabet){
		CheckDlgButton(
			hDlg,
			IDC_CHECK_ALLOW_DUPLICATES,
			BST_CHECKED
		);
		fEnabled = FALSE;
	}
	EnableWindow( GetDlgItem( hDlg, IDC_CHECK_ALLOW_DUPLICATES ), fEnabled );

	// Refresh the generated password with the new configuration
	PostMessage( hDlg, UWM_REFRESH, 0, 0 );
}

// WPGAboutEtc.cpp: defines the implementation for functions for the 'About' dialog.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// Cryptographic API Headers
#include <wincrypt.h>

// Local Project Headers
#include "BitOps.h"
#include "WPGGenerators.h"

// Declarations
#include "WPGAboutEtc.h"

// Functions
//

LPWSTR LoadStringProcessHeap(HINSTANCE hInstance, UINT uID) {

	LPWSTR pszTmp = NULL;
	const int nLength = LoadStringW( hInstance, uID, reinterpret_cast<LPWSTR>( &pszTmp ), 0 ); // On its own, this just returns a pointer to the entire contents of the string table..?
	if (!nLength){
		return NULL;
	}

	// Allocate a null-terminated buffer and copy into it
	LPWSTR pszText = static_cast<LPWSTR>( PH_ALLOC( sizeof( WCHAR ) * (nLength+1) ) );
	StringCchCopyW( pszText, (nLength + 1), pszTmp );
	return pszText;
}

VOID SetAboutStringMaybe(HWND hDlg, int nItem, UINT uString) {

	if (uString > 0){
		HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
		LPWSTR pszString = LoadStringProcessHeap( hInstance, uString );
		if (pszString){
			SetWindowTextW( GetDlgItem( hDlg, nItem ), pszString );
			PH_FREE( pszString );
		}
	}
}

VOID CenterSysLink(HWND hWnd, LONG lWidth) {

	// Get the current window rectangle
	RECT r = { 0 };
	GetWindowRect( hWnd, &r );

	// Map it to its parent (implicitly: the 'About' dialog)
	POINT p = { 0 };
	p.x = r.left;
	p.y = r.top;
	ScreenToClient( GetParent( hWnd ), &p );

	// Get the ideal size
	SIZE s = { 0 };
	s.cy = r.bottom - r.top;
	s.cx = r.right - r.left;
	SendMessage( hWnd, LM_GETIDEALSIZE, lWidth, reinterpret_cast<LPARAM>( &s ) );

	// Move into position
	INT x = p.x + ((lWidth - s.cx) / 2);
	MoveWindow( hWnd, x, p.y, s.cx, s.cy, TRUE );
}

BOOL OnInitAboutDialog(HWND hDlg) {

	// Use one of the pre-set labels as an anchor
	RECT r = { 0 };
	GetWindowRect( GetDlgItem( hDlg, IDC_USING_VEX ), &r );
	const LONG lAnchor = r.right - r.left;

	// Use this anchor to center the system links
	CenterSysLink( GetDlgItem( hDlg, IDC_SYSLINK_EMAIL_SUPPORT ), lAnchor );
	CenterSysLink( GetDlgItem( hDlg, IDC_SYSLINK_GITHUB ), lAnchor );

	// Identify the vector extensions we're using
	UINT uID = 0;
	auto xor = get_vex_xor( );
	switch (xor->vex( )){
		case XORVexMMX:
			uID = IDS_USING_VEX_MMX;
			break;

		case XORVexSSE:
			uID = IDS_USING_VEX_SSE;
			break;

		case XORVexSSE2:
			uID = IDS_USING_VEX_SSE2;
			break;

		case XORVexAVX:
			uID = IDS_USING_VEX_AVX;
			break;

		default:
			// Do nothing
			break;
	}
	SetAboutStringMaybe( hDlg, IDC_USING_VEX, uID );

	// Identify the version of TPM available to us
	const WPGCaps caps = WPGGetCaps( );
	uID = (caps & WPGCapTPM20) ? IDS_TPM_VERSION_20 : ((caps & WPGCapTPM12) ? IDS_TPM_VERSION_12 : 0);
	SetAboutStringMaybe( hDlg, IDC_TPM_VERSION, uID );
	return TRUE;
}

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	BOOL bResult = TRUE;
	switch (uMsg){
		// The dialog was initialised
		case WM_INITDIALOG:
			bResult = OnInitAboutDialog( hDlg );
			break;

		case WM_CLOSE:
			EndDialog( hDlg, 0 );
			break;

		case WM_NOTIFY:
		{
			LPNMHDR lpNmHdr = reinterpret_cast<LPNMHDR>( lParam );
			switch (lpNmHdr->code){
				case NM_CLICK:
				case NM_RETURN:
				{
					UINT uID1 = 0, uID2 = 0;
					switch (lpNmHdr->idFrom){
						case IDC_SYSLINK_EMAIL_SUPPORT:
							uID1 = IDS_EMAIL_SUPPORT;
							uID2 = IDS_EMAIL_CLIENT_ERROR;
							break;

						case IDC_SYSLINK_GITHUB:
							uID1 = IDS_GITHUB_LINK;
							uID2 = IDS_GITHUB_BROWSER_ERROR;
							break;

						default:
							break;
					}
					if (uID1){
						HINSTANCE hInstance = reinterpret_cast<HINSTANCE>( GetWindowLongPtr( hDlg, GWLP_HINSTANCE ) );
						LPWSTR pszBase64 = LoadStringProcessHeap( hInstance, uID1 );
						if (pszBase64){
							// Decode from Base64
							DWORD dwSize = 0;
							CryptStringToBinary( pszBase64, 0, CRYPT_STRING_BASE64, NULL, &dwSize, NULL, NULL );
							const SIZE_T cbUrl = dwSize + sizeof( WCHAR ); // +1 for the null terminator
							LPWSTR pszUrl = reinterpret_cast<LPWSTR>( PH_ALLOC( cbUrl ) );
							CryptStringToBinary( pszBase64, 0, CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>( pszUrl ), &dwSize, NULL, NULL );
#if defined (_DEBUG)
							OutputDebugString( TEXT( "Going to open: " ) );
							OutputDebugString( pszUrl );
							OutputDebugString( TEXT( "\x0A" ) );
#endif

							HINSTANCE hResult = ShellExecute( NULL, TEXT( "open" ), pszUrl, NULL, NULL, SW_SHOWNORMAL );
							if (reinterpret_cast<INT_PTR>( hResult ) < 32){
								LPWSTR pszMsg = LoadStringProcessHeap( hInstance, uID2 );
								if (pszMsg){
									MessageBox( hDlg, pszMsg, TEXT( "Error" ), MB_OK | MB_ICONERROR );
									PH_FREE( pszMsg );
								}else{
									MessageBox( hDlg, TEXT( "Unhandled error" ), TEXT( "Error" ), MB_OK | MB_ICONERROR );
								}
							}
							SecureZeroMemory( pszUrl, cbUrl ); // Don't leave the URL in memory after we're done
							PH_FREE( pszUrl );
							PH_FREE( pszBase64 );
						}
						break;
					}

					// If we get here, then fall through..
					;
				}

				default:
					bResult = FALSE;
					break;
			}
		}
		break;

		default:
			bResult = FALSE;
			break;
	}
	return bResult;
}

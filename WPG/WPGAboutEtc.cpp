// WPGAboutEtc.cpp: defines the implementation for functions for the 'About' dialog.
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

// Cryptographic API Headers
#include <wincrypt.h>

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

INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	BOOL bResult = TRUE;
	switch (uMsg){
		// The dialog was initialised
		case WM_INITDIALOG:
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
							if (reinterpret_cast<INT>( hResult ) < 32){
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

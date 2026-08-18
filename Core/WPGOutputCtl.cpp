// WPGOutputCtl.cpp: implements the "WPGOutput" window class
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "pch.h"

// Local Project Headers
#include "heaps.h"
#include "WPGOutputCtl.h"

// Macros
//

#define WPG_OUTPUT_PADDING	2

// Types
//

// Holds the per-window state
typedef struct _WPGOutputState {

	LPTSTR pszText;		// The internal text buffer
	int cchText;		// The length, in characters, of the text in the buffer (excluding the terminator)
	int cchBuffer;		// The capacity, in characters, of the buffer (including the terminator)
	HFONT hFont;		// The (cached) font with which the text is currently rendered

} WPGOutputState, *PWPGOutputState;

// Functions
//

// Returns the state associated with the given window
static inline PWPGOutputState GetWPGOutputState(HWND hWnd) {
	return reinterpret_cast<PWPGOutputState>( GetWindowLongPtr( hWnd, 0 ) );
}

// Releases the cached font, if any
static VOID DiscardOutputFont(PWPGOutputState pState) {

	if (pState->hFont){
		DeleteObject( pState->hFont );
		pState->hFont = NULL;
	}
}

// Creates a monospace font of the given (cell) height
static HFONT CreateOutputFont(int nHeight) {

	LOGFONT lf = { 0 };
	lf.lfHeight = -nHeight;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	// An empty face name leaves the font mapper to pick the default which matches the pitch and family
	return CreateFontIndirect( &lf );
}

// Creates the largest monospace font in which the given text fits within the given bounds
static HFONT FitOutputFont(HDC hdc, LPCTSTR pszText, int cchText, int cx, int cy) {

	const int nMax = max( 1, cy );
	int lo = 1, hi = nMax, nBest = 1;
	while (lo <= hi){
		const int nMid = lo + ((hi - lo) / 2);
		const HFONT hFont = CreateOutputFont( nMid );
		if (!hFont){
			break;
		}

		SIZE size = { 0 };
		const HGDIOBJ hPrev = SelectObject( hdc, hFont );
		const BOOL bExtent = GetTextExtentPoint32( hdc, pszText, cchText, &size );
		SelectObject( hdc, hPrev );
		DeleteObject( hFont );
		if (!bExtent){
			break;
		}

		if ((size.cx <= cx) && (size.cy <= cy)){
			nBest = nMid;
			lo = nMid + 1;
		}else{
			hi = nMid - 1;
		}
	}
	return CreateOutputFont( nBest );
}

// Copies the given text into the window's internal buffer
static BOOL SetOutputText(PWPGOutputState pState, LPCTSTR pszText) {

	size_t cch = 0;
	auto hr = StringCchLength( pszText, STRSAFE_MAX_CCH, &cch );
	if (FAILED( hr )){
		return FALSE;
	}

	if (cch >= pState->cchBuffer){
		LPTSTR psz = reinterpret_cast<LPTSTR>( PH_ALLOC( (cch + 1) * sizeof( TCHAR ) ) );
		if (!psz){
			return FALSE;
		}
		if (pState->pszText){
			SecureZeroMemory( pState->pszText, pState->cchBuffer * sizeof( TCHAR ) );
			PH_FREE( pState->pszText );
		}
		pState->pszText = psz;
		pState->cchBuffer = (cch + 1);
	}

	// Scrub the previous contents, so none of it survives in the tail of the buffer
	SecureZeroMemory( pState->pszText, pState->cchBuffer * sizeof( TCHAR ) );
	if (cch > 0){
		StringCchCopyN( pState->pszText, pState->cchBuffer, pszText, cch );
	}
	pState->cchText = cch;
	return TRUE;
}

// Paints the window
static VOID PaintOutput(HWND hWnd, PWPGOutputState pState) {

	PAINTSTRUCT ps = { 0 };
	const HDC hdc = BeginPaint( hWnd, &ps );
	if (hdc){
		RECT rc = { 0 };
		GetClientRect( hWnd, &rc );

		// Take the background (and text) colours from the parent, so the control blends into it
		HBRUSH hbr = reinterpret_cast<HBRUSH>( SendMessage(
			GetParent( hWnd ),
			WM_CTLCOLORSTATIC,
			reinterpret_cast<WPARAM>( hdc ),
			reinterpret_cast<LPARAM>( hWnd ) ) );
		if (!hbr){
			// Fallback to the system button-face colour to avoid potential crashes if the parent doesn't handle WM_CTLCOLORSTATIC
			hbr = GetSysColorBrush( COLOR_BTNFACE );
		}
		FillRect( hdc, &ps.rcPaint, hbr );

		if (pState->cchText > 0){
			const int cx = max( 1, (rc.right - rc.left) - (2 * WPG_OUTPUT_PADDING) );
			const int cy = max( 1, (rc.bottom - rc.top) - (2 * WPG_OUTPUT_PADDING) );
			if (!pState->hFont){
				pState->hFont = FitOutputFont( hdc, pState->pszText, pState->cchText, cx, cy );
			}

			if (pState->hFont){
				const HGDIOBJ hPrev = SelectObject( hdc, pState->hFont );
				const int nBkMode = SetBkMode( hdc, TRANSPARENT );
				DrawText( hdc, pState->pszText, pState->cchText, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
				SetBkMode( hdc, nBkMode );
				SelectObject( hdc, hPrev );
			}
		}
		EndPaint( hWnd, &ps );
	}
}

// Handles messages sent to windows of the "WPGOutput" class
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	PWPGOutputState pState = GetWPGOutputState( hWnd );
	switch (uMsg){
		case WM_NCCREATE:
			{
				pState = reinterpret_cast<PWPGOutputState>( PH_ALLOC( sizeof( WPGOutputState ) ) );
				if (!pState){
					return FALSE;
				}
				SetWindowLongPtr( hWnd, 0, reinterpret_cast<LONG_PTR>( pState ) );

				// Seed the buffer with the creation text, if any
				LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>( lParam );
				if (pcs && pcs->lpszName){
					SetOutputText( pState, pcs->lpszName );
				}
			}
			break;

		case WM_SETTEXT:
			if (pState){
				const BOOL bSet = SetOutputText( pState, reinterpret_cast<LPCTSTR>( lParam ) );
				if (bSet){
					DiscardOutputFont( pState );
					InvalidateRect( hWnd, NULL, TRUE );
				}
				return bSet;
			}
			return FALSE;

		case WM_GETTEXTLENGTH:
			return pState ? pState->cchText : 0;

		case WM_GETTEXT:
			if (pState && wParam){
				LPTSTR pszBuffer = reinterpret_cast<LPTSTR>( lParam );
				const int cch = min( static_cast<int>( wParam ) - 1, pState->cchText );
				for (int n = 0; n < cch; ++n){
					pszBuffer[n] = pState->pszText[n];
				}
				pszBuffer[cch] = TEXT( '\0' );
				return cch;
			}
			return 0;

		case WM_SIZE:
			if (pState){
				// The available space has changed, so the text needs to be re-fitted
				DiscardOutputFont( pState );
				InvalidateRect( hWnd, NULL, TRUE );
			}
			break;

		case WM_PAINT:
			if (pState){
				PaintOutput( hWnd, pState );
				return 0;
			}
			break;

		case WM_NCDESTROY:
			if (pState){
				DiscardOutputFont( pState );
				if (pState->pszText){
					SecureZeroMemory( pState->pszText, pState->cchBuffer * sizeof( TCHAR ) );
					PH_FREE( pState->pszText );
				}
				PH_FREE( pState );
			}
            SetWindowLongPtr( hWnd, 0, 0 );
			break;

		default:
			break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

// Registers the "WPGOutput" window class against the given instance
WPG_CORE_EXTERN_C WPG_CORE_API ATOM InitWPGOutputControl(__in HINSTANCE hInstance) {

	WNDCLASSEX wcex = { 0 };
	if (GetClassInfoEx( hInstance, WPG_OUTPUT_CLASS, &wcex )){
		// Already registered
		return static_cast<ATOM>( 1 );
	}

	SecureZeroMemory( &wcex, sizeof( wcex ) );
	wcex.cbSize = sizeof( wcex );
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
	wcex.lpfnWndProc = WndProc;
	wcex.cbWndExtra = sizeof( PWPGOutputState );
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground = NULL; // The background will be painted with the parent's brush
	wcex.lpszClassName = WPG_OUTPUT_CLASS;
	return RegisterClassEx( &wcex );
}

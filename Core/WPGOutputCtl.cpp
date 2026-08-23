// WPGOutputCtl.cpp: implements the window class for the output control
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "pch.h"

// Windows Headers
#include <windowsx.h>

// Local Project Headers
#include "heaps.h"
#include "WPGOutputCtl.h"
#include "WPGGenerator.h"

// Macros
//

#define WPG_OUTPUT_PADDING	2

// The fixed font size, in points; scaled for the device DPI at render time
#define WPG_OUTPUT_FONT_POINTS	16

// Types
//

// Holds the per-window state
typedef struct _WPGOutputState {

	LPTSTR pszText;		// The internal text buffer
	size_t cchText;		// The length, in characters, of the text in the buffer (excluding the terminator)
	size_t cchBuffer;	// The capacity, in characters, of the buffer (including the terminator)
	HFONT hFont;		// The (cached) font with which the text is currently rendered
	int nScrollPos;		// The current horizontal scroll offset, in pixels
	BOOL bDragging;		// Whether the text is currently being drag-scrolled with the mouse
	int nDragAnchorX;	// The client x-coordinate at which the drag began, in pixels
	int nDragAnchorPos;	// The scroll offset at which the drag began, in pixels

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

// Creates the monospace font at the fixed size, scaled for the DPI of the given device
static HFONT CreateOutputFont(HDC hdc) {

	LOGFONT lf = { 0 };
	lf.lfHeight = -MulDiv( WPG_OUTPUT_FONT_POINTS, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	// An empty face name leaves the font mapper to pick the default which matches the pitch and family
	return CreateFontIndirect( &lf );
}

// Ensures the cached font exists and returns the pixel width of the current text
static int MeasureOutputText(HWND hWnd, PWPGOutputState pState) {

	int cx = 0;
	const HDC hdc = GetDC( hWnd );
	if (hdc){
		if (!pState->hFont){
			pState->hFont = CreateOutputFont( hdc );
		}
		if (pState->hFont && (pState->cchText > 0)){
			SIZE size = { 0 };
			const HGDIOBJ hPrev = SelectObject( hdc, pState->hFont );
			if (GetTextExtentPoint32( hdc, pState->pszText, static_cast<int>( pState->cchText ), &size )){
				cx = size.cx;
			}
			SelectObject( hdc, hPrev );
		}
		ReleaseDC( hWnd, hdc );
	}
	return cx;
}

// Updates the horizontal scroll bar to reflect the current text; the bar stays visible but disabled while the text fits
static VOID UpdateOutputScroll(HWND hWnd, PWPGOutputState pState) {

	RECT rc = { 0 };
	GetClientRect( hWnd, &rc );
	const int avail = max( 1, (rc.right - rc.left) - (2 * WPG_OUTPUT_PADDING) );
	const int cxText = MeasureOutputText( hWnd, pState );

	const int nOverflow = (cxText > avail) ? (cxText - avail) : 0;
	pState->nScrollPos = max( 0, min( pState->nScrollPos, nOverflow ) );

	SCROLLINFO si = { 0 };
	si.cbSize = sizeof( si );
	// SIF_DISABLENOSCROLL keeps the (non-client) scroll bar shown, just disabled, when the text fits
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = nOverflow ? (cxText - 1) : 0;
	si.nPage = nOverflow ? avail : 0;
	si.nPos = pState->nScrollPos;
	SetScrollInfo( hWnd, SB_HORZ, &si, TRUE );
}

// Returns the largest valid horizontal scroll offset, in pixels, for the current text
static int GetMaxOutputScroll(HWND hWnd, PWPGOutputState pState) {

	RECT rc = { 0 };
	GetClientRect( hWnd, &rc );
	const int avail = max( 1, (rc.right - rc.left) - (2 * WPG_OUTPUT_PADDING) );
	const int cxText = MeasureOutputText( hWnd, pState );
	return (cxText > avail) ? (cxText - avail) : 0;
}

// Applies a new horizontal scroll offset, clamping it and refreshing the bar and paint as needed
static VOID ScrollOutputTo(HWND hWnd, PWPGOutputState pState, int nPos) {

	nPos = max( 0, min( nPos, GetMaxOutputScroll( hWnd, pState ) ) );
	if (nPos != pState->nScrollPos){
		pState->nScrollPos = nPos;
		SCROLLINFO si = { 0 };
		si.cbSize = sizeof( si );
		si.fMask = SIF_POS;
		si.nPos = nPos;
		SetScrollInfo( hWnd, SB_HORZ, &si, TRUE );
		InvalidateRect( hWnd, NULL, FALSE );
	}
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

// Paints the window, drawing through an off-screen buffer so scrolling doesn't flicker
static VOID PaintOutput(HWND hWnd, PWPGOutputState pState) {

	PAINTSTRUCT ps = { 0 };
	const HDC hdc = BeginPaint( hWnd, &ps );
	if (!hdc){
		return;
	}

	RECT rc = { 0 };
	GetClientRect( hWnd, &rc );
	const int cxClient = rc.right - rc.left;
	const int cyClient = rc.bottom - rc.top;

	// Everything is drawn into a back buffer and blitted to avoid flicker
	const HDC hdcMem = CreateCompatibleDC( hdc );
	const HBITMAP hbmMem = hdcMem ? CreateCompatibleBitmap( hdc, cxClient, cyClient ) : NULL;
	const HDC hdcTarget = hbmMem ? hdcMem : hdc;
	const HGDIOBJ hbmPrev = hbmMem ? SelectObject( hdcMem, hbmMem ) : NULL;

	// Paint white so the control and its scroll bar read as one surface, distinct from the window chrome
	FillRect( hdcTarget, &rc, reinterpret_cast<HBRUSH>( GetStockObject( WHITE_BRUSH ) ) );

	if (pState->cchText > 0){
		if (!pState->hFont){
			pState->hFont = CreateOutputFont( hdcTarget );
		}

		if (pState->hFont){
			const HGDIOBJ hPrev = SelectObject( hdcTarget, pState->hFont );
			const int nBkMode = SetBkMode( hdcTarget, TRANSPARENT );
			const COLORREF crText = SetTextColor( hdcTarget, RGB( 0, 0, 0 ) );

			RECT rcContent = rc;
			rcContent.left += WPG_OUTPUT_PADDING;
			rcContent.right -= WPG_OUTPUT_PADDING;
			const int avail = max( 1, rcContent.right - rcContent.left );

			SIZE size = { 0 };
			const auto cchText = static_cast<int>( pState->cchText );
			GetTextExtentPoint32( hdcTarget, pState->pszText, cchText, &size );
			if (size.cx <= avail){
				// The text fits, so keep it centred within the control
				DrawText( hdcTarget, pState->pszText, cchText, &rcContent, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
			}else{
				// The text overflows: fall back to the locale's natural alignment and offset by the scroll position
				RECT rcText = rcContent;
				rcText.left = (rcContent.left - pState->nScrollPos);
				DrawText( hdcTarget, pState->pszText, cchText, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX );
			}

			SetTextColor( hdcTarget, crText );
			SetBkMode( hdcTarget, nBkMode );
			SelectObject( hdcTarget, hPrev );
		}
	}

	if (hbmMem){
		BitBlt( hdc, 0, 0, cxClient, cyClient, hdcMem, 0, 0, SRCCOPY );
		SelectObject( hdcMem, hbmPrev );
		DeleteObject( hbmMem );
	}
	if (hdcMem){
		DeleteDC( hdcMem );
	}
	EndPaint( hWnd, &ps );
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
			}
			break;

		case WM_GETTEXTLENGTH:
			return pState ? pState->cchText : 0;

		case WM_GETTEXT:
			if (pState && wParam){
				LPTSTR pszBuffer = reinterpret_cast<LPTSTR>( lParam );
				const auto cch = min( static_cast<size_t>( wParam ) - 1, pState->cchText );
				for (size_t n = 0; n < cch; ++n){
					pszBuffer[n] = pState->pszText[n];
				}
				pszBuffer[cch] = TEXT( '\0' );
				return cch;
			}
			return 0;

		case WM_SIZE:
			if (pState){
				// The font is fixed; only the scroll extent depends on the available width
				UpdateOutputScroll( hWnd, pState );
				InvalidateRect( hWnd, NULL, FALSE );
			}
			break;

		case WM_HSCROLL:
			if (pState){
				SCROLLINFO si = { 0 };
				si.cbSize = sizeof( si );
				si.fMask = SIF_RANGE | SIF_POS | SIF_TRACKPOS;
				if (GetScrollInfo( hWnd, SB_HORZ, &si )){
					const int nStep = WPG_OUTPUT_FONT_POINTS;
					int nPos = si.nPos;
					switch (LOWORD( wParam )){
						case SB_LINELEFT:	nPos -= nStep; break;
						case SB_LINERIGHT:	nPos += nStep; break;
						case SB_THUMBTRACK:
						case SB_THUMBPOSITION:	nPos = si.nTrackPos; break;
						case SB_LEFT:		nPos = si.nMin; break;
						case SB_RIGHT:		nPos = si.nMax; break;
						default:
							break;
					}

					const int nMaxPos = max( 0, static_cast<int>( si.nMax ) - static_cast<int>( si.nPage ) + 1 );
					nPos = max( 0, min( nPos, nMaxPos ) );
					if (nPos != pState->nScrollPos){
						pState->nScrollPos = nPos;
						si.fMask = SIF_POS;
						si.nPos = nPos;
						SetScrollInfo( hWnd, SB_HORZ, &si, TRUE );
						InvalidateRect( hWnd, NULL, FALSE );
					}
				}
				return 0;
			}
			break;

		case WM_LBUTTONDOWN:
			if (pState && (GetMaxOutputScroll( hWnd, pState ) > 0)){
				pState->bDragging = TRUE;
				pState->nDragAnchorX = GET_X_LPARAM( lParam );
				pState->nDragAnchorPos = pState->nScrollPos;
				SetCapture( hWnd );
				return 0;
			}
			break;

		case WM_MOUSEMOVE:
			if (pState && pState->bDragging){
				// Dragging right reveals text to the right, so the offset moves opposite to the cursor
				const int dx = GET_X_LPARAM( lParam ) - pState->nDragAnchorX;
				ScrollOutputTo( hWnd, pState, pState->nDragAnchorPos - dx );
				return 0;
			}
			break;

		case WM_LBUTTONUP:
			if (pState && pState->bDragging){
				ReleaseCapture( );
				return 0;
			}
			break;

		case WM_CAPTURECHANGED:
			if (pState){
				pState->bDragging = FALSE;
			}
			break;

		case WM_ERASEBKGND:
			// The paint handler fills the whole client through a back buffer, so skip erasing to avoid flicker
			return 1;

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

		case AWM_WPG_GENERATED:
			if (pState){
				LPCTSTR pszPwd = reinterpret_cast<LPCTSTR>( wParam );
				const BOOL bSet = SetOutputText( pState, pszPwd );
				if (bSet){
					// The font is fixed, so only the scroll extent needs to be recomputed
					pState->nScrollPos = 0;
					UpdateOutputScroll( hWnd, pState );
					InvalidateRect( hWnd, NULL, FALSE );
				}
				return bSet;
			}
			return FALSE;

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
	wcex.hbrBackground = NULL;
	wcex.lpszClassName = WPG_OUTPUT_CLASS;
	return RegisterClassEx( &wcex );
}

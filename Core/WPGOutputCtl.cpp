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

// The hot key identifier
#define WPG_HOTKEY_ID			4321

// The hot key character code ('w')
#define WPG_HOTKEY_VK_CODE		0x57

// Types
//

// Holds the per-window state
typedef struct _WPGOutputState {

	HFONT hFont;		// The (cached) font with which the text is currently rendered
	HBITMAP hBitmap;	// The (cached) bitmap containing the rendered output text
	SIZE sizeBitmap;	// The backing bitmap dimensions, in pixels
	int nScrollPos;		// The current horizontal scroll offset, in pixels
	BOOL bDragging;		// Whether the text is currently being drag-scrolled with the mouse
	int nDragAnchorX;	// The client x-coordinate at which the drag began, in pixels
	int nDragAnchorPos;	// The scroll offset at which the drag began, in pixels
	size_t cch;			// Gives the number of characters held in the internal buffer
	int hotkeyId;		// Gives the id of the registered hotkey, or 0 if none registered

	// Gives the state's internal buffer
	SIZE_T cbBuffer;
	LPVOID pBuffer;

} WPGOutputState, *PWPGOutputState;

// Constants
//

static const DWORD c_dwCryptProtectMemoryFlags = CRYPTPROTECTMEMORY_SAME_PROCESS;

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

// Releases the cached text bitmap, if any
static VOID DiscardOutputBitmap(PWPGOutputState pState) {

	if (pState->hBitmap){
		DeleteObject( pState->hBitmap );
		pState->hBitmap = NULL;
	}
	pState->sizeBitmap.cx = 0;
	pState->sizeBitmap.cy = 0;
}

// Creates the monospace font at the fixed size, scaled for the DPI of the given device
static HFONT CreateOutputFont(HDC hdc) {

	// An empty face name leaves the font mapper to pick the default which matches the pitch and family
	LOGFONT lf = { 0 };
	lf.lfHeight = -MulDiv( WPG_OUTPUT_FONT_POINTS, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	return CreateFontIndirect( &lf );
}

// Renders the current output string into a bitmap owned by the window state
static BOOL RenderOutputBitmap(HWND hWnd, PWPGOutputState pState) {

	const auto pWpgBuffer = reinterpret_cast<PWPG_BUFFER>( pState->pBuffer );
	if ((pWpgBuffer == nullptr) || (pWpgBuffer->cch < 1)){
		return TRUE;
	}

	const HDC hdc = GetDC( hWnd );
	if (!hdc){
		return FALSE;
	}
	if (!pState->hFont){
		pState->hFont = CreateOutputFont( hdc );
	}
	if (!pState->hFont){
		ReleaseDC( hWnd, hdc );
		return FALSE;
	}

	SIZE size = { 0 };
	const HGDIOBJ hPrev = SelectObject( hdc, pState->hFont );
	const BOOL bMeasured = GetTextExtentPoint32( hdc, pWpgBuffer->szBuf, static_cast<int>( pWpgBuffer->cch ), &size );
	SelectObject( hdc, hPrev );
	if (!bMeasured || (size.cx < 1) || (size.cy < 1)){
		ReleaseDC( hWnd, hdc );
		return FALSE;
	}

	RECT rcClient = { 0 };
	GetClientRect( hWnd, &rcClient );
	const int cxClient = max( 1, rcClient.right - rcClient.left );
	const int cyClient = max( 1, rcClient.bottom - rcClient.top );
	const int cxBitmap = max( cxClient, size.cx + (2 * WPG_OUTPUT_PADDING) );
	const HDC hdcMem = CreateCompatibleDC( hdc );
	const HBITMAP hBitmap = hdcMem ? CreateCompatibleBitmap( hdc, cxBitmap, cyClient ) : NULL;
	if (!hBitmap){
		if (hdcMem){
			DeleteDC( hdcMem );
		}
		ReleaseDC( hWnd, hdc );
		return FALSE;
	}

	const HGDIOBJ hbmPrev = SelectObject( hdcMem, hBitmap );
	RECT rcBitmap = { 0, 0, cxBitmap, cyClient };
	FillRect( hdcMem, &rcBitmap, GetSysColorBrush( COLOR_WINDOW ) );
	const HGDIOBJ hFontPrev = SelectObject( hdcMem, pState->hFont );
	SetBkMode( hdcMem, TRANSPARENT );
	SetTextColor( hdcMem, GetSysColor( COLOR_WINDOWTEXT ) );
	const int x = (cxBitmap == cxClient) ? ((cxClient - size.cx) / 2) : WPG_OUTPUT_PADDING;
	const int y = (cyClient - size.cy) / 2;
	TextOut( hdcMem, x, y, pWpgBuffer->szBuf, static_cast<int>( pWpgBuffer->cch ) );
	SelectObject( hdcMem, hFontPrev );
	SelectObject( hdcMem, hbmPrev );
	DeleteDC( hdcMem );
	ReleaseDC( hWnd, hdc );

	// Assign and return
	pState->hBitmap = hBitmap;
	pState->sizeBitmap.cx = cxBitmap;
	pState->sizeBitmap.cy = cyClient;
	return TRUE;
}

// Un-protects the text prior to and re-protects it after rendering it into a bitmap
static BOOL UnlockRenderOutputBitmap(HWND hWnd, PWPGOutputState pState) {

	// Clear the slate
	DiscardOutputBitmap( pState );

	// Look for an early out
	if (pState->cch < 1 || pState->pBuffer == NULL){
		return TRUE;
	}

	if (CryptUnprotectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags )){
		const auto result = RenderOutputBitmap( hWnd, pState );
		CryptProtectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags );
		return result;
	}
	return FALSE;
}

// Updates the horizontal scroll bar to reflect the current text; the bar stays visible but disabled while the text fits
static VOID UpdateOutputScroll(HWND hWnd, PWPGOutputState pState) {

	RECT rc = { 0 };
	GetClientRect( hWnd, &rc );
	const int avail = max( 1, rc.right - rc.left );
	const int cxText = pState->sizeBitmap.cx;

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
	const int avail = max( 1, rc.right - rc.left );
	const int cxText = pState->sizeBitmap.cx;
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
static BOOL SetOutputText(PWPGOutputState pState, PWPG_BUFFER pWpgBuffer) {

	// Figure out the number of blocks needed
	const auto cb = pWpgBuffer->Cb( );
	auto blocks = (cb / CRYPTPROTECTMEMORY_BLOCK_SIZE);
	if (cb % CRYPTPROTECTMEMORY_BLOCK_SIZE){
		blocks += 1;
	}
	const auto cbBuffer = (blocks * CRYPTPROTECTMEMORY_BLOCK_SIZE);

	// (Re)allocate the buffer if needed
	if (cbBuffer > pState->cbBuffer){
		auto pBuffer = _aligned_malloc( cbBuffer, alignof( WPG_BUFFER ) );
		if (!pBuffer){
			return FALSE;
		}

		// Cleanup the old buffer
		if (pState->pBuffer){
			SecureZeroMemory( pState->pBuffer, pState->cbBuffer );
			_aligned_free( pState->pBuffer );
		}

		pState->pBuffer = pBuffer;
		pState->cbBuffer = cbBuffer;
	}

	// Copy and lock
	CopyMemory( pState->pBuffer, pWpgBuffer, cb );
	pState->cch = pWpgBuffer->cch;
	return CryptProtectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags );
}

static size_t UnlockGetOutputText(PWPGOutputState pState, LPTSTR pszBuffer, size_t cchBuffer) {

	// Look for an early out
	if (pState->cch < 1 || pState->pBuffer == NULL){
		return 0;
	}

	if (CryptUnprotectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags )){
		const auto pWpgBuffer = reinterpret_cast<PWPG_BUFFER>( pState->pBuffer );

		const auto cch = min( cchBuffer - 1, pWpgBuffer->cch );
		for (size_t n = 0; n < cch; ++n){
			pszBuffer[n] = pWpgBuffer->szBuf[n];
		}
		pszBuffer[cch] = TEXT( '\0' );

		CryptProtectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags );
		return cch;
	}
	return 0;
}

// Paints the window by blitting the cached output bitmap at the current scroll position
static VOID PaintOutput(HWND hWnd, PWPGOutputState pState) {

	PAINTSTRUCT ps = { 0 };
	const HDC hdc = BeginPaint( hWnd, &ps );
	if (!hdc){
		return;
	}

	RECT rc = { 0 };
	GetClientRect( hWnd, &rc );
	if (pState->hBitmap){
		const HDC hdcMem = CreateCompatibleDC( hdc );
		if (hdcMem){
			const int cxClient = rc.right - rc.left;
			const int cyClient = rc.bottom - rc.top;
			const HGDIOBJ hbmPrev = SelectObject( hdcMem, pState->hBitmap );
			BitBlt( hdc, 0, 0, cxClient, cyClient, hdcMem, pState->nScrollPos, 0, SRCCOPY );
			SelectObject( hdcMem, hbmPrev );
			DeleteDC( hdcMem );
		}
	}else{
		FillRect( hdc, &rc, GetSysColorBrush( COLOR_WINDOW ) );
	}
	EndPaint( hWnd, &ps );
}

static BOOL HotKeyPressed(PWPGOutputState pState, WPARAM wParam, LPARAM lParam) {

	if (wParam == WPG_HOTKEY_ID){
#if defined (_DEBUG)
		OutputDebugString( TEXT( "Hotkey pressed.\x0a" ) );
#endif

		if (pState->cch > 0){
			// Allocate the input array
			const auto inputCount = static_cast<UINT>(2 * pState->cch);
			const auto cbInputs = sizeof( INPUT ) * inputCount;
			auto inputs = static_cast<PINPUT>( PH_ALLOC( cbInputs ) );

			// Unlock the buffer and fill the array
			if (CryptUnprotectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags )){
				const auto pWpgBuffer = reinterpret_cast<PWPG_BUFFER>( pState->pBuffer );

				auto ptr = inputs;
				for (size_t n = 0; n < pState->cch; ++n) {
					// Populate the input
					INPUT input = { 0 };
					input.type = INPUT_KEYBOARD;
					input.ki.wScan = pWpgBuffer->szBuf[n];
					input.ki.dwFlags = KEYEVENTF_UNICODE;

					// Key down
					CopyMemory( ptr, &input, sizeof( INPUT ) );
					++ptr;

					// Key up
					input.ki.dwFlags |= KEYEVENTF_KEYUP;
					CopyMemory( ptr, &input, sizeof( INPUT ) );
					++ptr;
				}

				CryptProtectMemory( pState->pBuffer, static_cast<DWORD>( pState->cbBuffer ), c_dwCryptProtectMemoryFlags );

				// Send it
				auto sent = SendInput( inputCount, inputs, sizeof( INPUT ) );
#if defined (_DEBUG)
				if (sent == inputCount){
					TCHAR szBuf[128] = { 0 };
					StringCchPrintf( szBuf, _countof( szBuf ), TEXT( "Sent %u inputs \x0A" ), inputCount );
					OutputDebugString( szBuf );
				}else{
					TCHAR szBuf[128] = { 0 };
					StringCchPrintf( szBuf, _countof( szBuf ), TEXT( "Failed to send %u inputs with error %u (%u)\x0A" ), inputCount, GetLastError( ), sent );
					OutputDebugString( szBuf );
				}
#endif
			}

			// Cleanup prior to returning
			SecureZeroMemory( inputs, cbInputs );
			PH_FREE( inputs );
		}

		return TRUE;
	}

#if defined (_DEBUG)
	OutputDebugString( TEXT( "Unrecognised hotkey pressed.\x0a" ) );
#endif
	return FALSE;
}

static void CleanupState(HWND hWnd) {

	PWPGOutputState pState = GetWPGOutputState( hWnd );
	if (pState){
		if (pState->hotkeyId){
			UnregisterHotKey( hWnd, pState->hotkeyId );
		}

		DiscardOutputBitmap( pState );
		DiscardOutputFont( pState );
		if (pState->pBuffer){
			SecureZeroMemory( pState->pBuffer, pState->cbBuffer );
			_aligned_free( pState->pBuffer );
		}
		PH_FREE( pState );
	}
	SetWindowLongPtr( hWnd, 0, 0 );
}

static BOOL InitState(HWND hWnd) {

	// Allocate, set the state
	auto pState = reinterpret_cast<PWPGOutputState>( PH_ALLOC( sizeof( WPGOutputState ) ) );
	if (!pState){
		return FALSE;
	}
	SetWindowLongPtr( hWnd, 0, reinterpret_cast<LONG_PTR>( pState ) );

	if (RegisterHotKey( hWnd, WPG_HOTKEY_ID, MOD_ALT | MOD_SHIFT | MOD_CONTROL, WPG_HOTKEY_VK_CODE )){
		pState->hotkeyId = WPG_HOTKEY_ID;
	}

#if defined (_DEBUG)
	if (pState->hotkeyId == WPG_HOTKEY_ID){
		OutputDebugString( TEXT( "Registered hotkey\x0A" ) );
	}else{
		OutputDebugString( TEXT( "Failed to register hotkey\x0a" ) );
	}
#endif
	return TRUE;
}

static LRESULT GetHotKeyString(PWPGOutputState pState, LPTSTR pszBuffer, size_t cchBuffer) {

	// Look for an early out
	if (pState->hotkeyId == 0){
		return 0;
	}

	BYTE ks[256] = {};
	if (!GetKeyboardState( ks )){
		return 0;
	}

	// Convert the virtual key code to a character (or characters..?)
	TCHAR szBuf[16] = { 0 };
	auto translated = ToUnicodeEx(
		WPG_HOTKEY_VK_CODE,
		MapVirtualKey( WPG_HOTKEY_VK_CODE, MAPVK_VK_TO_VSC ),
		ks,
		szBuf,
		_countof( szBuf ),
		0,
		GetKeyboardLayout( 0 )
	);
	if (translated < 1) {
		// Can't use
		return 0;
	}

	// Format and emit the string
	szBuf[translated] = 0;
	auto hr = StringCchPrintf( pszBuffer, cchBuffer, TEXT( "Ctrl + Shift + Alt + %s" ), szBuf );
	if (FAILED( hr )){
		return 0;
	}

	// Get the length
	size_t cch;
	hr = StringCchLength( pszBuffer, cchBuffer, &cch );
	if (FAILED( hr )){
		return 0;
	}

	return static_cast<LRESULT>( cch );
}

// Handles messages sent to windows of the "WPGOutput" class
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	PWPGOutputState pState = GetWPGOutputState( hWnd );
	switch (uMsg){
		case WM_NCCREATE:
			if (!InitState( hWnd )){
				return FALSE;
			}
			break;

		case WM_GETTEXTLENGTH:
			return (pState) ? (pState->cch) : 0;

		case WM_GETTEXT:
			if (pState && wParam){
				return UnlockGetOutputText( pState, reinterpret_cast<LPTSTR>( lParam ), static_cast<size_t>( wParam ) );
			}
			return 0;

		case WM_SIZE:
			if (pState){
				UnlockRenderOutputBitmap( hWnd, pState );
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

		case WM_HOTKEY:
			if (HotKeyPressed( pState, wParam, lParam )){
				return 0;
			}
			break;

		case WM_NCDESTROY:
			CleanupState( hWnd );
			break;

		case AWM_WPG_GENERATED:
			if (pState){
				const BOOL bSet = SetOutputText( pState, reinterpret_cast<PWPG_BUFFER>( wParam ) );
				const BOOL bRendered = bSet && UnlockRenderOutputBitmap( hWnd, pState );
				if (bRendered){
					// The font is fixed, so only the scroll extent needs to be recomputed
					pState->nScrollPos = 0;
					UpdateOutputScroll( hWnd, pState );
					InvalidateRect( hWnd, NULL, FALSE );
				}
				return bRendered;
			}
			return FALSE;

		case AWM_WPG_GET_HOTKEY_STR:
			return GetHotKeyString( pState, reinterpret_cast<LPTSTR>( lParam ), static_cast<size_t>( wParam ) );

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

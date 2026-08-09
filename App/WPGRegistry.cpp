// WPGRegistry.cpp: defines the implementation for functions for working with the windows registry.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// Declarations
#include "WPGRegistry.h"

// Macros
//

#define WPG_REG_VALUE_NAME(id) c_pszRegistryValueNames[(id)-1]
#define WPG_REG_VALUE_TYPE(id) c_dwRegistryValueTypes[(id)-1]

// Constants
//

static LPCTSTR c_pszRegistryKeyPath = TEXT( "Software\\Waveson\\Password Generator" );

static LPCTSTR c_pszRegistryValueNames[] = {
	TEXT( "Checks" ),
	TEXT( "Alphabet" ),
	TEXT( "Length" )
};

static DWORD c_dwRegistryValueTypes[] = { REG_DWORD, REG_SZ, REG_DWORD };

// Functions
//

HRESULT WPGRegOpenKey(PHKEY phKey) {

	HRESULT hResult = (phKey != NULL) ? S_OK : E_INVALIDARG;
	if (SUCCEEDED( hResult )){
		HKEY hKeyCurrentUser = NULL;
		LSTATUS lStatus = RegOpenCurrentUser( KEY_READ | KEY_WRITE, &hKeyCurrentUser );
		if (lStatus == ERROR_SUCCESS){
			// Open/create the actual key
			DWORD dwDisposition = 0;
			lStatus = RegCreateKeyEx( hKeyCurrentUser, c_pszRegistryKeyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, phKey, &dwDisposition );
			RegCloseKey( hKeyCurrentUser );
		}
		hResult = (lStatus == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32( lStatus ));
	}
	return hResult;
}

HRESULT WPGRegSetDWORD(HKEY hKey, WPGRegValueID id, DWORD dwValue) {

	LPCTSTR pszValueName = WPG_REG_VALUE_NAME( id );
	if (!pszValueName){
		return E_INVALIDARG;
	}

	DWORD dwType = REG_DWORD;
	LONG lResult = RegSetValueEx( hKey, pszValueName, 0, dwType, reinterpret_cast<const BYTE*>( &dwValue ), static_cast<DWORD>( sizeof( DWORD ) ) );
	if (lResult == ERROR_SUCCESS){
		return S_OK;
	}
	return HRESULT_FROM_WIN32( lResult );
}

HRESULT WPGRegSetString(HKEY hKey, WPGRegValueID id, __in_z LPCTSTR pszValue) {

	LPCTSTR pszValueName = WPG_REG_VALUE_NAME( id );
	if (!pszValueName){
		return E_INVALIDARG;
	}

	size_t cchValue = 0;
	StringCchLength( pszValue, STRSAFE_MAX_CCH, &cchValue );
	const DWORD dwSize = static_cast<DWORD>( sizeof( TCHAR ) * cchValue );

	DWORD dwType = REG_SZ;
	LONG lResult = RegSetValueEx( hKey, pszValueName, 0, dwType, reinterpret_cast<const BYTE*>( pszValue ), dwSize );
	if (lResult == ERROR_SUCCESS){
		return S_OK;
	}
	return HRESULT_FROM_WIN32( lResult );
}

HRESULT WPGRegGetDWORD(__out LPDWORD lpValue, HKEY hKey, WPGRegValueID id) {

	LPCTSTR pszValueName = WPG_REG_VALUE_NAME( id );
	if (!pszValueName){
		return E_INVALIDARG;
	}

	DWORD dwType = REG_DWORD;
	DWORD dwSize = static_cast<DWORD>( sizeof( DWORD ) ); // ooh, very tautological..
	LONG lResult = RegQueryValueEx( hKey, pszValueName, 0, &dwType, reinterpret_cast<LPBYTE>( lpValue ), &dwSize );
	if (lResult == ERROR_SUCCESS){
		return S_OK;
	}
	return HRESULT_FROM_WIN32( lResult );
}

HRESULT WPGRegGetString(__out LPTSTR* ppszValue, HKEY hKey, WPGRegValueID id) {

	LPCTSTR pszValueName = WPG_REG_VALUE_NAME( id );
	if (!pszValueName){
		return E_INVALIDARG;
	}

	// Query for the size, first
	DWORD dwType = REG_SZ;
	DWORD dwSize = 0;
	LONG lResult = RegQueryValueEx( hKey, pszValueName, 0, &dwType, NULL, &dwSize );
	if (lResult != ERROR_SUCCESS){
		return HRESULT_FROM_WIN32( lResult );
	}

	// Allocate the buffer and query for the data
	LPBYTE lpValue = static_cast<LPBYTE>( PH_ALLOC( dwSize + sizeof( TCHAR ) ) );
	lResult = RegQueryValueEx( hKey, pszValueName, 0, &dwType, lpValue, &dwSize );
	if (lResult == ERROR_SUCCESS){
		*ppszValue = reinterpret_cast<LPTSTR>( lpValue );
		return S_OK;
	}
	PH_FREE( lpValue );
	return HRESULT_FROM_WIN32( lResult );
}

HRESULT WPGRegDelete(HKEY hKey, WPGRegValueID id) {

	LPCTSTR pszValueName = WPG_REG_VALUE_NAME( id );
	if (!pszValueName){
		return E_INVALIDARG;
	}

	// Query for the size, first
	DWORD dwType = WPG_REG_VALUE_TYPE( id ), dwSize = 0;
	LONG lResult = RegQueryValueEx( hKey, pszValueName, 0, &dwType, NULL, &dwSize );
	if (lResult != ERROR_SUCCESS){
		return (lResult == ERROR_FILE_NOT_FOUND) ? S_OK : HRESULT_FROM_WIN32( lResult );
	}

	// If we get here, the value exists?
	lResult = RegDeleteValue( hKey, pszValueName );
	if (lResult == ERROR_SUCCESS){
		return S_OK;
	}
	return HRESULT_FROM_WIN32( lResult );
}

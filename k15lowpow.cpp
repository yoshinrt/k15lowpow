/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	k15lowpow.cpp - main
	
*****************************************************************************/

#include "StdAfx.h"
#include "CProcessor.h"

/*** constant definition ****************************************************/

#define WINDOW_TITLE	_T( "K15 low power control" )		// NULL 可
#define WINDOW_CLASS	_T( "K15LowPowClass" )

/*** gloval var definition **************************************************/

_TCHAR		*szWinClassName = WINDOW_CLASS;

HINSTANCE	g_hInst;
HWND		g_hWnd;
CProcessor	*g_pCpu = NULL;
UINT		g_uTimerInterval = 200;

/*** WinRing0 ***************************************************************/

void InitWinRing0( void ){

	int dllStatus;
	BYTE verMajor,verMinor,verRevision,verRelease;

	InitializeOls ();

	dllStatus=GetDllStatus ();
	
	if (dllStatus!=0) {
		switch (dllStatus) {
			case OLS_DLL_UNSUPPORTED_PLATFORM:
				throw CK15LPError( "WinRing0: unsupported platform\n" );
				break;
			case OLS_DLL_DRIVER_NOT_LOADED:
				throw CK15LPError( "WinRing0: driver not loaded\n" );
				break;
			case OLS_DLL_DRIVER_NOT_FOUND:
				throw CK15LPError( "WinRing0: driver not found\n" );
				break;
			case OLS_DLL_DRIVER_UNLOADED:
				throw CK15LPError( "WinRing0: driver unloaded by other process\n" );
				break;
			case OLS_DLL_DRIVER_NOT_LOADED_ON_NETWORK:
				throw CK15LPError( "WinRing0: driver not loaded from network\n" );
				break;
		//	case OLS_DLL_UNKNOWN_ERROR:
		//		throw CK15LPError( "WinRing0: unknown error\n" );
		//		break;
			default:
				throw CK15LPError( "WinRing0: unknown error\n" );
		}
	}
	
	GetDriverVersion (&verMajor,&verMinor,&verRevision,&verRelease);
	
	if( !((verMajor>=1) && (verMinor>=2)))
		throw CK15LPError( "WinRing0: version 1.2 or later required\n" );
}

int CloseWinRing0 () {
	DeinitializeOls ();	
	return TRUE;
}

/*** H/W control ************************************************************/

#define ID_TIMER	0

bool Init( void ){
	try{
		InitWinRing0();
		if( !CProcessor::IsSupported()) throw( "Error: unsupported CPU" );
		
		g_pCpu = new CProcessor();
		g_pCpu->Init();
		
		SetTimer( g_hWnd, ID_TIMER, g_uTimerInterval, NULL );
		
	}catch( CK15LPError& err ){
		MessageBoxA( NULL, err.what(), "k15lowpow", MB_ICONSTOP | MB_OK );
		return false;
	}
	
	return true;
}

void DeInit( void ){
	delete g_pCpu;
	CloseWinRing0();
}

/*** window procedure *******************************************************/

LRESULT CALLBACK WindowProc(
	HWND	hWnd,
	UINT	Message,
	WPARAM	wParam,
	LPARAM	lParam ){
	
	switch( Message ){
	  case WM_CREATE:
		g_hWnd = hWnd;
		if( !Init()) PostQuitMessage( 1 );
		
	  Case WM_TIMER:
		g_pCpu->PowerManagement();
		
	  Case WM_DESTROY:				/* terminated by user					*/
		DeInit();
		PostQuitMessage( 1 );
		
	  Default:
		return( DefWindowProc( hWnd, Message, wParam, lParam ));
	}
	return( 0 );
}

/*** main procedure *********************************************************/

int WINAPI WinMain(
	HINSTANCE	hInst,
	HINSTANCE	hPrevInst,
	LPSTR		lpCmdLine,
	int			iCmdShow ){
	
	/* for creating window */
	
	HWND		hWnd;
	MSG			Msg;
	WNDCLASS	wcl;
	
	/*** cleate a window ****************************************************/
	
	g_hInst = hInst;
	
	/* define a window class */
	
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= szWinClassName;
	wcl.lpfnWndProc		= WindowProc;
	wcl.style			= 0;
	
	wcl.hIcon			= LoadIcon( NULL, IDI_APPLICATION );
	wcl.hCursor			= LoadCursor( NULL, IDC_ARROW );
	wcl.lpszMenuName	= NULL;
	
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	
	wcl.hbrBackground	=( HBRUSH )GetStockObject( WHITE_BRUSH );
	
	/* register the window class */
	
	if( !RegisterClass( &wcl )) return( 1 );
	
	g_hWnd = hWnd = CreateWindow(
		szWinClassName,
		WINDOW_TITLE,			// window title
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,			// x
		CW_USEDEFAULT,			// y
		320, //CW_USEDEFAULT,			// w
		240, //CW_USEDEFAULT,			// h
		HWND_DESKTOP,
		NULL,
		hInst,
		NULL
	);
	
	/* display the window */
	
	ShowWindow( hWnd, iCmdShow );
	UpdateWindow( hWnd );
	
	/* create the message loop */
	
	while( GetMessage( &Msg, NULL, 0, 0 )){
		TranslateMessage( &Msg );
		DispatchMessage( &Msg );
	}
	
	CloseWinRing0();
	
	return( 0 );
}

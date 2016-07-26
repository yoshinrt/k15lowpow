/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	k15lowpow.cpp - main
	
*****************************************************************************/

#include "StdAfx.h"
#include "CProcessor.h"

/*** constant definition ****************************************************/

#define WINDOW_TITLE	_T( "Sample window" )		// NULL 可
#define WINDOW_CLASS	_T( "SampleAppClass" )

/*** gloval var definition **************************************************/

_TCHAR		*szWinClassName = WINDOW_CLASS;

HINSTANCE	g_hInst;
HWND		g_hWnd;
CProcessor	*g_pCpu;

/*** WinRing0 ***************************************************************/

int InitWinRing0 () {

	int dllStatus;
	BYTE verMajor,verMinor,verRevision,verRelease;

	InitializeOls ();

	dllStatus=GetDllStatus ();
	
	if (dllStatus!=0) {
		DebugMsgD(_T( "Unable to initialize WinRing0 library\n" ));

		switch (dllStatus) {
			case OLS_DLL_UNSUPPORTED_PLATFORM:
				DebugMsgD(_T( "Error: unsupported platform\n" ));
				break;
			case OLS_DLL_DRIVER_NOT_LOADED:
				DebugMsgD(_T( "Error: driver not loaded\n" ));
				break;
			case OLS_DLL_DRIVER_NOT_FOUND:
				DebugMsgD(_T( "Error: driver not found\n" ));
				break;
			case OLS_DLL_DRIVER_UNLOADED:
				DebugMsgD(_T( "Error: driver unloaded by other process\n" ));
				break;
			case OLS_DLL_DRIVER_NOT_LOADED_ON_NETWORK:
				DebugMsgD(_T( "Error: driver not loaded from network\n" ));
				break;
			case OLS_DLL_UNKNOWN_ERROR:
				DebugMsgD(_T( "Error: unknown error\n" ));
				break;
			default:
				DebugMsgD(_T( "Error: unknown error\n" ));
		}
		
		return FALSE;
	}
	
	GetDriverVersion (&verMajor,&verMinor,&verRevision,&verRelease);
	
	if ((verMajor>=1) && (verMinor>=2)) return TRUE;
	
	return FALSE;
}

int CloseWinRing0 () {
	DeinitializeOls ();	
	return TRUE;
}

/*** H/W control ************************************************************/

#define ID_TIMER	0

void Init( HWND hWnd ){
	InitWinRing0();
	g_pCpu = new CProcessor();
	g_pCpu->Init();
	
	SetTimer( hWnd, ID_TIMER, 500, NULL );
}

/*** window procedure *******************************************************/

LRESULT CALLBACK WindowProc(
	HWND	hWnd,
	UINT	Message,
	WPARAM	wParam,
	LPARAM	lParam ){
	
	switch( Message ){
	  case WM_CREATE:
		Init( hWnd );
		
	  Case WM_TIMER:
		DebugMsgD( _T( "TIMER\n" ));
		
		
	  Case WM_DESTROY:				/* terminated by user					*/
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

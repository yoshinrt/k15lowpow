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
LPTSTR		g_lpCmdLine;

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

/*** Load config file *******************************************************/

#define BUF_SIZE	1024

bool GetUintConfig( const char *szBuf, const char *szName, UINT& val ){
	if( strncmp( szBuf, szName, strlen( szName ))) return false;
	val = atoi( szBuf + strlen( szName ));
	return true;
}

void LoadConfig( void ){
	TCHAR	*szFileName = new TCHAR[ _tcslen( g_lpCmdLine ) + 1 ];
	char	szBuf[ BUF_SIZE ];
	TCHAR	*szParam = g_lpCmdLine;
	FILE	*fp = NULL;
	
	UINT	uPState = 0;
	UINT	uPStateConfFlag = 0;
	UINT	uFid, uVid, uDid;
	
	enum{
		PSF_FID	= 1 << 0,
		PSF_DID = 1 << 1,
		PSF_VID = 1 << 2,
		PSF_ALL = ( 1 << 3 ) - 1,
	};
	
	try{
		// ファイル名 get
		if( StrGetParam( szFileName, &szParam ) == NULL ) throw CK15LPError( "Error: config file not specified" );
		
		// ファイルオープン
		if(( fp = _tfopen( szFileName, _T( "r" ))) == NULL ) throw CK15LPError( "Error: can't open file" );
		
		while( fgets( szBuf, BUF_SIZE - 1, fp )){
			if( GetUintConfig( szBuf, "[Pstate_", uPState )){
				uPStateConfFlag = 0;
			}else if( GetUintConfig( szBuf, "fid=", uFid )){
				uPStateConfFlag |= PSF_FID;
			}else if( GetUintConfig( szBuf, "did=", uDid )){
				uPStateConfFlag |= PSF_DID;
			}else if( GetUintConfig( szBuf, "vid=", uVid )){
				uPStateConfFlag |= PSF_VID;
			}
			
			if( uPStateConfFlag == PSF_ALL ){
				DebugMsgD( _T( "P%d: f:%d d:%d v:%d\n" ), uPState, uFid, uDid, uVid );
				g_pCpu->SetupPState( uPState, uFid, uDid, uVid );
				uPStateConfFlag = 0;
			}
		}
		
	}catch( CK15LPError& err ){
		if( fp ) fclose( fp );
		delete [] szFileName;
		throw err;
	}
	
	if( fp ) fclose( fp );
	delete [] szFileName;
}

/*** H/W control ************************************************************/

#define ID_TIMER	0

bool Init( void ){
	try{
		InitWinRing0();
		if( !CProcessor::IsSupported()) throw( "Error: unsupported CPU" );
		
		g_pCpu = new CProcessor();
		g_pCpu->Init();
		LoadConfig();
		g_pCpu->PowerManagementInit();
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
		
	  Case WM_POWERBROADCAST:
		DeInit();
		if( !Init()) PostQuitMessage( 1 );
		
	  Case WM_DESTROY:				/* terminated by user					*/
		DeInit();
		PostQuitMessage( 1 );
		
	  Default:
		return( DefWindowProc( hWnd, Message, wParam, lParam ));
	}
	return( 0 );
}

/*** main procedure *********************************************************/

int WINAPI _tWinMain(
	HINSTANCE	hInst,
	HINSTANCE	hPrevInst,
	LPTSTR		lpCmdLine,
	int			iCmdShow ){
	
	/* for creating window */
	
	g_lpCmdLine = lpCmdLine;
	
	HWND		hWnd;
	MSG			Msg;
	WNDCLASS	wcl;
	
	/* close if already executed */
	if( hWnd = FindWindow( szWinClassName, NULL )){
		PostMessage( hWnd, WM_CLOSE, 0, 0 );
	}
	
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
	
#ifdef DEBUG
	ShowWindow( hWnd, iCmdShow );
#else
	ShowWindow( hWnd, SW_HIDE );
#endif
	UpdateWindow( hWnd );
	
	/* create the message loop */
	
	while( GetMessage( &Msg, NULL, 0, 0 )){
		TranslateMessage( &Msg );
		DispatchMessage( &Msg );
	}
	
	CloseWinRing0();
	
	return( 0 );
}

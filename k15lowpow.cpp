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
				throw CError( "WinRing0: unsupported platform\n" );
				break;
			case OLS_DLL_DRIVER_NOT_LOADED:
				throw CError( "WinRing0: driver not loaded\n" );
				break;
			case OLS_DLL_DRIVER_NOT_FOUND:
				throw CError( "WinRing0: driver not found\n" );
				break;
			case OLS_DLL_DRIVER_UNLOADED:
				throw CError( "WinRing0: driver unloaded\n" );
				break;
			case OLS_DLL_DRIVER_NOT_LOADED_ON_NETWORK:
				throw CError( "WinRing0: driver not loaded from network\n" );
				break;
		//	case OLS_DLL_UNKNOWN_ERROR:
		//		throw CError( "WinRing0: unknown error\n" );
		//		break;
			default:
				throw CError( "WinRing0: unknown error\n" );
		}
	}
	
	GetDriverVersion (&verMajor,&verMinor,&verRevision,&verRelease);
	
	if( !((verMajor>=1) && (verMinor>=2)))
		throw CError( "WinRing0: version 1.2 or later required\n" );
}

int CloseWinRing0(){
	DeinitializeOls();
	return true;
}

/*** Load config file *******************************************************/

#define BUF_SIZE	1024

bool GetUintConfig( const char *szBuf, const char *szName, UINT& val ){
	if( strncmp( szBuf, szName, strlen( szName ))) return false;
	val = atoi( szBuf + strlen( szName ));
	return true;
}

void LoadConfig( void ){
	std::unique_ptr<TCHAR []>	szFileName( new TCHAR[ _tcslen( g_lpCmdLine ) + 1 ]);
	std::unique_ptr<char []>	szBuf( new char[ BUF_SIZE ]);
	
	TCHAR	*szParam = g_lpCmdLine;
	FILE	*fp = NULL;
	
	UINT	uPState = 0;
	UINT	uPStateConfFlag = 0;
	UINT	uFreq, uVid;
	UINT	u;
	
	enum{
		PSF_FREQ	= 1 << 0,
		PSF_VID 	= 1 << 1,
		PSF_ALL 	= ( 1 << 2 ) - 1,
	};
	
	try{
		// ファイル名 get
		if( StrGetParam( szFileName.get(), &szParam ) == NULL ) throw CError( "Error: config file not specified" );
		
		// ファイルオープン
		if( _tfopen_s( &fp, szFileName.get(), _T( "r" ))) throw CError( "Error: can't open file" );
		
		while( fgets( szBuf.get(), BUF_SIZE - 1, fp )){
			if( GetUintConfig( szBuf.get(), "[Pstate_", uPState )){
				uPStateConfFlag = 0;
			}else if( GetUintConfig( szBuf.get(), "freq=", uFreq )){
				uPStateConfFlag |= PSF_FREQ;
			}else if( GetUintConfig( szBuf.get(), "vid=", uVid )){
				uPStateConfFlag |= PSF_VID;
			}else if( GetUintConfig( szBuf.get(), "TargetLoad=", u )){
				g_pCpu->SetTargetLoad( u );
			}else if( GetUintConfig( szBuf.get(), "FullpowerLoad=", u )){
				g_pCpu->SetFullpowLoad( u );
			}else{
				GetUintConfig( szBuf.get(), "Interval=", g_uTimerInterval );
			}
			
			if( uPStateConfFlag == PSF_ALL ){
				DebugMsgD( _T( "P%d: f:%d v:%d\n" ), uPState, uFreq, uVid );
				g_pCpu->WritePState( uPState, uFreq, uVid );
				uPStateConfFlag = 0;
			}
		}
		
	}catch( CError& err ){
		if( fp ) fclose( fp );
		throw err;
	}
	
	if( fp ) fclose( fp );
}

/*** H/W control ************************************************************/

#define ID_TIMER	0

bool Init( bool bRestart = false ){
	try{
		if( !bRestart ) InitWinRing0();
		if( !CProcessor::IsSupported()) throw CError( "Error: unsupported CPU" );
		
		g_pCpu = new CProcessor();
		g_pCpu->Init();
		LoadConfig();
		g_pCpu->PowerManagementInit();
		SetTimer( g_hWnd, ID_TIMER, g_uTimerInterval, NULL );
		
	}catch( CError& err ){
		std::unique_ptr<char []> szBuf( new char[ BUF_SIZE ]);
		
		MessageBoxA( NULL, err.what( szBuf.get(), BUF_SIZE ), "k15lowpow", MB_ICONSTOP | MB_OK );
		return false;
	}
	
	return true;
}

void DeInit( bool bRestart = false ){
	delete g_pCpu;
	if( !bRestart ) CloseWinRing0();
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
		if( wParam == PBT_APMRESUMEAUTOMATIC ){
			DeInit( true );
			if( !Init( true )) PostQuitMessage( 1 );
		}
		
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
		320, //CW_USEDEFAULT,	// w
		240, //CW_USEDEFAULT,	// h
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

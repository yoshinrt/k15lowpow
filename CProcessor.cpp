/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	CProcessor.cpp - CPU low level control
	
*****************************************************************************/

#include "StdAfx.h"
#include "CProcessor.h"

/*** コンストラクタ・デストラクタ *******************************************/

CProcessor::CProcessor(){
	m_dwPerfEventSelAddr	= 0xC0010200;
	m_dwPerfEventCntAddr	= 0xC0010201;
	m_dwPerfSlotOffs	= 2;
	m_uMaxPerfSlot		= 6;
	m_uMaxPState		= 8;
	
	m_pqwPerfCnt[ 0 ]	= NULL;
	m_pqwPerfCnt[ 1 ]	= NULL;
	m_puFreq			= NULL;
	m_puVid				= NULL;
	m_uPerfCntBuf		= 0;
	m_uPerfSlot			= -1;
	m_uPStateLimit		= 0;
	m_uDebug			= 0;
	
	#ifndef NO_SMOOTH_MODE
		m_uApmMode			= APM_SMOOTH;
	#endif
	
	SetTargetLoad( 70 );
	SetFullpowLoad( 90 );
}

CProcessor::~CProcessor(){
	// PStateLimit 解除
	SetPStateLimit( 0 );
	
	// HwP7 を初期値に戻す
	if( m_uApmMode == APM_SMOOTH ){
		WriteFreqAutoVID( m_puFreq[ m_uMaxPState - 1 ]);
	}
	
	// perf slot 解放
	if( m_uPerfSlot != -1 ){
		WriteMSR(
			m_dwPerfEventSelAddr + m_uPerfSlot * m_dwPerfSlotOffs,
			0
		);
	}
	
	delete [] m_pqwPerfCnt[ 0 ];
	delete [] m_pqwPerfCnt[ 1 ];
	delete [] m_puVid;
	delete [] m_puFreq;
}

/*** CPU check **************************************************************/

bool CProcessor::IsSupported( void ){
	
	DWORD eax, ebx, ecx, edx;
	
	// ベンダチェック
	if(
		CProcessor::Cpuid( CPUID_VENDOR, eax, ebx, ecx, edx ) != 0xD ||
		( ebx != 0x68747541 ) || ( ecx != 0x444D4163 ) || ( edx != 0x69746E65 )
	){
		return false;
	}
	
	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	CProcessor::Cpuid( CPUID_BRAND, eax, ebx, ecx, edx );
	
	DWORD familyBase = ( eax & 0xf00 ) >> 8;
	DWORD familyExtended = (( eax & 0xff00000 ) >> 20 ) + familyBase;
	
	if( familyExtended != 0x15 ) return false;
	
	DWORD model = ( eax & 0xf0 ) >> 4;
	DWORD modelExtended = (( eax & 0xf0000 ) >> 12 ) + model; /* family 15h: modelExtended is valid */
	if( modelExtended < 0x30 || 0x3F < modelExtended ) return false;
	
	return true;
}

/*** Perf cnt リード ********************************************************/

void CProcessor::ReadPerfCnt( void ){
	m_uPerfCntBuf ^= 1;
	
	for( UINT u = 0; u < m_uCoreNum; ++u ){
		m_pqwPerfCnt[ m_uPerfCntBuf ][ u ] =
			ReadMSR64p( u, m_dwPerfEventCntAddr + m_uPerfSlot * m_dwPerfSlotOffs );
	}
}

/*** FID/DID/VID ************************************************************/

UINT CProcessor::ReadFreq( UINT uPState ){
	UINT u = ( UINT )ReadMSR( 0xC0010064 + uPState );
	return GetFreqByFDID( GetBits( u, 0, 5 ), GetBits( u, 6, 2 ));
}

UINT CProcessor::ReadVID( UINT uPState ){
	UINT u = ( UINT )ReadMSR( 0xC0010064 + uPState );
	return GetBits( u, 9, 8 );
}

void CProcessor::GetFDIDByFreq( UINT uFreq, UINT& uFid, UINT& uDid ){
	
	uDid =	uFreq <= GetFreqByFDID( 0x1F, 4 ) ? 4 :
			uFreq <= GetFreqByFDID( 0x1F, 3 ) ? 3 :
			uFreq <= GetFreqByFDID( 0x1F, 2 ) ? 2 :
			uFreq <= GetFreqByFDID( 0x1F, 1 ) ? 1 : 0;
	
	uFid = ((( uFreq << uDid ) + 50/*四捨五入*/ ) / 100 - 0x10 ) & 0x1F;
}

UINT CProcessor::GetVIDByFreq( UINT uFreq ){
	UINT u;
	
	if( uFreq < m_puFreq[ m_uMaxPState - 1 ] || m_puFreq[ 0 ] < uFreq ){
		throw CError( "Internal error: Freq out of range" );
	}
	
	for( u = 0; u < m_uMaxPState; ++u ){
		// ズバリの Freq 発見
		if( uFreq == m_puFreq[ u ]) return m_puVid[ u ];
		
		// 等比分割で VID を求める
		if( uFreq > m_puFreq[ u ]){
			
			return
				( int )( m_puVid[ u ] - m_puVid[ u - 1 ]) *
				( m_puFreq[ u - 1 ] - uFreq ) /
				( m_puFreq[ u - 1 ] - m_puFreq[ u ]) +
				m_puVid[ u - 1 ];
		}
	}
	
	// ありえないけど念のため
	throw CError( "Internal error: Freq out of range" );
	return GetSwPStateVID( 0 );
}

/*** set sw pstate limit ****************************************************/

void CProcessor::SetPStateLimit( UINT uPState ){
	WritePCI(
		PCI_PSTATE_LIMIT, 0x68,
		(( uPState + m_uBoostStateNum ) << 28 ) | ( uPState ? ( 1 << 5 ) : 0 )
	);
}

/*** Intialize CPU info *****************************************************/

void CProcessor::Init( void ){
	// CPU node
	m_uNodeNum = GetBits( ReadPCI( PCI_HT_CONFIG, 0x60 ), 4, 3 ) + 1;
	DebugMsgD( _T( "Num of node: %d\n" ), m_uNodeNum );
	
	// CPU Core	
	
	m_uCoreNum = GetBits( Cpuid( 0x80000008, REG_ECX ), 0, 8 ) + 1;
	DebugMsgD( _T( "Num of core: %d\n" ), m_uCoreNum );
}

/*** setup P-State **********************************************************/

void CProcessor::WritePState( UINT uPState, UINT uFid, UINT uDid, UINT uVid ){
	DebugMsgD( _T( "f:%4d %.5fV  " ), GetFreqByFDID( uFid, uDid ), GetVCoreByVID( uVid ));
	
	QWORD dat = ReadMSR64( MSR_CORE_COF + uPState );
	
	// 電圧を上げる場合は，電圧を先に設定
	if( uVid < GetBits( dat, 9, 8 )){
		dat = SetBits( dat, 9, 8, ( QWORD )uVid );
		WriteMSR( MSR_CORE_COF + uPState, dat );
		Sleep( 10 );
		dat = SetBits( dat, 6, 2, ( QWORD )uDid );
		dat = SetBits( dat, 0, 5, ( QWORD )uFid );
	}else{
		dat = SetBits( dat, 6, 2, ( QWORD )uDid );
		dat = SetBits( dat, 0, 5, ( QWORD )uFid );
		WriteMSR( MSR_CORE_COF + uPState, dat );
		Sleep( 10 );
		dat = SetBits( dat, 9, 8, ( QWORD )uVid );
	}
	
	WriteMSR( MSR_CORE_COF + uPState, dat );
}

void CProcessor::WritePState( UINT uPState, UINT uFreq, UINT uVid ){
	UINT uFid, uDid;
	GetFDIDByFreq( uFreq, uFid, uDid );
	WritePState( uPState, uFid, uDid, uVid );
}

/*** power management *******************************************************/

void CProcessor::PowerManagementInit( void ){
	UINT u, v;
	
	/*** PState 値の保存 ****************************************************/
	
	// 全 PState の freq 取得
	m_puFreq	= new UINT[ m_uMaxPState ];
	m_puVid		= new UINT[ m_uMaxPState ];
	for( u = 0; u < m_uMaxPState; ++u ){
		m_puFreq[ u ]	= ReadFreq( u );
		m_puVid[ u ]	= ReadVID( u );
		DebugMsgD( _T( "Freq#%d:%d\n" ), u, m_puFreq[ u ]);
	}
	
	m_uBoostStateNum = GetBits( ReadPCI( PCI_BOOST_CTRL, 0x15C ), 2, 2 );
	DebugMsgD( _T( "Boost state:%d\n" ), m_uBoostStateNum );
	
	if( m_uApmMode == APM_PSTATE ){
		// m_puFreq を P0 / Pn * 1024 に変換
		UINT uP0Freq = GetSwPStateFreq( 0 );
		for( u = 0; u < m_uMaxPState; ++u ){
			m_puFreq[ u ] = uP0Freq * 1024 / m_puFreq[ u ];
		}
	}
	
	/*** performance counter 設定 *******************************************/
	
	#define PERF_CTRL_VAL ( \
		( 1LL << 22 ) | /* enable			*/ \
		( 3LL << 16 ) |	/* count os & user	*/ \
		0x76			/* !Idle Counter	*/ \
	)
	
	// Perf 空きスロット検索
	for( u = 0; u < m_uMaxPerfSlot; ++u ){
		for( v = 0; v < m_uCoreNum; ++v ){
			
			QWORD val = ReadMSRp( v, m_dwPerfEventSelAddr + u * m_dwPerfSlotOffs );
			
			// 使用されていたら break
			// または もともと PERF 計測している設定値でなければ break
			if( val & ( 1 << 22 ) && val != PERF_CTRL_VAL ) break;
		}
		
		// 全 CPU 分開いていたら break
		if( v == m_uCoreNum ) break;
	}
	
	if( u == m_uMaxPerfSlot ) throw CError( "No free performance counter slot" );
	
	m_uPerfSlot = u;
	DebugMsgD( _T( "Free perf slot #%d\n" ), m_uPerfSlot );
	
	// Perf 設定
	for( u = 0; u < m_uCoreNum; ++u ){
		WriteMSRp(
			u,
			m_dwPerfEventSelAddr + m_uPerfSlot * m_dwPerfSlotOffs,
			PERF_CTRL_VAL
		);
	}
	
	/************************************************************************/
	
	if( m_uApmMode == APM_SMOOTH ){
		SetPStateLimit( m_uMaxPState - 1 );				// HwP7
		m_uCurrentFreq = m_puFreq[ m_uMaxPState - 1 ];	// SwP7
	}
	
	// 領域確保
	m_pqwPerfCnt[ 0 ]	= new QWORD[ m_uCoreNum ];
	m_pqwPerfCnt[ 1 ]	= new QWORD[ m_uCoreNum ];
	
	// perf cnt 初期リード
	ReadPerfCnt();
	m_qwPrevTsc = __rdtsc();
}

void CProcessor::WriteFreqAutoVID( UINT uFreq ){
	
	if( uFreq > m_puFreq[ m_uBoostStateNum ]){
		uFreq = m_puFreq[ m_uBoostStateNum ];
	}else if( uFreq < m_puFreq[ m_uMaxPState - 1 ]){
		uFreq = m_puFreq[ m_uMaxPState - 1 ];
	}
	
	UINT uFid, uDid;
	GetFDIDByFreq( uFreq, uFid, uDid );
	UINT uActFreq	= GetFreqByFDID( uFid, uDid );
	UINT uVid		= GetVIDByFreq( uActFreq );
	
	//SetPStateLimit( m_uMaxPState - 1 );
	WritePState( m_uMaxPState - 1, uFid, uDid, uVid );	// HwP7
	//SetPStateLimit( m_uMaxPState - 2 );
	
	m_uCurrentFreq = uActFreq;
}

void CProcessor::PowerManagement( void ){
	QWORD	qwTsc;
	QWORD	qwTime;
	UINT	uMaxLoad = 0;
	UINT	u;
	UINT	uCurState = m_uPStateLimit;
	
	ReadPerfCnt();
	qwTsc = __rdtsc();
	qwTime = qwTsc - m_qwPrevTsc;
	
	// 最大の CPU load を求める
	for( UINT u = 0; u < m_uCoreNum; ++u ){
		UINT uLoad = ( UINT )((
			GetPerfCnt( u ) - GetPrevPerfCnt( u )
		) * 1024 / qwTime );
		
		if( uLoad > uMaxLoad ) uMaxLoad = uLoad;
		
		//DebugMsgD( _T( "core%d:%d%%  " ), u, uLoad );
		//DebugMsgD( _T( "core%d: %016llx %016llx  " ), u, m_pqwPerfCnt[ m_uPerfCntBuf     ][ u ], m_pqwPerfCnt[ m_uPerfCntBuf ^ 1 ][ u ] );
	}
	
	//DebugMsgD( _T( "\n" ));
	m_qwPrevTsc = qwTsc;
	
	if( m_uApmMode == APM_PSTATE ){
		// 現在の PStateLimit での実 load を求める
		UINT uActualLoad = uMaxLoad * GetSwPStateFreq( m_uPStateLimit ) / 1024;
		
		// 95% load で P0 へ
		if( uActualLoad > m_uFullpowLoad ){
			m_uPStateLimit = 0;
		}
		
		// 80% を超えない PState を求める
		else{
			for( u = m_uMaxPState - 1; u > m_uBoostStateNum; --u ){
				if( uMaxLoad * m_puFreq[ u ] < ( m_uTargetLoad << 10 )) break;
			}
			m_uPStateLimit = u - m_uBoostStateNum;
		}
		
		if( GetDebug()){
			wprintf( _T( "\n%d%c%3u" ), uCurState,
				uCurState == m_uPStateLimit ? ' ' :
				uCurState >  m_uPStateLimit ? '+' : '-',
				( UINT )( uActualLoad / 10.24 + 0.5 )
			);
			
			#define GRAPH_RANGE	50
			
			UINT uLoad = ( UINT )( uActualLoad / 10.24 + 0.5 ) / ( 100 / GRAPH_RANGE );
			
			for( UINT u = 0; u <= GRAPH_RANGE; ++u ){
				putchar(
					u == 0 || u == GRAPH_RANGE		? '|' :
					u <= uLoad						? '*' :
					u % ( GRAPH_RANGE / 10 ) == 0	? ( '0' + u / ( GRAPH_RANGE / 10 )) : '.'
				);
			}
		}
		
		DebugMsgD(
			_T( "NewP:%d Load:%5.1f Act:%5.1f Tgt:%5.1f\n" ),
			m_uPStateLimit,
			uMaxLoad / 10.24, uActualLoad / 10.24,
			uMaxLoad * GetSwPStateFreq( m_uPStateLimit ) / ( 1024 * 10.24 )
		);
		
		SetPStateLimit( m_uPStateLimit );
		
	}else if( m_uApmMode == APM_SMOOTH ){
		// 現在の Freq での実 load を求める
		UINT uActualLoad = uMaxLoad * GetSwPStateFreq( 0 ) / m_uCurrentFreq;
		
		// 95% load で SwP0 へ
		if( uActualLoad > m_uFullpowLoad ){
			WriteFreqAutoVID( GetSwPStateFreq( 0 ));
		}else{
			WriteFreqAutoVID( GetSwPStateFreq( 0 ) * uMaxLoad / m_uTargetLoad );
		}
		DebugMsgD(
			_T( "Load:%5.1f Act:%5.1f\n" ),
			uMaxLoad / 10.24, uActualLoad / 10.24
		);
	}
}

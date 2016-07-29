/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	CProcessor.cpp - CPU low level control
	
*****************************************************************************/

#define	CPUID_VENDOR	0x00000000
#define	CPUID_BRAND		0x00000001
#define MSR_CORE_COF	0xC0010064

#define PCI_ADDR( dev, func )	(( dev << 3 ) | func )
#define PCI_HT_CONFIG			PCI_ADDR( 0x18, 0 )
#define PCI_PSTATE_LIMIT		PCI_ADDR( 0x18, 3 )
#define PCI_BOOST_CTRL			PCI_ADDR( 0x18, 4 )

#define MSG_R_CPUID		"Error: read CPUID(0x%08X)"
#define MSG_R_MSR		"Error: read MSR(0x%08X)"
#define MSG_W_MSR		"Error: write MSR(0x%08X)"
#define MSG_R_PCI		"Error: read PCI(D%X F%X Reg%X)"
#define MSG_W_PCI		"Error: write PCI(D%X F%X Reg%X)"

class CError : public std::logic_error {
  public:
	CError( const char *message ) : std::logic_error( message ){};
	
	virtual const char *what( char *szBuf, size_t size ){
		return std::logic_error::what();
	}
};

class CErrorHw : public CError {
  public:
	CErrorHw( const char *message, uint32_t uData ) :
		CError( message ), m_uData( uData ){};
	
	const char *what( char *szBuf, size_t size ){
		sprintf_s( szBuf, size, std::logic_error::what(), m_uData );
		return szBuf;
	}
	
  private:
	uint32_t	m_uData;
};

class CErrorPCI : public CError {
  public:
	CErrorPCI( const char *message, uint32_t uData1, uint32_t uData2 ) :
		CError( message ), m_uData1( uData1 ), m_uData2( uData2 ){};
	
	const char *what( char *szBuf, size_t size ){
		sprintf_s(
			szBuf, size, std::logic_error::what(),
			m_uData1 >> 3, m_uData1 & 0x7, m_uData2
		);
		return szBuf;
	}
	
  private:
	uint32_t	m_uData1;
	uint32_t	m_uData2;
};

class CProcessor {
  public:
	CProcessor();
	~CProcessor();
	
	void Init( void );
	void ReadPerfCnt( void );
	UINT GetFreq( UINT uPState );
	void SetPStateLimit( UINT uPState );
	void PowerManagementInit( void );
	void PowerManagement( void );
	static bool IsSupported( void );
	
	QWORD GetPerfCnt( UINT uPState ){
		return m_pqwPerfCnt[ m_uPerfCntBuf ][ uPState ];
	}
	
	QWORD GetPrevPerfCnt( UINT uPState ){
		return m_pqwPerfCnt[ m_uPerfCntBuf ^ 1 ][ uPState ];
	}
	
	void SetupPState( UINT uPState, UINT uFid, UINT uDid, UINT uVid );
	
	/*** utils **************************************************************/
	
	template <class T>
	static T GetBits( T val, UINT pos, UINT width ){
		return ( val >> pos ) & (( 1 << width ) - 1 );
	}
	
	template <class T>
	static T SetBits( T val, UINT pos, UINT width, T newval ){
		T mask = ((( T )1 << width ) - 1 ) << pos;
		return val & ~mask | ( newval << pos ) & mask;
	}
	
	/*** WinRing0 wrapper ***************************************************/
	
	enum {
		REG_EAX,
		REG_EBX,
		REG_ECX,
		REG_EDX,
	};
	
	static DWORD Cpuid(
		DWORD index,					// CPUID index
		DWORD& eax,
		DWORD& ebx,
		DWORD& ecx,
		DWORD& edx
	){
		if( !::Cpuid( index, &eax, &ebx, &ecx, &edx )){
			throw( CErrorHw( MSG_R_CPUID, index ));
		}
		return eax;
	}
	
	static DWORD Cpuid(
		DWORD index,						// CPUID index
		UINT	uReg = REG_EAX
	){
		DWORD dwReg[ 4 ];
		if( !::Cpuid( index, &dwReg[ 0 ], &dwReg[ 1 ], &dwReg[ 2 ], &dwReg[ 3 ])){
			throw( CErrorHw( MSG_R_CPUID, index ));
		}
		return dwReg[ uReg ];
	}
	
	static DWORD ReadPCI(
		DWORD pciAddress,	// PCI Device Address
		DWORD regAddress	// Configuration Address 0-whatever
	){
		DWORD val;
		
		if( !::ReadPciConfigDwordEx( pciAddress, regAddress, &val )){
			throw( CErrorPCI( MSG_R_PCI, pciAddress, regAddress ));
		}
		
		return val;
	}
	
	static void WritePCI(
		DWORD pciAddress,	// PCI Device Address
		DWORD regAddress,	// Configuration Address 0-whatever
		DWORD val
	){
		if( !::WritePciConfigDwordEx( pciAddress, regAddress, val )){
			throw( CErrorPCI( MSG_W_PCI, pciAddress, regAddress ));
		}
	}
	
	static DWORD ReadMSR(
		DWORD	index,
		UINT	uReg = REG_EAX
	){
		DWORD eax, edx;
		if( !::Rdmsr( index, &eax, &edx )){
			throw( CErrorHw( MSG_R_MSR, index ));
		}
		return uReg == REG_EAX ? eax : edx;
	}
	
	static DWORD ReadMSRp(
		UINT	uCoreId,
		DWORD	index,
		UINT	uReg = REG_EAX
	){
		DWORD eax, edx;
		if( !::RdmsrPx( index, &eax, &edx, ( DWORD_PTR )1 << uCoreId )){
			throw( CErrorHw( MSG_R_MSR, index ));
		}
		return uReg == REG_EAX ? eax : edx;
	}
	
	static QWORD ReadMSR64(
		DWORD	index
	){
		QWORD val;
		if( !::Rdmsr( index, (( DWORD *)&val ), (( DWORD *)&val ) + 1 )){
			throw( CErrorHw( MSG_R_MSR, index ));
		}
		return val;
	}
	
	static QWORD ReadMSR64p(
		UINT	uCoreId,
		DWORD	index
	){
		QWORD val;
		if( !::RdmsrPx( index, (( DWORD *)&val ), (( DWORD *)&val ) + 1, ( DWORD_PTR )1 << uCoreId )){
			throw( CErrorHw( MSG_R_MSR, index ));
		}
		return val;
	}
	
	static void WriteMSR(
		DWORD index,					// MSR index
		QWORD val
	){
		if( !::Wrmsr( index, ( DWORD )val, ( DWORD )( val >> 32 ))){
			throw( CErrorHw( MSG_W_MSR, index ));
		}
	}
	
	static void WriteMSRp(
		UINT uCoreId,
		DWORD index,					// MSR index
		QWORD val
	){
		if( !::WrmsrPx( index, ( DWORD )val, ( DWORD )( val >> 32 ), ( DWORD_PTR )1 << uCoreId )){
			throw( CErrorHw( MSG_W_MSR, index ));
		}
	}
	
  private:
	QWORD	m_qwPrevTsc;
	QWORD	*m_pqwPerfCnt[ 2 ];
	UINT	*m_puFreq;
	
	DWORD	m_dwPerfEventSelAddr;
	DWORD	m_dwPerfEventCntAddr;
	DWORD	m_dwPerfSlotOffs;
	UINT	m_uMaxPerfSlot;
	UINT	m_uMaxPState;
	UINT	m_uBoostStateNum;
	
	UINT	m_uPerfCntBuf;
	UINT	m_uPerfSlot;
	UINT	m_uPStateLimit;
	UINT	m_uNodeNum;
	UINT	m_uCoreNum;
};

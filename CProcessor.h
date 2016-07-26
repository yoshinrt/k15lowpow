/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	CProcessor.cpp - CPU low level control
	
*****************************************************************************/

#define	CPUID_VENDOR	0x00000000
#define	CPUID_BRAND		0x00000001

#define PCI_ADDR( dev, func )	(( dev << 3 ) | func )
#define PCI_HT_CONFIG	PCI_ADDR( 0x18, 0 )

class CInvalidHWAccess : protected std::logic_error {
  public:
	CInvalidHWAccess( const char *message, uint64_t uData ) :
		std::logic_error( message ), m_uData( uData ){};
	
  private:
	uint64_t	m_uData;
};

class CProcessor {
  public:
	static bool IsSupported( void );
	
	void Init( void );
	
	/*** const **************************************************************/
	
	//Family 15h Performance Registers
	virtual DWORD BASE_PESR_REG_15	( void ){ return 0xC0010200; }
	virtual DWORD BASE_PERC_REG_15	( void ){ return 0xC0010201; }
	virtual DWORD PERC_REG_OFFS		( void ){ return 0xC0010201; }
	virtual UINT MAX_PERF_SLOT		( void ){ return 6; }
	virtual UINT MAX_PSTATE			( void ){ return 8; }
	
	/*** utils **************************************************************/
	
	template <class T>
	static T GetBits( T val, UINT pos, UINT width ){
		return ( val >> pos ) & (( 1 << width ) - 1 );
	}
	
	template <class T>
	static T SetBits( T val, UINT pos, UINT width, T newval ){
		T mask = (( 1 << width ) - 1 ) << pos;
		return val & ~mask | newval & mask;
	}
	
	/*** WinRing0 wrapper ***************************************************/
	
	enum {
		CPUID_EAX,
		CPUID_EBX,
		CPUID_ECX,
		CPUID_EDX,
	};
	
	DWORD Cpuid(
		DWORD index,					// CPUID index
		PDWORD eax,
		PDWORD ebx,
		PDWORD ecx,
		PDWORD edx
	){
		if( !::Cpuid( index, eax, ebx, ecx, edx )){
			throw( CInvalidHWAccess( "CPUID", index ));
		}
		
		return *eax;
	}
	
	DWORD Cpuid(
		DWORD index,					// CPUID index
		UINT uReg
	){
		DWORD eax, ebx, ecx, edx;
		if( !::Cpuid( index, &eax, &ebx, &ecx, &edx )){
			throw( CInvalidHWAccess( "CPUID", index ));
		}
		
		return
			uReg == CPUID_EAX ? eax :
			uReg == CPUID_EBX ? ebx :
			uReg == CPUID_ECX ? ecx : edx;
	}
	
	DWORD ReadPCI(
		DWORD pciAddress,	// PCI Device Address
		DWORD regAddress	// Configuration Address 0-whatever
	){
		DWORD val;
		
		if( !::ReadPciConfigDwordEx( pciAddress, regAddress, &val )){
			throw( CInvalidHWAccess( "PCI", (( uint64_t )pciAddress << 32 | regAddress )));
		}
		
		return val;
	}
	
  private:
	UINT	m_uNodeNum;
	UINT	m_uCoreNum;
};

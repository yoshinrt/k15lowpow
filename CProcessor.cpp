/*****************************************************************************
	
	k15lowpow -- K15 low power control
	
	CProcessor.cpp - CPU low level control
	
*****************************************************************************/

#include "StdAfx.h"
#include "CProcessor.h"

/*** CPU check **************************************************************/

bool IsSupported( void ){
	
	DWORD eax, ebx, ecx, edx;
	
	// ベンダチェック
	if(
		Cpuid( CPUID_VENDOR, &eax, &ebx, &ecx, &edx ) != 0xD ||
		( ebx != 0x68747541 ) || ( ecx != 0x444D4163 ) || ( edx != 0x69746E65 )
	){
		return false;
	}
	
	//Check extended CpuID Information - CPUID Function 0000_0001 reg EAX
	if( Cpuid( CPUID_BRAND, &eax, &ebx, &ecx, &edx ) != 0x1 ){
		return false;
	}
	
	DWORD familyBase = ( eax & 0xf00 ) >> 8;
	DWORD familyExtended = (( eax & 0xff00000 ) >> 20 ) + familyBase;
	
	if( familyExtended != 0x15 ) return false;
	
	DWORD model = ( eax & 0xf0 ) >> 4;
	DWORD modelExtended = (( eax & 0xf0000 ) >> 12 ) + model; /* family 15h: modelExtended is valid */
	if( modelExtended < 0x30 || 0x3F < modelExtended ) return false;
	
	return true;
}

/*** Intialize CPU info *****************************************************/

void CProcessor::Init( void ){
	// CPU node
	m_uNodeNum = GetBits( ReadPCI( PCI_HT_CONFIG, 0x60 ), 4, 3 ) + 1;
	DebugMsgD( _T( "Num of node: %d\n" ), m_uNodeNum );
	
	// CPU Core
	m_uCoreNum = GetBits( Cpuid( 0x80000008, CPUID_ECX ), 0, 8 ) + 1;
	DebugMsgD( _T( "Num of core: %d\n" ), m_uCoreNum );
}

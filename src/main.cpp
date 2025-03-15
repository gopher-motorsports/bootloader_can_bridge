/**
  ******************************************************************************
  * @file    main_example.cpp
  * @author  MCD Application Team
  * @brief   This module is an example, it can be integrated to a C++ console project
  *          to do a basic USB connection and  GPIO bridge test with the
  *          STLINK-V3SET probe, using the Bridge C++ open API.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
#include "platform_include.h"
#if defined(_MSC_VER) &&  (_MSC_VER >= 1000)
// no additional include needed
#else
#include <cstdlib>
#endif
#include <stdio.h>
#include "bridge.h"
#include <tchar.h>

#define TEST_BUF_SIZE 3000

class cBrgExample
{
public:
	cBrgExample();
	~cBrgExample();

	Brg_StatusT SelectSTLink(STLinkInterface *pStlinkIf, int *pFirstDevNotInUse);
	Brg_StatusT Connect(Brg* pBrg, int deviceNb);
	void Disconnect(void);

    Brg_StatusT SendCanBootloaderStart(int moduleId);
    Brg_StatusT CanInit(void);

	// CAN
	Brg_StatusT CanTest(void);
	Brg_StatusT CanTestInit(void);
	Brg_StatusT CanTestLoopback(void);
	Brg_StatusT CanFilterDisable(Brg_CanFilterConfT* pFilterConf, uint8_t filterNb, Brg_CanMsgIdT filterIde);
	Brg_StatusT CanMsgTxRxVerif(Brg_CanTxMsgT *pCanTxMsg, uint8_t *pDataTx, Brg_CanRxMsgT *pCanRxMsg, uint8_t *pDataRx, Brg_CanRxFifoT rxFifo, uint8_t size);

protected:
private:
	Brg* m_pBrg;
	char m_serialNumber[SERIAL_NUM_STR_MAX_LEN];
};

cBrgExample::cBrgExample() : m_pBrg(NULL)
{
	for (int i=0; i<SERIAL_NUM_STR_MAX_LEN; i++) {
		m_serialNumber[i] = 0;
	}
}

cBrgExample::~cBrgExample()
{
}

/*****************************************************************************/
// STLINK USB management
/*****************************************************************************/
Brg_StatusT cBrgExample::SelectSTLink(STLinkInterface* pStlinkIf, int* pFirstDevNotInUse)
{
	uint32_t i, numDevices;
	TDeviceInfo2 devInfo2;
	STLinkIf_StatusT ifStatus = STLINKIF_NO_ERR;
	STLink_EnumStlinkInterfaceT stlinkIfId;

	if ((pStlinkIf == NULL) || (pFirstDevNotInUse == NULL)) {
		printf("Internal parameter error in SelectSTLink\n");
		return BRG_PARAM_ERR;
	}
	stlinkIfId = pStlinkIf->GetIfId();
	if (stlinkIfId != STLINK_BRIDGE) {
		printf("Wrong interface in SelectSTLink\n");
		return BRG_PARAM_ERR;
	}

	ifStatus = pStlinkIf->EnumDevices(&numDevices, FALSE);
	// Choose the first STLink Bridge available
	if ((ifStatus == STLINKIF_NO_ERR) || (ifStatus == STLINKIF_PERMISSION_ERR)) {
		printf("%d BRIDGE device found\n", (int)numDevices);

		for( i=0; i<numDevices; i++ ) {
			ifStatus = pStlinkIf->GetDeviceInfo2(i, &devInfo2, sizeof(devInfo2));
			printf("Bridge %d PID: 0X%04hx SN:%s\n", (int)i, (unsigned short)devInfo2.ProductId, devInfo2.EnumUniqueId);

			if( (*pFirstDevNotInUse==-1) && (devInfo2.DeviceUsed == false) ) {
				*pFirstDevNotInUse = i;
				memcpy(m_serialNumber, &devInfo2.EnumUniqueId, SERIAL_NUM_STR_MAX_LEN);
				printf("SELECTED BRIDGE Stlink SN:%s\n\n", m_serialNumber);
			}
		}
	} else if (ifStatus == STLINKIF_CONNECT_ERR) {
		printf("No STLink BRIDGE device detected\n");
	} else {
		printf("Enum error (status = %d)\n", ifStatus);
		if (ifStatus == STLINKIF_NO_STLINK) {
			printf("No BRIDGE STLink available\n");
		}
	}

	return Brg::ConvSTLinkIfToBrgStatus(ifStatus);
}

Brg_StatusT cBrgExample::Connect(Brg* pBrg, int deviceNb)
{
	// The firmware may not be the very last one, but it may be OK like that (just inform)
	bool bOldFirmwareWarning=false;
	Brg_StatusT brgStat = BRG_NO_ERR;
	if (pBrg == NULL) {
		return BRG_CONNECT_ERR;
	}
	m_pBrg = pBrg;
	// Open the STLink connection
	if (brgStat == BRG_NO_ERR) {
		m_pBrg->SetOpenModeExclusive(true);

		brgStat = m_pBrg->OpenStlink(deviceNb);

		if (brgStat == BRG_NOT_SUPPORTED) {
			printf("BRIDGE not supported SN:%s\n", m_serialNumber);
		}
		if (brgStat == BRG_OLD_FIRMWARE_WARNING) {
			// Status to restore at the end if all is OK
			bOldFirmwareWarning = true;
			brgStat = BRG_NO_ERR;
		}
	}

	// // Test Voltage command
	// if (brgStat == BRG_NO_ERR) {
	// 	float voltage = 0;
	// 	// T_VCC pin must be connected to target voltage on bridge or debug connector
	// 	// T_VCC input is mandatory for STLink using levelshifter (STLINK-V3PWR or STLINK-V3SET+B-STLINK-VOLT/ISOL),
	// 	// else bridge signals are all 0
	// 	brgStat = m_pBrg->GetTargetVoltage(&voltage);
	// 	if (brgStat != BRG_NO_ERR) {
	// 		printf("BRIDGE get voltage error \n");
	// 	} else if (voltage < 1) {
	// 		printf("BRIDGE get voltage WARNING: %fV < 1V, check if T_VCC pin is connected to target voltage on bridge (or debug) connector \n", (double)voltage);
	// 	} else {
	// 		printf("BRIDGE get voltage: %f V \n", (double)voltage);
	// 	}
	// }

	if ((brgStat == BRG_NO_ERR) && (bOldFirmwareWarning == true)) {
		// brgStat = BRG_OLD_FIRMWARE_WARNING;
		printf("BRG_OLD_FIRMWARE_WARNING: v%d B%d \n",(int)m_pBrg->m_Version.Major_Ver, (int)m_pBrg->m_Version.Bridge_Ver);
	}

	return brgStat;

}

void cBrgExample::Disconnect(void)
{
	if (m_pBrg != NULL) {
		//robustness in case not already done
		m_pBrg->CloseBridge(COM_UNDEF_ALL);
		// break link to current STLink BRIDGE
		m_pBrg = NULL;
	}
}

/*****************************************************************************/
// Test CAN commands
/*****************************************************************************/

Brg_StatusT cBrgExample::SendCanBootloaderStart(int moduleId)
{
    Brg_StatusT brgStat = BRG_NO_ERR;
	uint8_t bridgeCom = COM_CAN;

	if (m_pBrg == NULL) {
		return BRG_CONNECT_ERR;
	}

    brgStat = CanInit();
	if( brgStat != BRG_NO_ERR ) {
		printf("CAN init error \n");
	}

	Brg_CanTxMsgT canTxMsg;
    uint8_t dataTx[8];

    canTxMsg.ID = 0x123;
    canTxMsg.IDE = CAN_ID_STANDARD;
    canTxMsg.RTR = CAN_DATA_FRAME;
    canTxMsg.DLC = 0;

    for(int32_t i = 0; i < 7; i++)
    {
        dataTx[i] = 0xFF;
    }

    dataTx[7] = moduleId;

    if( brgStat == BRG_NO_ERR ) {
        printf("Starting GCAN Bootloader on target with module ID: %d\n", moduleId);
		brgStat = m_pBrg->WriteMsgCAN(&canTxMsg, dataTx, 8);

		if( brgStat != BRG_NO_ERR ) {
			printf("CAN Write Message error\n");
		}
	}

    // Close Bridge CAN COM, even in case of error
	m_pBrg->CloseBridge(COM_CAN);

    return brgStat;
}

Brg_StatusT cBrgExample::CanInit(void)
{
    Brg_StatusT brgStat = BRG_NO_ERR;
	uint32_t prescal;
	uint32_t reqBaudrate = 1000000; //1 Mbaud
	uint32_t finalBaudrate = 0;
	Brg_CanInitT canParam;

	// Set baudrate to 1 Mbaud
	canParam.BitTimeConf.PropSegInTq = 1;
	canParam.BitTimeConf.PhaseSeg1InTq = 5;
	canParam.BitTimeConf.PhaseSeg2InTq = 1;
	canParam.BitTimeConf.SjwInTq = 4; //min (4, PhaseSeg1InTq)

	brgStat = m_pBrg->GetCANbaudratePrescal(&canParam.BitTimeConf, reqBaudrate, (uint32_t*)&prescal, (uint32_t*)&finalBaudrate);
	if( brgStat == BRG_COM_FREQ_MODIFIED ) {
		brgStat = BRG_NO_ERR;
		printf("WARNING Bridge CAN init baudrate asked %d bps but applied %d bps \n", (int)reqBaudrate, (int)finalBaudrate);
	}

	if( brgStat == BRG_NO_ERR ) {
		canParam.Prescaler = prescal;
		// canParam.BitTimeConf already filled above
		canParam.Mode = CAN_MODE_LOOPBACK; // CAN_MODE_LOOPBACK (for internal CAN STLINK test) CAN_MODE_NORMAL for loopback with real target
		canParam.bIsTxfpEn = false;
		canParam.bIsRflmEn = false;
		canParam.bIsNartEn = false;
		canParam.bIsAwumEn = false;
		canParam.bIsAbomEn = false;
		brgStat = m_pBrg->InitCAN(&canParam, BRG_INIT_FULL);
	} else if( brgStat == BRG_COM_FREQ_NOT_SUPPORTED ) {
		printf("ERROR Bridge CAN init baudrate %d bps not possible (invalid prescaler: %d) change Bit Time or baudrate settings. \n", (int)reqBaudrate, (int)prescal);
	}

    printf("CAN bridge baudrate set to %d bps \n", (int)finalBaudrate);
	return brgStat;
}

Brg_StatusT cBrgExample::CanTest(void)
{
	Brg_StatusT brgStat = BRG_NO_ERR;
	uint8_t bridgeCom = COM_CAN;

	if (m_pBrg == NULL) {
		return BRG_CONNECT_ERR;
	}

	printf("Run BRIDGE CAN test\n");
	brgStat = CanTestInit();
	if( brgStat != BRG_NO_ERR ) {
		printf("CAN init error \n");
	}
	if (brgStat == BRG_NO_ERR) {
		brgStat = CanTestLoopback();
	} else if ((brgStat == BRG_CAN_ERR) && (m_pBrg->IsCanFilter16Support() == true)) {
		// STLINK-V3SET case: loopback requires an external CAN bus
		printf("CAN Loopback test Skipped\n STLINK-V3SET requires to be connected to a CAN bus (e.g.: V3SET ADAPTER board with CAN on) \n");
		brgStat = BRG_NO_ERR;
	}

	// Close Bridge CAN COM, even in case of error
	m_pBrg->CloseBridge(COM_CAN);

	if( brgStat == BRG_NO_ERR ) {
		printf("CAN Test OK \n");
	}

	return brgStat;
}

// Test CAN commands Brg::InitCAN Brg::GetCANbaudratePrescal
Brg_StatusT cBrgExample::CanTestInit(void)
{
	Brg_StatusT brgStat = BRG_NO_ERR;
	uint32_t prescal;
	uint32_t reqBaudrate = 125000; //125kbps
	uint32_t finalBaudrate = 0;
	Brg_CanInitT canParam;

	// Set baudrate to 125kbps
	// with default CAN bit Time: PropSegInTq = 1, PhaseSeg1InTq = 7, PhaseSeg2InTq = 7 SjwInTq=4
	// N=sync+prop+seg1+seg2= 1+1+7+7= 16, 125000 bps
	// -> prescal = 24 = (CanClk = 48MHz)/(16*125000) for STLINK-V3SET
	// -> prescal = 40 = (CanClk = 80MHz)/(16*125000) for STLINK-V3PWR
	canParam.BitTimeConf.PropSegInTq = 1;
	canParam.BitTimeConf.PhaseSeg1InTq = 7;
	canParam.BitTimeConf.PhaseSeg2InTq = 7;
	canParam.BitTimeConf.SjwInTq = 4; //min (4, PhaseSeg1InTq)

	brgStat = m_pBrg->GetCANbaudratePrescal(&canParam.BitTimeConf, reqBaudrate, (uint32_t*)&prescal, (uint32_t*)&finalBaudrate);
	if( brgStat == BRG_COM_FREQ_MODIFIED ) {
		brgStat = BRG_NO_ERR;
		printf("WARNING Bridge CAN init baudrate asked %d bps but applied %d bps \n", (int)reqBaudrate, (int)finalBaudrate);
	}

	if( brgStat == BRG_NO_ERR ) {
		canParam.Prescaler = prescal;
		// canParam.BitTimeConf already filled above
		canParam.Mode = CAN_MODE_LOOPBACK; // CAN_MODE_LOOPBACK (for internal CAN STLINK test) CAN_MODE_NORMAL for loopback with real target
		canParam.bIsTxfpEn = false;
		canParam.bIsRflmEn = false;
		canParam.bIsNartEn = false;
		canParam.bIsAwumEn = false;
		canParam.bIsAbomEn = false;
		brgStat = m_pBrg->InitCAN(&canParam, BRG_INIT_FULL);
	} else if( brgStat == BRG_COM_FREQ_NOT_SUPPORTED ) {
		printf("ERROR Bridge CAN init baudrate %d bps not possible (invalid prescaler: %d) change Bit Time or baudrate settings. \n", (int)reqBaudrate, (int)prescal);
	}
	return brgStat;
}

// Test CAN commands Brg::StartMsgReceptionCAN Brg::InitFilterCAN
Brg_StatusT cBrgExample::CanTestLoopback(void)
{
	Brg_StatusT brgStat = BRG_NO_ERR;
	uint8_t dataRx[8], dataTx[8]; // Classic CAN: DLC limited to 8
	int i, nb;
	Brg_CanFilterConfT filterConf;
	Brg_CanRxMsgT canRxMsg;
	Brg_CanTxMsgT canTxMsg;
	uint8_t size=0;
	uint8_t maxMsgSize = 8;

	brgStat =  m_pBrg->StartMsgReceptionCAN();
	if (brgStat != BRG_NO_ERR) {
		printf("CAN StartMsgReceptionCAN failed \n");
	}

	// robustess test
	if (brgStat == BRG_NO_ERR) {
		int maxLoop;
		maxLoop = 500;

		// Receive messages with specific ID with all Classic Standard CAN DLC possible size (0->8)
		// Filter0: CAN prepare receive (no filter: ID_MASK with Id =0 & Mask = 0) receive all in FIFO0
		filterConf.AssignedFifo = CAN_MSG_RX_FIFO0;
		filterConf.bIsFilterEn = true;
		filterConf.FilterBankNb = 0; //0 to 13
		filterConf.FilterMode = CAN_FILTER_ID_MASK;
		filterConf.FilterScale = CAN_FILTER_32BIT; // note: STLINK-V3PWR does not support CAN_FILTER_16BIT
		for( i = 0; i<4; i++) {
			filterConf.Id[i].ID = 0;
			filterConf.Id[i].IDE = CAN_ID_STANDARD;//CAN_ID_EXTENDED;
			filterConf.Id[i].RTR = CAN_DATA_FRAME;
		}
		for( i = 0; i<2; i++) {
			filterConf.Mask[i].ID = 0;
			filterConf.Mask[i].IDE = CAN_ID_STANDARD;//CAN_ID_EXTENDED;
			filterConf.Mask[i].RTR = CAN_DATA_FRAME;
		}

		brgStat = m_pBrg->InitFilterCAN(&filterConf);
		if (brgStat != BRG_NO_ERR) {
			printf("CAN filter0 init failed \n");
		}

		//Init Rx / Tx msg
		canRxMsg.ID = 0;
		canRxMsg.IDE = CAN_ID_EXTENDED; // must be = canTxMsg.IDE for the test
		canRxMsg.RTR = CAN_DATA_FRAME; // must be = canTxMsg.RTR for the test
		canRxMsg.DLC = 0;

		canTxMsg.ID = 0x678;//0x12345678; // must be <=0x7FF for CAN_ID_STANDARD, <=0x1FFFFFFF
		canTxMsg.IDE = CAN_ID_STANDARD;//CAN_ID_EXTENDED;, for STLINK-V3PWR must be the same as filter configuration
		canTxMsg.RTR = CAN_DATA_FRAME;
		canTxMsg.DLC = 0;

		nb = 0;
		while ( (brgStat == BRG_NO_ERR) && (nb<maxLoop) ) {
			for (i=0; i<maxMsgSize; i++) {
				dataRx[i] = 0;
				dataTx[i] = (uint8_t)(nb+i);
			}
			canRxMsg.DLC = 0;
			canTxMsg.DLC = 2; // unused in CAN_DATA_FRAME
			size = (uint8_t)(nb%(maxMsgSize+1)); // try 0 to 8

			if (brgStat == BRG_NO_ERR) {
				brgStat = CanMsgTxRxVerif(&canTxMsg, dataTx, &canRxMsg, dataRx, CAN_MSG_RX_FIFO0, size);
			}
			nb++;
		}
	}

	// disable used filter: std filter0
	if (brgStat == BRG_NO_ERR) {
		brgStat = CanFilterDisable(&filterConf, 0, CAN_ID_STANDARD);
	}

	if (brgStat == BRG_NO_ERR) {
		brgStat =  m_pBrg->StopMsgReceptionCAN();
		if( brgStat != BRG_NO_ERR) {
			printf("CAN StopMsgReceptionCAN failed \n");
		}
	} else { // stop anyway
		m_pBrg->StopMsgReceptionCAN();
	}

	if (brgStat == BRG_NO_ERR) {
		printf(" CanLoopBack test OK \n");
	}

	return brgStat;
}

Brg_StatusT cBrgExample::CanFilterDisable(Brg_CanFilterConfT* pFilterConf, uint8_t filterNb, Brg_CanMsgIdT filterIde)
{
	Brg_StatusT brgStat;
	int i;
	pFilterConf->FilterBankNb = filterNb;
	pFilterConf->bIsFilterEn = false;
	// ID unused but must be valid value (ID 0 valid for both std and ext)
	for (i = 0; i<4; i++) {
		pFilterConf->Id[i].ID = 0;
		pFilterConf->Id[i].IDE = filterIde; //must match enabled filter for STLINK-V3PWR
		pFilterConf->Id[i].RTR = CAN_DATA_FRAME;
	}
	for (i = 0; i<2; i++) { //unused
		pFilterConf->Mask[i].ID = 0;
		pFilterConf->Mask[i].IDE = CAN_ID_STANDARD;
		pFilterConf->Mask[i].RTR = CAN_DATA_FRAME;
	}
	//unused when disabling
	pFilterConf->FilterMode = CAN_FILTER_ID_LIST;
	pFilterConf->FilterScale = CAN_FILTER_32BIT;
	pFilterConf->AssignedFifo = CAN_MSG_RX_FIFO0;

	brgStat = m_pBrg->InitFilterCAN(pFilterConf);
	if( brgStat != BRG_NO_ERR ) {
		if (filterIde == CAN_ID_EXTENDED) {
			printf("CAN ext filter%d Deinit failed \n", (int)filterNb);
		} else {
			printf("CAN std filter%d Deinit failed \n", (int)filterNb);
		}
	}
	return brgStat;
}

// send a message and verify it is received and that TX = Rx, Test CAN commands Brg::WriteMsgCAN Brg::GetRxMsgNbCAN Brg::GetRxMsgCAN
Brg_StatusT cBrgExample::CanMsgTxRxVerif(Brg_CanTxMsgT *pCanTxMsg, uint8_t *pDataTx, Brg_CanRxMsgT *pCanRxMsg, uint8_t *pDataRx, Brg_CanRxFifoT rxFifo, uint8_t size)
{
	Brg_StatusT brgStat = BRG_NO_ERR;
	uint16_t msgNb = 0;
	// Send message
	if( brgStat == BRG_NO_ERR ) {
		brgStat = m_pBrg->WriteMsgCAN(pCanTxMsg, pDataTx, size);
		if( brgStat != BRG_NO_ERR ) {
			printf("CAN Write Message error (Tx ID: 0x%08X)\n", (unsigned int)pCanTxMsg->ID);
		}
	}
	// Receive message
	if( brgStat == BRG_NO_ERR ) {
		uint16_t dataSize;
		int retry = 100;
		while( (retry > 0)&&(msgNb==0) ) {
			brgStat = m_pBrg->GetRxMsgNbCAN(&msgNb);
			retry --;
		}
		if( msgNb == 0 ) { // check if enough messages available
			brgStat = BRG_TARGET_CMD_TIMEOUT;
			printf("CAN Rx error (not enough msg available: 0/1)\n");
		}
		if( brgStat == BRG_NO_ERR ) { // read only 1 msg even if more available
			brgStat = m_pBrg->GetRxMsgCAN(pCanRxMsg, 1, pDataRx, 8, &dataSize);
		}
		if( brgStat != BRG_NO_ERR ) {
			printf("CAN Read Message error (Tx ID: 0x%08X, nb of Rx msg available: %d)\n", (unsigned int)pCanTxMsg->ID, (int)msgNb);
		} else {
			if( pCanRxMsg->Fifo != rxFifo ) {
				printf("CAN Read Message FIFO error (Tx ID: 0x%08X in FIFO%d instead of %d)\n", (unsigned int)pCanTxMsg->ID, (int)pCanRxMsg->Fifo, (int)rxFifo);
				brgStat = BRG_VERIF_ERR;
			}
		}
	}
	// verif Rx = Tx
	if( brgStat == BRG_NO_ERR ) {
		if( (pCanRxMsg->ID != pCanTxMsg->ID) || (pCanRxMsg->IDE != pCanTxMsg->IDE) || (pCanRxMsg->DLC != size) ||
			(pCanRxMsg->Overrun != CAN_RX_NO_OVERRUN) ) {
			brgStat = BRG_CAN_ERR;
			printf("CAN ERROR ID Rx: 0x%08X Tx 0x%08X, IDE Rx %d Tx %d, DLC Rx %d size Tx %d\n", (unsigned int)pCanRxMsg->ID, (unsigned int)pCanTxMsg->ID, (int)pCanRxMsg->IDE, (int) pCanTxMsg->IDE, (int)pCanRxMsg->DLC, (int)size);
		} else {
			for(int i=0; i<size; i++) {
				if( pDataRx[i] != pDataTx[i] ) {
					printf("CAN ERROR data[%d] Rx: 0x%02hX Tx 0x%02hX \n", (int)i, (unsigned short)(unsigned char)pDataRx[i], (unsigned short)(unsigned char)pDataTx[i]);
					brgStat = BRG_VERIF_ERR;
				}
			}
		}
		if( brgStat != BRG_NO_ERR ) {
			printf("CAN ERROR Read/Write verification \n");
		}
	}
	return brgStat;
}

/*****************************************************************************/
// Main example
/*****************************************************************************/

// main() Defines the entry point for the console application.
#ifdef WIN32 //Defined for applications for Win32 and Win64.
int _tmain(int argc, _TCHAR* argv[])

#else
using namespace std;

int main(int argc, char** argv)
#endif
{
	cBrgExample brgTest;
	Brg_StatusT brgStat = BRG_NO_ERR;
    STLinkIf_StatusT ifStat = STLINKIF_NO_ERR;
	char path[MAX_PATH];
#ifdef WIN32 //Defined for applications for Win32 and Win64.
	char *pEndOfPrefix;
#endif
	int firstDevNotInUse=-1;
	Brg* pBrg = NULL;
	STLinkInterface *m_pStlinkIf = NULL;

	// Note: cErrLog g_ErrLog; to be instanciated and initialized if used with USING_ERRORLOG

	// In case previously used, close the previous connection (not the case here)
	if (pBrg!=NULL) {
		pBrg->CloseBridge(COM_UNDEF_ALL);
		pBrg->CloseStlink();
		delete pBrg;
		pBrg = NULL;
	}
	if (m_pStlinkIf !=NULL) {// never delete STLinkInterface before Brg that is using it.
		delete m_pStlinkIf;
		m_pStlinkIf = NULL;
	}

	// USB interface initialization and device detection done using STLinkInterface

	// Create USB BRIDGE interface
	m_pStlinkIf = new STLinkInterface(STLINK_BRIDGE);
#ifdef USING_ERRORLOG
	m_pStlinkIf->BindErrLog(&g_ErrLog);
#endif

#ifdef WIN32 //Defined for applications for Win32 and Win64.
	GetModuleFileNameA(NULL, path, MAX_PATH); //may require shlwapi library in "Additionnal Dependencies Input" linker settings
	// Remove process file name from the path
	pEndOfPrefix = strrchr(path,'\\');

	if (pEndOfPrefix != NULL)
	{
		*(pEndOfPrefix + 1) = '\0';
	}
#else
	strcpy(path, "");
#endif
	// Load STLinkUSBDriver library
	// In this example STLinkUSBdriver (dll on windows) must be copied near test executable
	// Copy last STLinkUSBDriver dll from  STSW-LINK007 package (STLINK firmware upgrade application),
	// choose correct library according to your project architecture
	ifStat = m_pStlinkIf->LoadStlinkLibrary(path);
	if( ifStat!=STLINKIF_NO_ERR ) {
		printf("STLinkUSBDriver library (dll) issue \n");
	}

	// Enumerate the STLink Bridge instance, and choose the first one in the list
	brgStat = brgTest.SelectSTLink(m_pStlinkIf, &firstDevNotInUse);

	// USB Connection to a given device done with Brg
	if (brgStat == BRG_NO_ERR) {
		pBrg = new Brg(*m_pStlinkIf);
#ifdef USING_ERRORLOG
		m_pBrg->BindErrLog(&g_ErrLog);
#endif
	}
	// Connect to the selected STLink
	if (brgStat == BRG_NO_ERR) {
		brgStat = brgTest.Connect(pBrg, firstDevNotInUse);
	}

    // Check for module ID
    if(argc < 2)
    {
        printf("No module ID specified, aborting CAN bootloader start\n");
        brgStat = BRG_INTERFACE_ERR;
    }
    else
    {
        // Send CAN message to start CAN bootloader over GCAN
        int moduleId = atoi(argv[1]);
        brgStat = brgTest.SendCanBootloaderStart(moduleId);
    }

	// test disconnect
	brgTest.Disconnect();

	// STLink Disconnect
	if (pBrg!=NULL) {
		pBrg->CloseBridge(COM_UNDEF_ALL);
		pBrg->CloseStlink();
		delete pBrg;
		pBrg = NULL;
	}
	// unload STLinkUSBdriver library
	if (m_pStlinkIf!=NULL) {
		// always delete STLinkInterface after Brg (because Brg uses STLinkInterface)
		delete m_pStlinkIf;
		m_pStlinkIf = NULL;
	}

	if (brgStat == BRG_NO_ERR) 	{
		printf("CAN Bootloader Start SUCCESS \n");
        return 0;
	} else {
		printf("CAN Bootloader Start FAIL (Bridge error: %d) \n", (int)brgStat);
        return 1;
	}
}

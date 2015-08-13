/******************************************************************************
*
* Copyright (C) 2015 Xilinx, Inc. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file xdprxss.c
*
* This is the main file for Xilinx DisplayPort Receiver Subsystem driver.
* This file contains a minimal set of functions for the XDpRxSs driver that
* allow access to all of the DisplayPort Receiver Subsystem core's
* functionality. Please see xdprxss.h for more details of the driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver  Who Date     Changes
* ---- --- -------- -----------------------------------------------------
* 1.00 sha 05/18/15 Initial release.
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xdprxss.h"
#include "xvidc_dp159.h"
#include "string.h"
#include "xdebug.h"

/************************** Constant Definitions *****************************/


/***************** Macros (Inline Functions) Definitions *********************/


/**************************** Type Definitions *******************************/

/* Subsystem sub-core's structure includes instances of each sub-core */
typedef struct {
	XDp DpInst;
	XIic IicInst;
} XDpRxSs_SubCores;

/************************** Function Prototypes ******************************/

static void DpRxSs_GetIncludedSubCores(XDpRxSs *InstancePtr);
static void DpRxSs_PopulateDpRxPorts(XDpRxSs *InstancePtr);
static void StubTp1Callback(void *InstancePtr);
static void StubTp2Callback(void *InstancePtr);
static void StubUnplugCallback(void *InstancePtr);

/************************** Variable Definitions *****************************/

/* A generic EDID structure. */
u8 GenEdid[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x61, 0x2c, 0x01, 0x00, 0x78, 0x56, 0x34, 0x12,
	0x01, 0x18, 0x01, 0x04, 0xa0, 0x2f, 0x1e, 0x78,
	0x00, 0xee, 0x95, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x21, 0x08, 0x00, 0x71, 0x4f,
	0x81, 0x80, 0xb3, 0x00, 0xd1, 0xc0, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xff, 0x00, 0x58, 0x49, 0x4c,
	0x44, 0x50, 0x53, 0x49, 0x4e, 0x4b, 0x0a, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x58,
	0x49, 0x4c, 0x20, 0x44, 0x50, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
	0x00, 0x38, 0x3c, 0x1e, 0x53, 0x10, 0x00, 0x0a,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x39
};

/* A generic DPCD structure. */
u8 GenDpcd[] = {
	0x12, 0x0a, 0x84, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02
};

/* DisplayPort RX subcores instance */
XDpRxSs_SubCores DpRxSsSubCores;

/************************** Function Definitions *****************************/

/*****************************************************************************/
/**
*
* This function initializes the DisplayPort Receiver Subsystem core. This
* function must be called prior to using the core. Initialization of the core
* includes setting up the instance data and ensuring the hardware is in a
* quiescent state.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
* @param	CfgPtr points to the configuration structure associated with
*		the DisplayPort RX Subsystem core.
* @param	EffectiveAddr is the base address of the device. If address
*		translation is being used, then this parameter must reflect the
*		virtual base address. Otherwise, the physical address should be
*		used.
*
* @return
*		- XST_DEVICE_NOT_FOUND if sub-core not found.
*		- XST_FAILURE if sub-core initialization failed.
*		- XST_SUCCESS if XDpRxSs_CfgInitialize successful.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_CfgInitialize(XDpRxSs *InstancePtr, XDpRxSs_Config *CfgPtr,
				u32 EffectiveAddr)
{
	XIic_Config IicConfig;
	XDp_Config DpConfig;
	u32 Status;

	/* Verify arguments. */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(CfgPtr != NULL);
	Xil_AssertNonvoid(EffectiveAddr != (u32)0x0);

	/* Setup the instance */
	(void)memset((void *)InstancePtr, 0, sizeof(XDpRxSs));
	(void)memcpy((void *)&(InstancePtr->Config), (const void *)CfgPtr,
			sizeof(XDpRxSs_Config));

	InstancePtr->Config.BaseAddress = EffectiveAddr;

	/* Get included sub-cores in the DisplayPort RX Subsystem */
	DpRxSs_GetIncludedSubCores(InstancePtr);

	/* Check for IIC availability */
	if (InstancePtr->IicPtr) {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS INFO: Initializing "
			"IIC IP\n\r");

		/* Calculate absolute base address of IIC sub-core */
		InstancePtr->Config.IicSubCore.IicConfig.BaseAddress +=
					InstancePtr->Config.BaseAddress;

		/* Copy IIC config into local IIC config */
		(void)memcpy((void *)&(IicConfig),
			(const void *)&CfgPtr->IicSubCore.IicConfig,
				sizeof(IicConfig));

		/* IIC config initialize */
		IicConfig.BaseAddress += InstancePtr->Config.BaseAddress;
		Status = XIic_CfgInitialize(InstancePtr->IicPtr, &IicConfig,
				IicConfig.BaseAddress);
		if ((Status != XST_SUCCESS) && (Status !=
						XST_DEVICE_IS_STARTED)) {
			xdbg_printf(XDBG_DEBUG_GENERAL,"SS ERR:: IIC "
				"initialization failed!\n\r");
			return XST_FAILURE;
		}

		/* Reset DP159 */
		XVidC_Dp159Reset(InstancePtr->IicPtr, TRUE);

		/* IIC initialization for dynamic functionality */
		Status = XIic_DynamicInitialize(InstancePtr->IicPtr);
		if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_GENERAL,"SS ERR:: IIC "
				"dynamic initialization failed!\n\r");
			return XST_FAILURE;
		}
	}

	/* Check for DisplayPort availability */
	if (InstancePtr->DpPtr) {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS INFO: Initializing "
			"DisplayPort Receiver IP\n\r");

		/* Assign number of streams to one when MST is not enabled */
		if (InstancePtr->Config.MstSupport) {
			InstancePtr->UsrOpt.NumOfStreams =
					InstancePtr->Config.NumMstStreams;
		}
		else {
			InstancePtr->Config.DpSubCore.DpConfig.NumMstStreams =
				1;
			InstancePtr->UsrOpt.NumOfStreams = 1;
			InstancePtr->Config.NumMstStreams = 1;
		}

		/* Calculate absolute base address of DP sub-core */
		InstancePtr->Config.DpSubCore.DpConfig.BaseAddr +=
					InstancePtr->Config.BaseAddress;

		/* Copy DP config into local DP config */
		(void)memcpy((void *)&(DpConfig),
			(const void *)&CfgPtr->DpSubCore.DpConfig,
				sizeof(XDp_Config));

		/* DisplayPort config initialize */
		DpConfig.BaseAddr += InstancePtr->Config.BaseAddress;
		XDp_CfgInitialize(InstancePtr->DpPtr, &DpConfig,
				DpConfig.BaseAddr);

		/* Set maximum link rate for the first time */
		XDp_RxSetLinkRate(InstancePtr->DpPtr,
				InstancePtr->DpPtr->Config.MaxLinkRate);

		/* Set maximum lane count for the first time */
		XDp_RxSetLaneCount(InstancePtr->DpPtr,
				InstancePtr->Config.MaxLaneCount);

		/* Bring DP159 out of reset */
		XVidC_Dp159Reset(InstancePtr->IicPtr, FALSE);

		/* Wait for us */
		XDp_WaitUs(InstancePtr->DpPtr, 800);

		/* Initialize DP159 */
		XVidC_Dp159Initialize(InstancePtr->IicPtr);

		/* Wait for us */
		XDp_WaitUs(InstancePtr->DpPtr, 900);

		/* Initialize default training pattern callbacks in DP RX
		 * Subsystem
		 */
		XDp_RxSetIntrTp1Handler(InstancePtr->DpPtr, StubTp1Callback,
			(void *)InstancePtr);
		XDp_RxSetIntrTp2Handler(InstancePtr->DpPtr, StubTp2Callback,
			(void *)InstancePtr);
		XDp_RxSetIntrTp3Handler(InstancePtr->DpPtr, StubTp2Callback,
			(void *)InstancePtr);
		XDp_RxSetIntrUnplugHandler(InstancePtr->DpPtr,
			StubUnplugCallback, (void *)InstancePtr);

		/* Initialize configurable parameters */
		InstancePtr->UsrOpt.Bpc = InstancePtr->Config.MaxBpc;
		InstancePtr->UsrOpt.LaneCount =
					InstancePtr->Config.MaxLaneCount;
		InstancePtr->UsrOpt.LinkRate =
					InstancePtr->DpPtr->Config.MaxLinkRate;
		InstancePtr->UsrOpt.MstSupport =
				InstancePtr->Config.MstSupport;

		/* Populate the RX core's ports with default values */
		DpRxSs_PopulateDpRxPorts(InstancePtr);
	}

	/* Set the flag to indicate the subsystem is ready */
	InstancePtr->IsReady = (u32)(XIL_COMPONENT_IS_READY);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function resets the DisplayPort Receiver Subsystem including all
* sub-cores.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		IIC needs to be reinitialized after reset.
*
******************************************************************************/
void XDpRxSs_Reset(XDpRxSs *InstancePtr)
{
	/* Verify argument. */
	Xil_AssertVoid(InstancePtr != NULL);

	/* Reset the video and AUX logic from DP RX */
	XDpRxSs_WriteReg(InstancePtr->DpPtr->Config.BaseAddr,
			XDP_RX_SOFT_RESET, XDP_RX_SOFT_RESET_VIDEO_MASK |
				XDP_RX_SOFT_RESET_AUX_MASK);

	/* Reset the IIC core */
	XIic_Reset(InstancePtr->IicPtr);
}

/*****************************************************************************/
/**
*
* This function starts the DisplayPort Receiver Subsystem including all
* sub-cores.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return
*		- XST_SUCCESS, if DP RX Subsystem and its included sub-cores
*		configured successfully.
*		- XST_FAILURE, otherwise.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_Start(XDpRxSs *InstancePtr)
{
	u32 Status;

	/* Verify arguments. */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid((InstancePtr->UsrOpt.MstSupport == 0) ||
				(InstancePtr->UsrOpt.MstSupport == 1));

	/* Reinitialize DP */
	Status = XDp_Initialize(InstancePtr->DpPtr);
	if (Status != (XST_SUCCESS)) {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS ERR::DP RX "
			"start failed!\n\r");
		Status = XST_FAILURE;
	}

	return Status;
}

/*****************************************************************************/
/**
*
* This function sets the data rate to be used by the DisplayPort RX Subsystem
* core.
*
* @param	InstancePtr is a pointer to the XDpRxSs instance.
* @param	LinkRate is the rate at which link needs to be driven.
*		- XDPRXSS_LINK_BW_SET_162GBPS = 0x06(for a 1.62 Gbps data rate)
*		- XDPRXSS_LINK_BW_SET_270GBPS = 0x0A(for a 2.70 Gbps data rate)
*		- XDPRXSS_LINK_BW_SET_540GBPS = 0x14(for a 5.40 Gbps data rate)
*
* @return
*		- XST_SUCCESS if setting the new lane rate was successful.
*		- XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_SetLinkRate(XDpRxSs *InstancePtr, u8 LinkRate)
{
	/* Verify arguments. */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid((LinkRate == (XDPRXSS_LINK_BW_SET_162GBPS)) ||
			(LinkRate == (XDPRXSS_LINK_BW_SET_270GBPS)) ||
			(LinkRate == (XDPRXSS_LINK_BW_SET_540GBPS)));

	/* Check for maximum supported link rate */
	if (LinkRate > InstancePtr->DpPtr->Config.MaxLinkRate) {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS info: This link rate is "
			"not supported by Source/Sink.\n\rMax Supported link "
			"rate is 0x%x.\n\rSetting maximum supported link "
			"rate.\n\r", InstancePtr->DpPtr->Config.MaxLinkRate);
		LinkRate = InstancePtr->DpPtr->Config.MaxLinkRate;
	}

	/* Set link rate */
	XDp_RxSetLinkRate(InstancePtr->DpPtr, LinkRate);
	InstancePtr->UsrOpt.LinkRate = LinkRate;

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function sets the number of lanes to be used by DisplayPort RX Subsystem
* core.
*
* @param	InstancePtr is a pointer to the XDpRxSs instance.
* @param	LaneCount is the number of lanes to be used.
*		- 1 = XDPRXSS_LANE_COUNT_SET_1
*		- 2 = XDPRXSS_LANE_COUNT_SET_2
*		- 4 = XDPRXSS_LANE_COUNT_SET_4
* @return
*		- XST_SUCCESS if setting the new lane count was successful.
*		- XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_SetLaneCount(XDpRxSs *InstancePtr, u8 LaneCount)
{
	/* Verify arguments. */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid((LaneCount == (XDPRXSS_LANE_COUNT_SET_1)) ||
			(LaneCount == (XDPRXSS_LANE_COUNT_SET_2)) ||
			(LaneCount == (XDPRXSS_LANE_COUNT_SET_4)));

	/* Check for maximum supported lane count */
	if (LaneCount > InstancePtr->DpPtr->Config.MaxLaneCount) {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS info: This lane count is "
			"not supported by Source/Sink.\n\rMax Supported lane "
			"count is 0x%x.\n\rSetting maximum supported lane "
			"count.\n\r", InstancePtr->DpPtr->Config.MaxLaneCount);
		LaneCount = InstancePtr->DpPtr->Config.MaxLaneCount;
	}

	/* Set lane count */
	XDp_RxSetLaneCount(InstancePtr->DpPtr, LaneCount);
	InstancePtr->UsrOpt.LaneCount = LaneCount;

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function allows the user to select number of ports to be exposed when
* replying to a LINK_ADDRESS sideband message and hides rest of the ports.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
* @param	Port specifies the number of ports to be exposed within the
*		range 1 to 4.
*
* @return
*		- XST_SUCCESS, if ports exposed successfully.
*		- XST_FAILURE, if exposing ports which are already exposed or
*		ports are exceeding total number of stream supported by the
*		system.
*
* @note		Number of ports are equal to number of streams.
*
******************************************************************************/
u32 XDpRxSs_ExposePort(XDpRxSs *InstancePtr, u8 Port)
{
	u32 Status;
	u8 PortIndex;

	/* Verify arguments. */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(Port > 0x0);

	if (Port == InstancePtr->UsrOpt.NumOfStreams) {
		xdbg_printf(XDBG_DEBUG_GENERAL,"SS INFO:Subsystem is "
			"already in %s mode with port[s] %d.\n\r",
				InstancePtr->UsrOpt.MstSupport? "MST": "SST",
					Port);
		Status = XST_FAILURE;
	}
	/* Check for stream[s] less than supported stream[s] */
	else if (Port < InstancePtr->Config.NumMstStreams) {
		/* Expose each port */
		for (PortIndex = 0; PortIndex < Port; PortIndex++) {
			/* Expose the ports configured above. Used when
			 * replying to a LINK_ADDRESS request. Make sure that
			 * the number of downstream ports matches the number
			 * exposed, otherwise the LINK_ADDRESS reply will be
			 * incorrect
			 */
			XDp_RxMstExposePort(InstancePtr->DpPtr,
				PortIndex + 1, 1);
		}

		/* Set number of ports/stream[s] are exposed */
		InstancePtr->UsrOpt.NumOfStreams = Port;

		/* Hide remaining ports */
		for (PortIndex = Port;
			PortIndex <= InstancePtr->Config.NumMstStreams;
								PortIndex++) {
			XDp_RxMstExposePort(InstancePtr->DpPtr,
				PortIndex + 1, 0);
		}

		Status = XST_SUCCESS;
	}
	/* Everything else */
	else {
		xdbg_printf((XDBG_DEBUG_GENERAL),"SS ERR::Subsystem does not "
			"support %s mode with stream[s] %d\n\r",
				InstancePtr->UsrOpt.MstSupport? "MST": "SST",
					Port);
		Status = XST_FAILURE;
	}

	return Status;
}

/*****************************************************************************/
/**
*
* This function checks if the receiver's DisplayPort Configuration Data (DPCD)
* indicates that the receiver has achieved clock recovery, channel
* equalization, symbol lock, and interlane alignment for all lanes currently
* in use.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return
*		- XST_SUCCESS if the RX device has achieved clock recovery,
*		  channel equalization, symbol lock, and interlane alignment.
*		- XST_DEVICE_NOT_FOUND if no RX device is connected.
*		- XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_CheckLinkStatus(XDpRxSs *InstancePtr)
{
	u32 Status;

	/* Verify argument.*/
	Xil_AssertNonvoid(InstancePtr != NULL);

	/* Check link status */
	Status = XDp_RxCheckLinkStatus(InstancePtr->DpPtr);

	return Status;
}

/*****************************************************************************/
/**
*
* This function configures the number of pixels output through the user data
* interface.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
* @param	UserPixelWidth is the user pixel width to be configured.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XDpRxSs_SetUserPixelWidth(XDpRxSs *InstancePtr, u8 UserPixelWidth)
{
	/* Verify arguments.*/
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid((UserPixelWidth == 1) || (UserPixelWidth == 2) ||
			(UserPixelWidth == 4));

	/* Set user pixel width */
	XDp_RxSetUserPixelWidth(InstancePtr->DpPtr, UserPixelWidth);
}

/*****************************************************************************/
/**
*
* This function handles incoming sideband messages. It will
* 1) Read the contents of the down request registers,
* 2) Delegate control depending on the request type, and
* 3) Send a down reply.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return
*		- XST_SUCCESS if the entire message was sent successfully.
*		- XST_DEVICE_NOT_FOUND if no device is connected.
*		- XST_ERROR_COUNT_MAX if sending one of the message fragments
*		timed out.
*		- XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
u32 XDpRxSs_HandleDownReq(XDpRxSs *InstancePtr)
{
	u32 Status;

	/* Verify argument.*/
	Xil_AssertNonvoid(InstancePtr != NULL);

	Status = XDp_RxHandleDownReq(InstancePtr->DpPtr);

	return Status;
}

/*****************************************************************************/
/**
*
* This function reports list of cores included in DisplayPort RX Subsystem.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void DpRxSs_GetIncludedSubCores(XDpRxSs *InstancePtr)
{
	/* Assign instance of DisplayPort core */
	InstancePtr->DpPtr = ((InstancePtr->Config.DpSubCore.IsPresent)?
					(&DpRxSsSubCores.DpInst): NULL);
	/* Assign instance of IIC core */
	InstancePtr->IicPtr = ((InstancePtr->Config.DpSubCore.IsPresent)?
					(&DpRxSsSubCores.IicInst): NULL);
}

/*****************************************************************************/
/**
*
* This function sets up the downstream topology that will be used by the
* DisplayPort RX to respond to down requests coming from the TX.
* - Sinks equal to number of streams (each with its own global unique
*   identifier) are exposed to LINK_ADDRESS sideband messages.
* - The extended display identification data (EDID) will be set to a generic
*   EDID (GenEdid) for each of the sinks. REMOTE_I2C_READ sideband messages
*   on address 0x50 will be replied to with the contents set by
*   XDp_RxSetIicMapEntry for 0x50.
* - REMOTE_DPCD_READ sideband messages will be responded to with the contents
*   set for the sink using XDp_RxSetDpcdMap. All sinks are given the a generic
*   DPCD (GenDpcd).
* - ENUM_PATH_RESOURCES sideband messages will be responded to with the value
*   set by XDp_RxMstSetPbn (or 0 if the sink already has a stream allocated to
*   it).
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void DpRxSs_PopulateDpRxPorts(XDpRxSs *InstancePtr)
{
	XDp_SbMsgLinkAddressReplyPortDetail Port;
	u8 PortIndex;
	u8 GuidIndex;
	u8 StreamIndex;

	/* Check for MST */
	if (InstancePtr->Config.MstSupport) {
		/* Ensure that all ports are not exposed in the link address */
		for (PortIndex = 0; PortIndex < XDP_MAX_NPORTS; PortIndex++) {
			XDp_RxMstExposePort(InstancePtr->DpPtr, PortIndex, 0);
		}

		/* Configure the commonality between the downstream sink
		 * devices
		 */
		Port.InputPort = 0;
		Port.PeerDeviceType = 0x3;
		Port.MsgCapStatus = 0;
		Port.DpDevPlugStatus = 1;
		Port.LegacyDevPlugStatus = 0;
		Port.DpcdRev = 0x11;
		Port.NumSdpStreams = InstancePtr->Config.MaxNumAudioCh;
		Port.NumSdpStreamSinks = InstancePtr->Config.MaxNumAudioCh;

		/* Configure the unique port number and GUID for all possible
		 * downstream sinks
		 */
		for (PortIndex = 1; PortIndex < XDPRXSS_MAX_NPORTS;
								PortIndex++) {
			/* Set the GUID to a repeating pattern of the port
			 * number
			 */
			for (GuidIndex = 0; GuidIndex < XDPRXSS_GUID_NBYTES;
								GuidIndex++) {
				Port.Guid[GuidIndex] = PortIndex;
			}
			/* Port.PortNum is set to the index. */
			XDp_RxMstSetPort(InstancePtr->DpPtr, PortIndex, &Port);
		}

		/* Configure each port with I2C, DPCD and PBN values */
		for (StreamIndex = 0;
			StreamIndex < InstancePtr->Config.NumMstStreams;
							StreamIndex++) {
			/* Set I2C maps. */
			XDp_RxSetIicMapEntry(InstancePtr->DpPtr,
				StreamIndex + 1, 0x50, 128, GenEdid);

			/* Set DPCD maps. */
			XDp_RxSetDpcdMap(InstancePtr->DpPtr, StreamIndex + 1,
				0, sizeof(GenDpcd), GenDpcd);

			/* Set available PBN. */
			XDp_RxMstSetPbn(InstancePtr->DpPtr, StreamIndex + 1,
				2560);

			/* Expose the ports configured above. Used when
			 * replying to a LINK_ADDRESS request. Make sure that
			 * the number of downstream ports matches the number
			 * exposed, otherwise the LINK_ADDRESS reply will be
			 * incorrect
			 */
			XDp_RxMstExposePort(InstancePtr->DpPtr,
				StreamIndex + 1, 1);
		}

		/* Set up the input port and expose it */
		XDp_RxMstSetInputPort(InstancePtr->DpPtr, 0, NULL);
	}
}

/*****************************************************************************/
/**
*
* This routine is a stub for the asynchronous training pattern 1 interrupt
* callback. On initialization, training pattern 1 interrupt handler is set to
* this callback. It is considered as an training pattern 1 for this handler
* to be invoked.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void StubTp1Callback(void *InstancePtr)
{
	XDpRxSs *DpRxSsPtr = (XDpRxSs *)InstancePtr;

	/* Verify argument.*/
	Xil_AssertVoid(DpRxSsPtr != NULL);

	/* Read link rate */
	DpRxSsPtr->UsrOpt.LinkRate =
		XDpRxSs_ReadReg(DpRxSsPtr->DpPtr->Config.BaseAddr,
			XDPRXSS_DPCD_LINK_BW_SET);

	/* Read lane count */
	DpRxSsPtr->UsrOpt.LaneCount =
		XDpRxSs_ReadReg(DpRxSsPtr->DpPtr->Config.BaseAddr,
			XDPRXSS_DPCD_LANE_COUNT_SET);

	/* Link bandwidth callback */
	if (DpRxSsPtr->LinkBwCallback) {
		DpRxSsPtr->LinkBwCallback(DpRxSsPtr->LinkBwRef);
	}

	/* DP159 config for TP1 */
	XVidC_Dp159Config(DpRxSsPtr->IicPtr, XVIDC_DP159_CT_TP1,
			DpRxSsPtr->UsrOpt.LinkRate,
				DpRxSsPtr->UsrOpt.LaneCount);

	XDpRxSs_WriteReg(DpRxSsPtr->DpPtr->Config.BaseAddr,
		XDPRXSS_RX_PHY_CONFIG, 0x3800000);

	/* PLL reset callback */
	if (DpRxSsPtr->PllResetCallback) {
		DpRxSsPtr->PllResetCallback(DpRxSsPtr->PllResetRef);
	}

	/* Set vertical blank */
	DpRxSsPtr->VBlankEnable = 1;

	/* Enable vertical blank interrupt */
	XDp_RxInterruptEnable(DpRxSsPtr->DpPtr, XDPRXSS_INTR_VBLANK_MASK);

	/* Set vertical blank count */
	DpRxSsPtr->VBlankCount = 0;
}

/*****************************************************************************/
/**
*
* This routine is a stub for the asynchronous training pattern 2 interrupt
* callback. On initialization, training pattern 2 interrupt handler is set to
* this callback. It is considered as an training pattern 2 for this handler
* to be invoked.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void StubTp2Callback(void *InstancePtr)
{
	XDpRxSs *DpRxSsPtr = (XDpRxSs *)InstancePtr;

	/* Verify argument.*/
	Xil_AssertVoid(DpRxSsPtr != NULL);

	/* DP159 config for TP2 */
	XVidC_Dp159Config(DpRxSsPtr->IicPtr, XVIDC_DP159_CT_TP2,
			DpRxSsPtr->UsrOpt.LinkRate,
				DpRxSsPtr->UsrOpt.LaneCount);
}

/*****************************************************************************/
/**
*
* This routine is a stub for the asynchronous unplug interrupt callback.
* On initialization, unplug interrupt handler is set to this callback. It is
* considered as an unplug for this handler to be invoked.
*
* @param	InstancePtr is a pointer to the XDpRxSs core instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void StubUnplugCallback(void *InstancePtr)
{
	XDpRxSs *DpRxSsPtr = (XDpRxSs *)InstancePtr;

	/* Verify argument.*/
	Xil_AssertVoid(DpRxSsPtr != NULL);

	/* DP159 config for TP2 */
	XVidC_Dp159Config(DpRxSsPtr->IicPtr, XVIDC_DP159_CT_UNPLUG,
		DpRxSsPtr->UsrOpt.LinkRate, DpRxSsPtr->UsrOpt.LaneCount);

	/* Disable unplug interrupt so that no unplug event when RX is
	 * disconnected
	 */
	XDp_RxInterruptDisable(DpRxSsPtr->DpPtr,
				XDP_RX_INTERRUPT_MASK_UNPLUG_MASK);

	/* Unplug event callback */
	if (DpRxSsPtr->UnplugCallback) {
		DpRxSsPtr->UnplugCallback(DpRxSsPtr->UnplugRef);
	}
}
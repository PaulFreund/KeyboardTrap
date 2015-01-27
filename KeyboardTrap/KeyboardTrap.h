//######################################################################################################################
/*
	Copyright (c) since 2014 - Paul Freund

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
*/
//######################################################################################################################

#include <ntddk.h>
#include <wdf.h>

#include <kbdmou.h>
#include <ntddmou.h>

//======================================================================================================================

#if DBG
	#define TRAP() DbgBreakPoint()
	#define DebugPrint(_x_) DbgPrint _x_
#else   // DBG
	#define TRAP()
	#define DebugPrint(_x_)
#endif

//======================================================================================================================

DEFINE_GUID(GUID_DEVINTERFACE_KEYBOARDTRAP, 0xea708fa1, 0x5397, 0x4eec, 0x8d, 0x13, 0xf4, 0x97, 0x3b, 0x69, 0x29, 0x32);

//======================================================================================================================

typedef struct _DEVICE_CONTEXT {
	CONNECT_DATA UpperConnectData;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;


WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//======================================================================================================================

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KeyboardTrapEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KeyboardTrapEvtIoInternalDeviceControl;

void KeyboardTrapServiceCallback(DEVICE_OBJECT* deviceObject, KEYBOARD_INPUT_DATA* inputDataStart, KEYBOARD_INPUT_DATA* inputDataEnd, ULONG* inputDataConsumed);

//======================================================================================================================

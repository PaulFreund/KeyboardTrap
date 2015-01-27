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

#include "KeyboardTrap.h"

//======================================================================================================================

#ifdef ALLOC_PRAGMA
	#pragma alloc_text (INIT, DriverEntry)
	#pragma alloc_text (PAGE, KeyboardTrapEvtDeviceAdd)
	#pragma alloc_text (PAGE, KeyboardTrapEvtIoInternalDeviceControl)
#endif

//======================================================================================================================

NTSTATUS DriverEntry(DRIVER_OBJECT* driverObject, UNICODE_STRING* registryPath) {
	WDF_DRIVER_CONFIG config;
	WDF_DRIVER_CONFIG_INIT(&config, KeyboardTrapEvtDeviceAdd);

	NTSTATUS status = WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[KeyboardTrap] WdfDriverCreate failed with status 0x%x\n", status));
	}

	return status;
}

//======================================================================================================================

NTSTATUS KeyboardTrapEvtDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT deviceInit) {
	UNREFERENCED_PARAMETER(driver);

	PAGED_CODE(); // Ensure paging is allowed in current IRQL

	// Create filter
	WdfFdoInitSetFilter(deviceInit);

	// Set driver type to Keyboard
	WdfDeviceInitSetDeviceType(deviceInit, FILE_DEVICE_KEYBOARD);

	// Create attributes for a device extension
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	// Create framework device object 
	WDFDEVICE hDevice;
	NTSTATUS status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &hDevice);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[KeyboardTrap] WdfDeviceCreate failed with status code 0x%x\n", status));
		return status;
	}

	// Set request queue type
	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	// Set handler for device control requests 
	ioQueueConfig.EvtIoInternalDeviceControl = KeyboardTrapEvtIoInternalDeviceControl;

	// Create queue for filter device
	status = WdfIoQueueCreate(hDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[KeyboardTrap] WdfIoQueueCreate failed 0x%x\n", status));
		return status;
	}

	return status;
}


//======================================================================================================================

void KeyboardTrapEvtIoInternalDeviceControl(WDFQUEUE queue, WDFREQUEST request, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode) {
	UNREFERENCED_PARAMETER(outputBufferLength);
	UNREFERENCED_PARAMETER(inputBufferLength);

	NTSTATUS status = STATUS_SUCCESS;

	PAGED_CODE(); // Ensure paging is allowed in current IRQL

	// Get extension data
	WDFDEVICE hDevice = WdfIoQueueGetDevice(queue);
	PDEVICE_CONTEXT context = DeviceGetContext(hDevice);

	if(ioControlCode == IOCTL_INTERNAL_KEYBOARD_CONNECT) {
		// Only allow one connection.
		if(context->UpperConnectData.ClassService == NULL) {
			// Copy the connection parameters to the device extension.
			PCONNECT_DATA connectData;
			size_t length;
			status = WdfRequestRetrieveInputBuffer(request, sizeof(CONNECT_DATA), &connectData, &length);
			if(NT_SUCCESS(status)) {
				// Hook into the report chain (I am not sure this is correct)
				context->UpperConnectData = *connectData;
				connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

				#pragma warning(push)
				#pragma warning(disable:4152)
				connectData->ClassService = KeyboardTrapServiceCallback;
				#pragma warning(pop)
			}
			else {
				DebugPrint(("[KeyboardTrap] WdfRequestRetrieveInputBuffer failed %x\n", status));
			}
		}
		else {
			status = STATUS_SHARING_VIOLATION;
		}
	}
	else if(ioControlCode == IOCTL_INTERNAL_KEYBOARD_DISCONNECT) {
		status = STATUS_NOT_IMPLEMENTED;
	}

	// Complete on error
	if(!NT_SUCCESS(status)) {
		WdfRequestComplete(request, status);
		return;
	}

	// Dispatch to higher level driver
	WDF_REQUEST_SEND_OPTIONS options;
	WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

	if(WdfRequestSend(request, WdfDeviceGetIoTarget(hDevice), &options) == FALSE) {
		NTSTATUS status = WdfRequestGetStatus(request);
		DebugPrint(("[KeyboardTrap] WdfRequestSend failed: 0x%x\n", status));
		WdfRequestComplete(request, status);
	}
}

//======================================================================================================================

void KeyboardTrapServiceCallback(DEVICE_OBJECT* deviceObject, KEYBOARD_INPUT_DATA* inputDataStart, KEYBOARD_INPUT_DATA* inputDataEnd, ULONG* inputDataConsumed) {
	// Get context data
	WDFDEVICE hDevice = WdfWdmDeviceGetWdfDeviceHandle(deviceObject);
	PDEVICE_CONTEXT context = DeviceGetContext(hDevice);

	//// Invert all scroll actions
	for(PKEYBOARD_INPUT_DATA current = inputDataStart; current != inputDataEnd; current++) {
		DebugPrint(("[KeyboardTrap] Code: %d\n", current->MakeCode));
	}

	// Call parent
	#pragma warning(push)
	#pragma warning(disable:4055)
	(*(PSERVICE_CALLBACK_ROUTINE)context->UpperConnectData.ClassService)(context->UpperConnectData.ClassDeviceObject, inputDataStart, inputDataEnd, inputDataConsumed);
	#pragma warning(pop)

}

//======================================================================================================================

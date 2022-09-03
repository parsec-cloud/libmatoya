#include "matoya.h"
#include "pnpdevice.h"

#define DRIVER_VERSION_VALUE_NAME	"DriverVersion"


static MTY_PnPDeviceStatus dev_node_status_to_mty(ULONG devStatus, ULONG devProblemCode)
{
	// Device is operational
	if ((devStatus & DN_DRIVER_LOADED) && (devStatus & DN_STARTED))
	{
		return  MTY_PNP_DEVICE_STATUS_NORMAL;
	}

	// Device has problem status
	if ((devStatus & DN_HAS_PROBLEM))
	{
		switch (devProblemCode)
		{
		case CM_PROB_DISABLED:
		case CM_PROB_HARDWARE_DISABLED:
			return MTY_PNP_DEVICE_STATUS_DISABLED;
		case CM_PROB_FAILED_POST_START:
			return MTY_PNP_DEVICE_STATUS_DRIVER_ERROR;
		case CM_PROB_NEED_RESTART:
			return MTY_PNP_DEVICE_STATUS_RESTART_REQUIRED;
		case CM_PROB_DISABLED_SERVICE:
			return MTY_PNP_DEVICE_STATUS_DISABLED_SERVICE;
		default:
			return MTY_PNP_DEVICE_STATUS_UNKNOWN_PROBLEM;
		}
	}

	return MTY_PNP_DEVICE_STATUS_UNKNOWN;
}

bool MTY_PnPDeviceGetStatus(const GUID* classGuid, const char* hardwareId, uint32_t instanceIndex, MTY_PnPDeviceStatus* status)
{
	if (!classGuid || !hardwareId || !status)
		return false;

	DWORD err = ERROR_SUCCESS;
	bool found = false, succeeded = false;
	ULONG devStatus = 0, devProblemCode = 0;

	*status = MTY_PNP_DEVICE_STATUS_NOT_FOUND;

	SP_DEVINFO_DATA spDevInfoData;

	const HDEVINFO hDevInfo = SetupDiGetClassDevsA(
		classGuid,
		NULL,
		NULL,
		DIGCF_PRESENT
	);

	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return succeeded;
	}

	spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++)
	{
		DWORD type;
		LPSTR buffer = NULL;
		DWORD bufferSize = 0;
		uint32_t instance = 0;

		// get hardware ID(s) property
		while (!SetupDiGetDeviceRegistryPropertyA(hDevInfo,
			&spDevInfoData,
			SPDRP_HARDWAREID,
			&type,
			(PBYTE)buffer,
			bufferSize,
			&bufferSize))
		{
			if (GetLastError() == ERROR_INVALID_DATA)
			{
				break;
			}
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					LocalFree(buffer);
				buffer = LocalAlloc(LPTR, bufferSize);
			}
			else
			{
				goto except;
			}
		}

		if (GetLastError() == ERROR_INVALID_DATA)
			continue;

		// Find device with matching Hardware ID
		for (LPSTR p = buffer; p && *p && (p < &buffer[bufferSize]); p += lstrlenA(p) + sizeof(char))
		{
			if (!strcmp(hardwareId, p))
			{
				// multiple device instances with the same hardware ID may exist
				if (instance++ == instanceIndex)
				{
					found = true;
					break;
				}
			}
		}

		if (buffer)
			LocalFree(buffer);

		if (found && (CM_Get_DevNode_Status(&devStatus, &devProblemCode, spDevInfoData.DevInst, 0) == CR_SUCCESS))
		{
			succeeded = true;

			*status = dev_node_status_to_mty(devStatus, devProblemCode);
			break;
		}
	}

	// device not found but no enumeration error
	if (!found && GetLastError() == ERROR_SUCCESS)
	{
		*status = MTY_PNP_DEVICE_STATUS_NOT_FOUND;
		succeeded = true;
	}

except:
	err = GetLastError();
	if (hDevInfo)
		SetupDiDestroyDeviceInfoList(hDevInfo);
	SetLastError(err);

	return succeeded;
}

bool MTY_PnPDeviceInterfaceGetStatus(const GUID* interfaceGuid, uint32_t instanceIndex, MTY_PnPDeviceStatus* status)
{
	if (!interfaceGuid || !status)
		return false;

	DWORD err = ERROR_SUCCESS;
	bool found = false, succeeded = false;
	ULONG devStatus = 0, devProblemCode = 0;
	WCHAR* instanceId = NULL;
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailDataBuffer = NULL;

	*status = MTY_PNP_DEVICE_STATUS_NOT_FOUND;

	SP_DEVINFO_DATA spDevInfoData;

	const HDEVINFO hDevInfo = SetupDiGetClassDevsA(
		interfaceGuid,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
	);

	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return succeeded;
	}

	spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	DWORD requiredSize = 0;
	DWORD memberIndex = instanceIndex;
	SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
	deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

	// enumerate device instances
	while (SetupDiEnumDeviceInterfaces(
		hDevInfo,
		NULL,
		interfaceGuid,
		memberIndex++,
		&deviceInterfaceData
	))
	{
		// get required target buffer size
		SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

		if (detailDataBuffer)
		{
			free(detailDataBuffer);
			detailDataBuffer = NULL;
		}

		detailDataBuffer = calloc(requiredSize, 1);
		if (!detailDataBuffer)
			goto except;
		detailDataBuffer->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// get content
		if (SetupDiGetDeviceInterfaceDetail(
			hDevInfo,
			&deviceInterfaceData,
			detailDataBuffer,
			requiredSize,
			&requiredSize,
			NULL
		))
		{
			ULONG bytes = 0;
			DEVPROPTYPE type = 0;
			// get required property size in bytes
			CONFIGRET ret = CM_Get_Device_Interface_Property(
				detailDataBuffer->DevicePath,
				&DEVPKEY_Device_InstanceId,
				&type,
				NULL,
				&bytes,
				0
			);
			if (ret != CR_BUFFER_SMALL) {
				goto except;
			}

			instanceId = calloc(bytes, 1);

			// get property content
			ret = CM_Get_Device_Interface_Property(
				detailDataBuffer->DevicePath,
				&DEVPKEY_Device_InstanceId,
				&type,
				(PBYTE)instanceId,
				&bytes,
				0
			);
			if (ret != CR_SUCCESS) {
				goto except;
			}

			DEVINST instance = 0;
			// get instance handle from instance ID
			ret = CM_Locate_DevNode(&instance, instanceId, CM_LOCATE_DEVNODE_NORMAL);
			if (ret != CR_SUCCESS) {
				goto except;
			}

			// get node status
			ret = CM_Get_DevNode_Status(&devStatus, &devProblemCode, instance, 0);
			if (ret != CR_SUCCESS) {
				goto except;
			}

			*status = dev_node_status_to_mty(devStatus, devProblemCode);
			found = true;
			succeeded = true;
			break;
		}
	}

	// device not found but no enumeration error
	if (!found && (GetLastError() == ERROR_SUCCESS || GetLastError() == ERROR_NO_MORE_ITEMS))
	{
		*status = MTY_PNP_DEVICE_STATUS_NOT_FOUND;
		succeeded = true;
		SetLastError(ERROR_SUCCESS);
	}

except:
	if (instanceId)
		free(instanceId);
	if (detailDataBuffer)
		free(detailDataBuffer);

	err = GetLastError();
	if (hDevInfo)
		SetupDiDestroyDeviceInfoList(hDevInfo);
	SetLastError(err);

	return succeeded;
}

bool MTY_PnPDeviceDriverGetVersion(const GUID* classGuid, const char* hardwareId, uint32_t instanceIndex, uint32_t* version)
{
	if (!classGuid || !hardwareId)
		return false;

	DWORD err = ERROR_SUCCESS;
	bool found = false, succeeded = false;

	SP_DEVINFO_DATA spDevInfoData;

	const HDEVINFO hDevInfo = SetupDiGetClassDevsA(
		classGuid,
		NULL,
		NULL,
		DIGCF_PRESENT
	);

	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return succeeded;
	}

	spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++)
	{
		LPSTR buffer = NULL;
		DWORD bufferSize = 0;
		uint32_t instance = 0;

		// get hardware ID(s) property
		while (!SetupDiGetDeviceRegistryPropertyA(hDevInfo,
			&spDevInfoData,
			SPDRP_HARDWAREID,
			NULL,
			(PBYTE)buffer,
			bufferSize,
			&bufferSize))
		{
			if (GetLastError() == ERROR_INVALID_DATA)
			{
				break;
			}
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					LocalFree(buffer);
				buffer = LocalAlloc(LPTR, bufferSize);
			}
			else
			{
				goto except;
			}
		}

		if (GetLastError() == ERROR_INVALID_DATA)
			continue;

		// Find device with matching Hardware ID
		for (LPSTR p = buffer; p && *p && (p < &buffer[bufferSize]); p += lstrlenA(p) + sizeof(char))
		{
			if (!strcmp(hardwareId, p))
			{
				// multiple device instances with the same hardware ID may exist
				if (instance++ == instanceIndex)
				{
					found = true;
					break;
				}
			}
		}

		if (buffer)
			LocalFree(buffer);

		if (found)
		{
			bufferSize = 0;
			// get driver key name size
			SetupDiGetDeviceRegistryPropertyA(
				hDevInfo,
				&spDevInfoData,
				SPDRP_DRIVER,
				NULL,
				NULL,
				bufferSize,
				&bufferSize
			);

			buffer = LocalAlloc(LPTR, bufferSize);
			// get driver key name content
			if (!SetupDiGetDeviceRegistryPropertyA(
				hDevInfo,
				&spDevInfoData,
				SPDRP_DRIVER,
				NULL,
				(PBYTE)buffer,
				bufferSize,
				&bufferSize
			))
			{
				LocalFree(buffer);
				goto except;
			}

			HKEY hKey;
			CHAR subKey[MAX_PATH];
			// build driver key path
			(void)sprintf_s(subKey, MAX_PATH, "SYSTEM\\CurrentControlSet\\Control\\Class\\%s", buffer);

			LocalFree(buffer);

			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ | KEY_QUERY_VALUE, &hKey))
				goto except;

			bufferSize = 0;
			RegGetValueA(
				hKey,
				NULL,
				DRIVER_VERSION_VALUE_NAME,
				RRF_RT_REG_SZ | RRF_ZEROONFAILURE,
				NULL,
				NULL,
				&bufferSize
			);

			buffer = LocalAlloc(LPTR, bufferSize);

			if (RegGetValueA(
				hKey,
				NULL,
				DRIVER_VERSION_VALUE_NAME,
				RRF_RT_REG_SZ | RRF_ZEROONFAILURE,
				NULL,
				buffer,
				&bufferSize
			))
			{
				LocalFree(buffer);
				goto except;
			}

			uint32_t mjr = 0, mnr = 0, rev = 0, bld = 0;
			if (sscanf_s(buffer, "%u.%u.%u.%u", &mjr, &mnr, &rev, &bld) != 4)
			{
				LocalFree(buffer);
				goto except;
			}

			*version |= ((UCHAR)(bld & UCHAR_MAX)) << 0;
			*version |= ((UCHAR)(rev & UCHAR_MAX)) << 8;
			*version |= ((UCHAR)(mnr & UCHAR_MAX)) << 16;
			*version |= ((UCHAR)(mjr & UCHAR_MAX)) << 24;

			LocalFree(buffer);
			RegCloseKey(hKey);
			succeeded = true;
		}
	}

	// device not found but no enumeration error
	if (!found && GetLastError() == ERROR_SUCCESS)
	{
		succeeded = false;
	}

except:
	err = GetLastError();
	if (hDevInfo)
		SetupDiDestroyDeviceInfoList(hDevInfo);
	SetLastError(err);

	return succeeded;
}

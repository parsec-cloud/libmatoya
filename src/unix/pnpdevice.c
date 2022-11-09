#include "matoya.h"

bool MTY_PnPDeviceGetStatus(const GUID *classGuid, const char *hardwareId, uint32_t instanceIndex, MTY_PnPDeviceStatus *status)
{
    return false;
}

bool MTY_PnPDeviceInterfaceGetStatus(const GUID *interfaceGuid, uint32_t instanceIndex, MTY_PnPDeviceStatus *status)
{
    return false;
}

bool MTY_PnPDeviceDriverGetVersion(const GUID *classGuid, const char *hardwareId, uint32_t instanceIndex, uint32_t *version)
{
    return false;
}

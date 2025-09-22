#include "VRLog.h"
#include "Hooking.h"
#include "InterfaceHookInjector.h"
#include "DeviceProvider.h"

static DeviceProvider* Driver = nullptr;

static Hook<void* (*)(vr::IVRDriverContext*, const char*, vr::EVRInitError*)>
GetGenericInterfaceHook("IVRDriverContext::GetGenericInterface");

static Hook<void(*)(vr::IVRServerDriverHost*, uint32_t, const vr::DriverPose_t&, uint32_t)>
TrackedDevicePoseUpdatedHook005("IVRServerDriverHost005::TrackedDevicePoseUpdated");

static Hook<void(*)(vr::IVRServerDriverHost*, uint32_t, const vr::DriverPose_t&, uint32_t)>
TrackedDevicePoseUpdatedHook006("IVRServerDriverHost006::TrackedDevicePoseUpdated");

// Public API
void SetHmdPositionOverride(bool enabled, Vec3 spacePos)
{
    g_hmdPosOverrideEnabled = enabled;
    g_hmdPosOverride.v[0] = spacePos.x;
    g_hmdPosOverride.v[1] = spacePos.y;
    g_hmdPosOverride.v[2] = spacePos.z;
}

// Detour 005
DeviceProvider::SetDeviceTransformStruct t;
static void DetourTrackedDevicePoseUpdated005(
    vr::IVRServerDriverHost* _this,
    uint32_t unWhichDevice,
    const vr::DriverPose_t& newPose,
    uint32_t unPoseStructSize)
{
    auto pose = newPose; // copy

    if (!Driver)
    {
        if (TrackedDevicePoseUpdatedHook005.originalFunc)
            TrackedDevicePoseUpdatedHook005.originalFunc(_this, unWhichDevice, newPose, unPoseStructSize);
        else
            _this->TrackedDevicePoseUpdated(unWhichDevice, newPose, unPoseStructSize);
        return;
    }

    if (unWhichDevice == 0) {
        t.enabled = true;
        t.openVRID = 0;
        t.updateTranslation = true;
        t.updateRotation = true;
        t.updateScale = false;
        t.translation = g_hmdPosOverride;
        t.rotation = pose.qWorldFromDriverRotation;
        t.scale = 1.0;

        Driver->SetDeviceTransform(t);
    }

    Driver->HandleDevicePoseUpdated(unWhichDevice, pose);

    if (TrackedDevicePoseUpdatedHook005.originalFunc)
        TrackedDevicePoseUpdatedHook005.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
    else
        _this->TrackedDevicePoseUpdated(unWhichDevice, pose, unPoseStructSize);
}

// Detour 006
static void DetourTrackedDevicePoseUpdated006(
    vr::IVRServerDriverHost* _this,
    uint32_t unWhichDevice,
    const vr::DriverPose_t& newPose,
    uint32_t unPoseStructSize)
{
    auto pose = newPose; // copy

    if (!Driver)
    {
        if (TrackedDevicePoseUpdatedHook006.originalFunc)
            TrackedDevicePoseUpdatedHook006.originalFunc(_this, unWhichDevice, newPose, unPoseStructSize);
        else
            _this->TrackedDevicePoseUpdated(unWhichDevice, newPose, unPoseStructSize);
        return;
    }

    if (unWhichDevice == 0) {
        t.enabled = true;
        t.openVRID = 0;
        t.updateTranslation = true;
        t.updateRotation = true;
        t.updateScale = false;
        t.translation = g_hmdPosOverride;
        t.rotation = pose.qWorldFromDriverRotation;
        t.scale = 1.0;

        Driver->SetDeviceTransform(t);
    }

    Driver->HandleDevicePoseUpdated(unWhichDevice, pose);

    if (TrackedDevicePoseUpdatedHook006.originalFunc)
        TrackedDevicePoseUpdatedHook006.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
    else
        _this->TrackedDevicePoseUpdated(unWhichDevice, pose, unPoseStructSize);
}

static void* DetourGetGenericInterface(vr::IVRDriverContext* _this, const char* pchInterfaceVersion, vr::EVRInitError* peError)
{
	// MessageBoxA(nullptr, "DetourGetGenericInterface", pchInterfaceVersion, MB_OK);

	auto originalInterface = GetGenericInterfaceHook.originalFunc(_this, pchInterfaceVersion, peError);

	std::string iface(pchInterfaceVersion);
	if (iface == "IVRServerDriverHost_005")
	{
		if (!IHook::Exists(TrackedDevicePoseUpdatedHook005.name))
		{
			TrackedDevicePoseUpdatedHook005.CreateHookInObjectVTable(originalInterface, 1, &DetourTrackedDevicePoseUpdated005);
			IHook::Register(&TrackedDevicePoseUpdatedHook005);
		}
	}
	else if (iface == "IVRServerDriverHost_006")
	{
		if (!IHook::Exists(TrackedDevicePoseUpdatedHook006.name))
		{
			TrackedDevicePoseUpdatedHook006.CreateHookInObjectVTable(originalInterface, 1, &DetourTrackedDevicePoseUpdated006);
			IHook::Register(&TrackedDevicePoseUpdatedHook006);
		}
	}

	return originalInterface;
}

void InjectHooks(DeviceProvider* driver, vr::IVRDriverContext* pDriverContext)
{
	Driver = driver;

	auto err = MH_Initialize();
	if (err == MH_OK)
	{
		GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, &DetourGetGenericInterface);
		IHook::Register(&GetGenericInterfaceHook);
	}
	else
	{
		LOG("MH_Initialize error: %s", MH_StatusToString(err));
	}
}

void DisableHooks()
{
	IHook::DestroyAll();
	MH_Uninitialize();
}
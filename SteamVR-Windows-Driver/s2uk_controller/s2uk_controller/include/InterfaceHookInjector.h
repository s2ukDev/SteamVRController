#pragma once

#include <openvr_driver.h>
#include "VectorMath.h"

class DeviceProvider;

static bool g_hmdPosOverrideEnabled{ false };
static vr::HmdVector3d_t g_hmdPosOverride{ 0.0, 0.0, 0.0 };

void SetHmdPositionOverride(bool enabled, Vec3 spacePos);

void InjectHooks(DeviceProvider* driver, vr::IVRDriverContext* pDriverContext);
void DisableHooks();
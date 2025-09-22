#include "PositionalTracking.h"
#include "VRLog.h"
#include <chrono>
#include <cmath>

#include <algorithm>

// TODO: full-body tracking support.

HRESULT PositionalTrackingClass::sensorInit() {
    if (sensorInitialized) return S_FALSE;

    LOG("PositionalTrackingClass::sensorInit()");
    
    HRESULT hr = NuiGetSensorCount(&numSensors);

    if (FAILED(hr)) return hr;
    if (numSensors < 1) return E_NUI_NOTCONNECTED;

    hr = NuiCreateSensorByIndex(0, &sensor);
    if (FAILED(hr)) {
        sensorShutdown();
        return hr;
    }

    hr = sensor->NuiInitialize(
        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX
        | NUI_INITIALIZE_FLAG_USES_DEPTH
        | NUI_INITIALIZE_FLAG_USES_SKELETON);
    if (FAILED(hr)) {
        sensorShutdown();
        return hr;
    }

    hr = sensor->NuiSkeletonTrackingEnable(
        NULL,
        1     // NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT for only upper body
    );
    if (FAILED(hr)) {
        sensorShutdown();
        return hr;
    }
    sensorInitialized = true;
    return S_OK;
}

bool PositionalTrackingClass::setSensorTilt(int deg) noexcept {
    std::lock_guard lock(sensorMutex);
    if (!sensorInitialized || sensor == nullptr) return false;
    const int clamped = std::clamp(deg, -27, 27);
    HRESULT hr = sensor->NuiCameraElevationSetAngle(clamped);
    if (SUCCEEDED(hr)) {
        LOG(std::format("Changed sensor tilt to: {}deg", clamped).c_str());
        return true;
    }
    return SUCCEEDED(hr);
}

int PositionalTrackingClass::getSensorTilt() noexcept {
    if (!sensorInitialized || sensor == nullptr) return 0;

    LONG deg = 0;
    HRESULT hr = sensor->NuiCameraElevationGetAngle(&deg);
    if (FAILED(hr)) return false;

    return static_cast<int>(deg);
}

void PositionalTrackingClass::getSkeletalData() {
    if (!sensorInitialized || sensor == nullptr) return;

    NUI_SKELETON_FRAME skeletonFrame{};
    if (sensor->NuiSkeletonGetNextFrame(0, &skeletonFrame) < 0) return;

    sensor->NuiTransformSmooth(&skeletonFrame, nullptr);

    for (int z = 0; z < NUI_SKELETON_COUNT; ++z) {
        const NUI_SKELETON_DATA& skeleton = skeletonFrame.SkeletonData[z];

        if (skeleton.eTrackingState == NUI_SKELETON_TRACKED) {
            for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i) {
                if (skeleton.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_NOT_TRACKED) {
                    skeletonPosition[i].w = 0;
                }
                else {
                    skeletonPosition[i] = skeleton.SkeletonPositions[i];
                }
            }
            return;
        }
    }
}

PositionalTrackingClass::PositionalData PositionalTrackingClass::getPositionalData() {
    PositionalData outData;
    if (!sensorInitialized) return outData;

    getSkeletalData();

    auto toVec3 = [](const Vector4& v) -> Vec3 {
        return (v.w == 0.0f) ? Vec3() : Vec3(v.x, v.y, v.z);
        };

    outData.headPos = toVec3(skeletonPosition[NUI_SKELETON_POSITION_HEAD]);
    outData.leftHandPos = toVec3(skeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT]);
    outData.rightHandPos = toVec3(skeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT]);

    return outData;
}

void PositionalTrackingClass::sensorShutdown() {
    LOG("PositionalTrackingClass::sensorShutdown()");

    if (sensor)
    {
        sensor->NuiShutdown();
        sensor->Release();
        sensor = nullptr;
        sensorInitialized = false;
        numSensors = 0;
    }
}
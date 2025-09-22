#pragma once
#ifndef s2uk_positionalTracking
#define s2uk_positionalTracking

#include <Ole2.h>
#include <Windows.h>
#include "NuiApi.h"

#include "VectorMath.h"
#include <thread>
#include <mutex>

class PositionalTrackingClass {
public:
	struct PositionalData{
		Vec3 headPos;
		Vec3 leftHandPos;
		Vec3 rightHandPos;
	};
	HRESULT sensorInit();
    bool isRunning = false;

	bool isSensorInitialized() const {
		return sensorInitialized;
	}

    // Will return 0 unless setSensorTilt was ran first.
    // Not sure why. Seems to be an api limitation
    int getSensorTilt() noexcept;

    bool setSensorTilt(int deg) noexcept;

	PositionalData getPositionalData();

	void sensorShutdown();

    void showErrorMessage(HRESULT hr) {
        ShowMessageBoxA_async("Positional Tracking",
            std::format("No ready Kinect found!\n{}", GetKinectErrorMessage(hr)), MB_OK | MB_ICONERROR);
    }
private:
    void ShowMessageBoxA_async(const std::string& title, const std::string& message, const UINT& uType) {
        std::thread([title, message, uType]() {
            MessageBoxA(nullptr, message.c_str(), title.c_str(), uType);
            }).detach();
    }

    std::string GetKinectErrorMessage(HRESULT hr) {
        if (hr == S_OK) return "OK";

        LPSTR msgBuffer = nullptr;
        DWORD sz = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            hr,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&msgBuffer,
            0,
            nullptr
        );

        if (sz && msgBuffer) {
            std::string message = msgBuffer;
            LocalFree(msgBuffer);
        }

        switch (hr) {
        case S_NUI_INITIALIZING:
            return "NUI is initializing";
        case E_NUI_DEVICE_NOT_CONNECTED:
            return "Kinect device not connected";
        case E_NUI_DEVICE_NOT_READY:
            return "Kinect device not ready";
        case E_NUI_ALREADY_INITIALIZED:
            return "Kinect already initialized";
        case E_NUI_NO_MORE_ITEMS:
            return "No more items";
        case E_NUI_FRAME_NO_DATA:
            return "Frame has no data";
        case E_NUI_STREAM_NOT_ENABLED:
            return "Stream not enabled";
        case E_NUI_IMAGE_STREAM_IN_USE:
            return "Image stream in use";
        case E_NUI_FRAME_LIMIT_EXCEEDED:
            return "Frame limit exceeded";
        case E_NUI_FEATURE_NOT_INITIALIZED:
            return "Feature not initialized";
        case E_NUI_NOTGENUINE:
            return "Not genuine device";
        case E_NUI_INSUFFICIENTBANDWIDTH:
            return "Insufficient bandwidth";
        case E_NUI_NOTSUPPORTED:
            return "Not supported";
        case E_NUI_DEVICE_IN_USE:
            return "Device in use";
        case E_NUI_DATABASE_NOT_FOUND:
            return "Database not found";
        case E_NUI_DATABASE_VERSION_MISMATCH:
            return "Database version mismatch";
        case E_NUI_HARDWARE_FEATURE_UNAVAILABLE:
            return "Hardware feature unavailable";
        case E_NUI_NOTCONNECTED:
            return "Device not connected";
        case E_NUI_NOTREADY:
            return "Device not ready";
        case E_NUI_SKELETAL_ENGINE_BUSY:
            return "Skeletal engine busy";
        case E_NUI_NOTPOWERED:
            return "Device not powered";
        case E_NUI_BADINDEX:
            return "Bad index passed";
        default:
            return std::format("Unknown error 0x{:08X}", static_cast<unsigned>(hr));
        }
    }

    // bool setSensorTilt_i(int deg) noexcept;
	void getSkeletalData();

    // Sensor rotation
    std::atomic<bool> tiltInProgress{ false };
    std::mutex sensorMutex;

	// Kinect variables
	bool sensorInitialized = false;
	int numSensors = 0;
	INuiSensor* sensor;

	Vector4 skeletonPosition[NUI_SKELETON_POSITION_COUNT];
};
#endif
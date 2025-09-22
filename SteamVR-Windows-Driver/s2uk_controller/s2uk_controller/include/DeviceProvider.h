#pragma once
#include <ControllerDriver.h>
#include <openvr_driver.h>

#include "VectorMath.h"
#include "PositionalTracking.h"
#include "DriverConfig.h"

namespace TrackingEMA {
	extern Vec3EMA headFilter;
	extern Vec3EMA lhFilter;
	extern Vec3EMA rhFilter;
}

using namespace vr;

/**
This class instantiates all the device drivers you have, meaning if you've 
created multiple drivers for multiple different controllers, this class will 
create instances of each of those and inform OpenVR about all of your devices.

Take a look at the comment blocks for all the methods in IServerTrackedDeviceProvider
too.
**/
class DeviceProvider : public IServerTrackedDeviceProvider
{
public:
	/**
	Initiailze and add your drivers to OpenVR here.
	**/
	EVRInitError Init(IVRDriverContext* pDriverContext);


	//void GetSensorData();
	/**
	Called right before your driver is unloaded.
	**/
	void Cleanup();

	/**
	Returns version of the openVR interface this driver works with.
	**/
	const char* const* GetInterfaceVersions();

	/**
	Called every frame. Update your drivers here.
	**/
	void RunFrame();

	/**
	Return true if standby mode should be blocked. False otherwise.
	**/
	bool ShouldBlockStandbyMode();

	/**
	Called when OpenVR goes into stand-by mode, so you can tell your devices to go into stand-by mode
	**/
	void EnterStandby();

	/**
	Called when OpenVR leaves stand-by mode.
	**/
	void LeaveStandby();


	// End of generic OPENVR_API defenitions.

	struct SetDeviceTransformStruct
	{
		uint32_t openVRID;
		bool enabled;
		bool updateTranslation;
		bool updateRotation;
		bool updateScale;
		vr::HmdVector3d_t translation;
		vr::HmdQuaternion_t rotation;
		double scale;
	};

	void SetDeviceTransform(const SetDeviceTransformStruct& newTransform);
	bool HandleDevicePoseUpdated(uint32_t openVRID, vr::DriverPose_t& pose);


private:
	ControllerDriver* controllerDriverL = nullptr;
	ControllerDriver* controllerDriverR = nullptr;

	struct DeviceTransform
	{
		bool enabled = false;
		vr::HmdVector3d_t translation;
		vr::HmdQuaternion_t rotation;
		double scale;
	};

	DeviceTransform transforms[vr::k_unMaxTrackedDeviceCount];
};
#pragma once
// #include "SocketClass.h"
#include <openvr_driver.h>
#include <windows.h>
#include <chrono>

#include "BufferCompression.h"


using namespace vr;

/**
This class controls the behavior of the controller. This is where you 
tell OpenVR what your controller has (buttons, joystick, trackpad, etc.).
This is also where you inform OpenVR when the state of your controller 
changes (for example, a button is pressed).

For the methods, take a look at the comment blocks for the ITrackedDeviceServerDriver 
class too. Those comment blocks have some good information.

This example driver will simulate a controller that has a joystick and trackpad on it.
It is hardcoded to just return a value for the joystick and trackpad. It will cause 
the game character to move forward constantly.
**/
class ControllerDriver : public ITrackedDeviceServerDriver {
public:
	void SetControllerIndex(int32_t CtrlIndex);
	/**
	Initialize your controller here. Give OpenVR information 
	about your controller and set up handles to inform OpenVR when 
	the controller state changes.
	**/
	EVRInitError Activate(uint32_t unObjectId);

	/**
	Un-initialize your controller here.
	**/
	void Deactivate();

	/**
	Tell your hardware to go into stand-by mode (low-power).
	**/
	void EnterStandby();

	/**
	Take a look at the comment block for this method on ITrackedDeviceServerDriver. So as far 
	as I understand, driver classes like this one can implement lots of functionality that 
	can be categorized into components. This class just acts as an input device, so it will 
	return the IVRDriverInput class, but it could return other component classes if it had 
	more functionality, such as maybe overlays or UI functionality.
	**/
	void* GetComponent(const char* pchComponentNameAndVersion);

	/**
	Refer to ITrackedDeviceServerDriver. I think it sums up what this does well.
	**/
	void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize);

	/**
	Returns the Pose for your device. Pose is an object that contains the position, rotation, velocity, 
	and angular velocity of your device.
	**/
	DriverPose_t GetPose();

	/**
	You can retrieve the state of your device here and update OpenVR if anything has changed. This 
	method should be called every frame.
	**/
	void RunFrame();

	void ReadBuffer(BufferCompression::ControllerState state);
private:
	struct ControllerData {
		// Position
		Vec3 position = Vec3(0, 0, 0);

		// Rotation
		Quaternion controllerRotation{ 0, 0, 0, 0 };

		// Timing
		std::chrono::high_resolution_clock::time_point lastScalarValueUpdate;
		float deltaTime = 0.0f;

		// Joystick
		bool joystickThumbrest = false;
		bool joystickTouch = false;
		bool joystickClick = false;
		float joystickX = 0.0f;
		float joystickY = 0.0f;

		// Buttons
		bool btnX = false;
		bool btnY = false;
		bool btnA = false;
		bool btnB = false;
		bool btnSystem = false;

		// Trigger / Grip
		float triggerState = 0.0f;
		float gripState = 0.0f;
		float triggerStateRaw = 0.0f;
		float gripStateRaw = 0.0f;

		// Power info
		bool isCharging = false;
		double batteryPercentage = 1.0;
	};

	vr::VREvent_t vrEvent;
	DriverPose_t pose;
	int32_t ControllerIndex;
	uint32_t driverId;
	VRInputComponentHandle_t thumbrestHandle;
	VRInputComponentHandle_t joystickTouchHandle;
	VRInputComponentHandle_t joystickClickHandle;
	VRInputComponentHandle_t joystickYHandle;
	VRInputComponentHandle_t trackpadYHandle;
	VRInputComponentHandle_t joystickXHandle;
	VRInputComponentHandle_t trackpadXHandle;
	VRInputComponentHandle_t TriggerHandle;
	VRInputComponentHandle_t TriggerTouchHandle;
	VRInputComponentHandle_t GripHandle;
	VRInputComponentHandle_t GripTouchHandle;
	VRInputComponentHandle_t AHandle;
	VRInputComponentHandle_t BHandle;
	VRInputComponentHandle_t XHandle;
	VRInputComponentHandle_t YHandle;
	VRInputComponentHandle_t SystemHandle;

	VRInputComponentHandle_t hapticHandleL;
	VRInputComponentHandle_t hapticHandleR;

	ControllerData controllerData;
};
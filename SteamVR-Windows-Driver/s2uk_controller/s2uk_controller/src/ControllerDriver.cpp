#include <TcpServer.h>
#include <ControllerDriver.h>
#include <DeviceProvider.h>
#include <InterfaceHookInjector.h>
#include <VRLog.h>
#include <thread>

#include <chrono>
#include <sstream>
#include "BufferCompression.h"
#include "PositionalTracking.h"
#include "DriverConfig.h"


extern TcpSocketClass* tcpSocketObj;
extern PositionalTrackingClass* posTrackingObj;
extern PositionalTrackingClass::PositionalData posDataEMA;
extern DriverConfig* driverConfigObj;

extern Vec3EMA TrackingEMA::headFilter;
extern Vec3EMA TrackingEMA::lhFilter;
extern Vec3EMA TrackingEMA::rhFilter;

auto map2StateVar = [](int s) -> float {
	return (s == 1) ? 1.0f : (s == 2) ? 0.5f : 0.0f;
	};

constexpr float lerpSpeed = 700.0f;
auto lerp = [](float current, float target, float deltaTime, float speed) -> float {
	float t = 1.0f - std::exp(-speed * deltaTime);
	return current + (target - current) * t;
	};

void ControllerDriver::ReadBuffer(BufferCompression::ControllerState state) {
	if (state.left_controller == ((ControllerIndex==1) ? true : false)) {
		try {
			controllerData.position = (static_cast<bool>(ControllerIndex) == state.left_controller) // static_cast<int32_t --> bool>
				? posDataEMA.leftHandPos
				: posDataEMA.rightHandPos;

			controllerData.isCharging = state.controller_battery_plugged;
			controllerData.batteryPercentage = state.batteryPercentage;

			controllerData.controllerRotation = s2uk_vecMath::eulerToQuaternion(state.gyro);

			controllerData.triggerStateRaw = map2StateVar(state.trigger_state);
			controllerData.gripStateRaw = map2StateVar(state.grip_state);
			controllerData.triggerState = lerp(controllerData.triggerState, controllerData.triggerStateRaw, controllerData.deltaTime, lerpSpeed);
			controllerData.gripState = lerp(controllerData.gripState, controllerData.gripStateRaw, controllerData.deltaTime, lerpSpeed);

			if (controllerData.triggerState < 0.01f) { controllerData.triggerState = 0.0f; }
			if (controllerData.gripState < 0.01f) { controllerData.gripState = 0.0f; }

			controllerData.joystickClick = state.joy_state == 2;
			controllerData.joystickTouch = state.joy_state == 1;

			controllerData.joystickThumbrest = state.joy_state != 0;

			controllerData.joystickX = (state.joy.x != 0 && !state.joy_in_dz) ? state.joy.x : 0;
			controllerData.joystickY = (state.joy.y != 0 && !state.joy_in_dz) ? state.joy.y : 0;

			controllerData.btnA = state.btn_a_or_x_state;
			controllerData.btnX = state.btn_a_or_x_state;

			controllerData.btnB = state.btn_b_or_y_state;
			controllerData.btnY = state.btn_b_or_y_state;

			// TODO: Holding the System button down on right controller must recenter you, just like in Steam link.
			// not sure how to implement
			controllerData.btnSystem = state.btn_system_or_menu_state;
		}
		catch (...) {
	
		}
	}
}

void ControllerDriver::SetControllerIndex(int32_t CtrlIndex)
{
	ControllerIndex = CtrlIndex;
}

EVRInitError ControllerDriver::Activate(uint32_t unObjectId)
{
	pose = { 0 };

	driverId = unObjectId;

	PropertyContainerHandle_t props = VRProperties()->TrackedDeviceToPropertyContainer(driverId);

	VRProperties()->SetInt32Property(props, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller);

	VRProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{s2ukController}/input/touch_plus_profile.json");
	VRProperties()->SetStringProperty(props, vr::Prop_ManufacturerName_String, "oculus");

	switch (ControllerIndex) {
	case 1:
		vr::VRProperties()->SetInt32Property(props, Prop_ControllerRoleHint_Int32, TrackedControllerRole_LeftHand);
		VRProperties()->SetInt32Property(props, vr::Prop_ControllerHandSelectionPriority_Int32, 10001);
		break;
	case 2:
		vr::VRProperties()->SetInt32Property(props, Prop_ControllerRoleHint_Int32, TrackedControllerRole_RightHand);
		VRProperties()->SetInt32Property(props, vr::Prop_ControllerHandSelectionPriority_Int32, 10000);
		break;
	}

	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/thumbrest/touch", &thumbrestHandle);
	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/joystick/touch", &joystickTouchHandle);
	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/joystick/click", &joystickClickHandle);
	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/grip/touch", &GripTouchHandle);
	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/trigger/touch", &TriggerTouchHandle);
	vr::VRDriverInput()->CreateBooleanComponent(props, "/input/system/click", &SystemHandle);

	vr::VRDriverInput()->CreateScalarComponent(
		props, "/input/trigger/value", &TriggerHandle,
		vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided
	);

	vr::VRDriverInput()->CreateScalarComponent(
		props, "/input/grip/value", &GripHandle,
		vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided
	);

	VRDriverInput()->CreateScalarComponent(props, "/input/joystick/y", &joystickYHandle, EVRScalarType::VRScalarType_Absolute,
		EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
	
	VRDriverInput()->CreateScalarComponent(props, "/input/joystick/x", &joystickXHandle, EVRScalarType::VRScalarType_Absolute,
		EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

	vr::VRProperties()->SetBoolProperty(props, vr::Prop_DeviceProvidesBatteryStatus_Bool, true);

	switch (ControllerIndex) {
	case 1:
		vr::VRProperties()->SetFloatProperty(props, vr::Prop_DeviceBatteryPercentage_Float, static_cast<float>(controllerData.batteryPercentage));
		vr::VRProperties()->SetBoolProperty(props, vr::Prop_DeviceIsCharging_Bool, controllerData.isCharging);

		vr::VRDriverInput()->CreateHapticComponent(props, "/output/haptic", &hapticHandleL);

		vr::VRDriverInput()->CreateBooleanComponent(props, "/input/x/click", &XHandle);
		vr::VRDriverInput()->CreateBooleanComponent(props, "/input/y/click", &YHandle);

		vr::VRProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Meta Touch Plus (Left Controller)");
		vr::VRProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_quest_plus_controller_left");
		vr::VRProperties()->SetStringProperty(props, vr::Prop_RegisteredDeviceType_String, "oculus/WMHD315M3114GV_Controller_Left");

		//set tracking image
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons/rifts_left_controller_ready.png");

		//set all other tracking images
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/rifts_left_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/rifts_left_controller_searching.gif");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/rifts_left_controller_searching_alert.gif");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/rifts_left_controller_ready_alert.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/rifts_left_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/rifts_left_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/rifts_left_controller_ready_low.png");
		break;
	case 2:
		vr::VRProperties()->SetFloatProperty(props, vr::Prop_DeviceBatteryPercentage_Float, static_cast<float>(controllerData.batteryPercentage));
		vr::VRProperties()->SetBoolProperty(props, vr::Prop_DeviceIsCharging_Bool, controllerData.isCharging);

		vr::VRDriverInput()->CreateHapticComponent(props, "/output/haptic", &hapticHandleR);

		vr::VRDriverInput()->CreateBooleanComponent(props, "/input/a/click", &AHandle);
		vr::VRDriverInput()->CreateBooleanComponent(props, "/input/b/click", &BHandle);

		vr::VRProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Meta Touch Plus (Right Controller)");
		vr::VRProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_quest_plus_controller_right"); 
		vr::VRProperties()->SetStringProperty(props, vr::Prop_RegisteredDeviceType_String, "oculus/WMHD315M3819GV_Controller_Left");

		//set tracking image
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons/rifts_right_controller_ready.png");

		//set all other tracking images
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/rifts_right_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/rifts_right_controller_searching.gif");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/rifts_right_controller_searching_alert.gif");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/rifts_right_controller_ready_alert.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/rifts_right_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/rifts_right_controller_off.png");
		VRProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/rifts_right_controller_ready_low.png");

		break;
	}

	return VRInitError_None;
}

DriverPose_t ControllerDriver::GetPose()
{
	pose.deviceIsConnected = true;
	pose.poseIsValid = true;
	pose.result = true ? vr::ETrackingResult::TrackingResult_Running_OK : vr::ETrackingResult::TrackingResult_Running_OutOfRange;
	pose.willDriftInYaw = false;
	pose.shouldApplyHeadModel = false;
	pose.qDriverFromHeadRotation.w = pose.qWorldFromDriverRotation.w = pose.qRotation.w = 1.0;

	pose.qRotation = std::bit_cast<HmdQuaternion_t>(controllerData.controllerRotation);

	pose.vecPosition[0] = controllerData.position.x; // right
	pose.vecPosition[1] = controllerData.position.y; // up
	pose.vecPosition[2] = controllerData.position.z; // forward

	return pose;
}

void ControllerDriver::RunFrame()
{
	PropertyContainerHandle_t props = VRProperties()->TrackedDeviceToPropertyContainer(driverId);

	auto now = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> elapsed = now - controllerData.lastScalarValueUpdate;
	controllerData.deltaTime = elapsed.count();
	controllerData.lastScalarValueUpdate = now;

	VRServerDriverHost()->TrackedDevicePoseUpdated(this->driverId, GetPose(), sizeof(vr::DriverPose_t));
	VRDriverInput()->UpdateScalarComponent(GripHandle, controllerData.gripState, 0);
	VRDriverInput()->UpdateBooleanComponent(GripTouchHandle, controllerData.gripState > 0.95f, 0);
	VRDriverInput()->UpdateScalarComponent(TriggerHandle, controllerData.triggerState, 0);
	VRDriverInput()->UpdateBooleanComponent(TriggerTouchHandle, controllerData.triggerState > 0.95f, 0);
	VRDriverInput()->UpdateBooleanComponent(joystickTouchHandle, controllerData.joystickTouch, 0);
	VRDriverInput()->UpdateBooleanComponent(joystickClickHandle, controllerData.joystickClick, 0);
	VRDriverInput()->UpdateBooleanComponent(thumbrestHandle, controllerData.joystickThumbrest, 0);
	VRDriverInput()->UpdateScalarComponent(joystickXHandle, controllerData.joystickX, 0);
	VRDriverInput()->UpdateScalarComponent(joystickYHandle, controllerData.joystickY, 0);
	VRDriverInput()->UpdateBooleanComponent(SystemHandle, controllerData.btnSystem, 0);

	vr::VRProperties()->SetFloatProperty(props, vr::Prop_DeviceBatteryPercentage_Float, static_cast<float>(controllerData.batteryPercentage));
	vr::VRProperties()->SetBoolProperty(props, vr::Prop_DeviceIsCharging_Bool, controllerData.isCharging);

	while (vr::VRServerDriverHost()->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
		if (vrEvent.eventType == vr::VREvent_EnterStandbyMode &&
			vrEvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) {
			if (posTrackingObj->isSensorInitialized())
				posTrackingObj->sensorShutdown();
			LOG("HMD entered standby mode.");
		}

		if (vrEvent.eventType == vr::VREvent_LeaveStandbyMode &&
			vrEvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) {
			LOG("HMD left standby mode.");
			if (!posTrackingObj->isSensorInitialized()) {
				HRESULT hr = posTrackingObj->sensorInit();
				if (FAILED(hr)) posTrackingObj->showErrorMessage(hr);

				if (!driverConfigObj->readConfig()) driverConfigObj->createConfig();

				if (driverConfigObj->getConfig().sensorTilt != posTrackingObj->getSensorTilt()) {
					posTrackingObj->setSensorTilt(driverConfigObj->getConfig().sensorTilt);
				}
				
				TrackingEMA::headFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().headEMA));
				TrackingEMA::lhFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().leftHandEMA));
				TrackingEMA::rhFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().rightHandEMA));
			}
		}

		if (vrEvent.eventType != vr::VREvent_Input_HapticVibration)
			continue;

		if (vrEvent.data.hapticVibration.componentHandle == hapticHandleL) {
			// LOG("hapticEvent_L");
			std::string result = BufferCompression::compressResponseData(true,
				vrEvent.data.hapticVibration.fAmplitude,
				vrEvent.data.hapticVibration.fFrequency,
				vrEvent.data.hapticVibration.fDurationSeconds);
			tcpSocketObj->broadcastMessage(result.c_str());
		}
		if (vrEvent.data.hapticVibration.componentHandle != hapticHandleL) {
			// LOG("hapticEvent_R");
			std::string result = BufferCompression::compressResponseData(false,
				vrEvent.data.hapticVibration.fAmplitude,
				vrEvent.data.hapticVibration.fFrequency,
				vrEvent.data.hapticVibration.fDurationSeconds);
			tcpSocketObj->broadcastMessage(result.c_str());
		}
	}

	switch (ControllerIndex) {
	case 1:
		// left
		vr::VRDriverInput()->UpdateBooleanComponent(XHandle, controllerData.btnX == 1, 0);
		vr::VRDriverInput()->UpdateBooleanComponent(YHandle, controllerData.btnY == 1, 0);
		break;
	case 2:
		// right
		vr::VRDriverInput()->UpdateBooleanComponent(AHandle, controllerData.btnA == 1, 0);
		vr::VRDriverInput()->UpdateBooleanComponent(BHandle, controllerData.btnB == 1, 0);
		break;
	}
}

void ControllerDriver::Deactivate()
{
	driverId = k_unTrackedDeviceIndexInvalid;
}

void* ControllerDriver::GetComponent(const char* pchComponentNameAndVersion)
{
	// Always returns null.
	if (strcmp(IVRDriverInput_Version, pchComponentNameAndVersion) == 0)
	{
		return this;
	}
	return NULL;
}

void ControllerDriver::EnterStandby() {}

void ControllerDriver::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) 
{
	if (unResponseBufferSize >= 1)
	{
		pchResponseBuffer[0] = 0;
	}
}
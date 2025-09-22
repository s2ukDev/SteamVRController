#include "TcpServer.h"
#include <DeviceProvider.h>
#include <thread>
#include "InterfaceHookInjector.h"
#include "VRLog.h"

#include "BufferCompression.h"
#include "PositionalTracking.h"
#include "DriverConfig.h"

void GetSensorData(TcpSocketClass* tcpSocketObject, ControllerDriver* left, ControllerDriver* right);

void GetPositionalData(PositionalTrackingClass* posTrackingObject);
void InterpolatePositionalData(PositionalTrackingClass* posTrackingObject);

TcpSocketClass* tcpSocketObj;
PositionalTrackingClass* posTrackingObj;
DriverConfig* driverConfigObj;

PositionalTrackingClass::PositionalData posDataRaw;
PositionalTrackingClass::PositionalData posDataEMA;

Vec3EMA TrackingEMA::headFilter{ 0.3f };
Vec3EMA TrackingEMA::lhFilter{ 0.3f };
Vec3EMA TrackingEMA::rhFilter{ 0.3f };

EVRInitError DeviceProvider::Init(IVRDriverContext* pDriverContext)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
    LOG("DeviceProvider::Init()");

    memset(transforms, 0, vr::k_unMaxTrackedDeviceCount * sizeof DeviceTransform);

    InjectHooks(this, pDriverContext);
    SetHmdPositionOverride(true, Vec3(0, 1.8, 0));

    EVRInitError initError = InitServerDriverContext(pDriverContext);
    if (initError != EVRInitError::VRInitError_None)
    {
        return initError;
    }

    controllerDriverR = new ControllerDriver();
    controllerDriverR->SetControllerIndex(2);
    VRServerDriverHost()->TrackedDeviceAdded("WMHD315M3819GV", TrackedDeviceClass_Controller, controllerDriverR);

    controllerDriverL = new ControllerDriver();
    controllerDriverL->SetControllerIndex(1);
    VRServerDriverHost()->TrackedDeviceAdded("WMHD315M3114GV", TrackedDeviceClass_Controller, controllerDriverL);

    tcpSocketObj = new TcpSocketClass();
    std::thread getSensorDatathread(GetSensorData, tcpSocketObj,controllerDriverL,controllerDriverR);
    getSensorDatathread.detach();

    posTrackingObj = new PositionalTrackingClass();
    HRESULT hr = posTrackingObj->sensorInit();

    driverConfigObj = new DriverConfig();
    
    posTrackingObj->setSensorTilt(driverConfigObj->getConfig().sensorTilt);
    TrackingEMA::headFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().headEMA));
    TrackingEMA::lhFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().leftHandEMA));
    TrackingEMA::rhFilter.setAlpha(static_cast<float>(driverConfigObj->getConfig().rightHandEMA));

    if (FAILED(hr)) posTrackingObj->showErrorMessage(hr);
    posTrackingObj->isRunning = true;

    std::thread getPositionalDatathread(GetPositionalData, posTrackingObj);
    getPositionalDatathread.detach();

    std::thread interpolatePositionalDatathread(InterpolatePositionalData, posTrackingObj);
    interpolatePositionalDatathread.detach();

    return vr::VRInitError_None;
}

void DeviceProvider::Cleanup()
{
    LOG("DeviceProvider::Cleanup()");

    posTrackingObj->isRunning = false;
    posTrackingObj->sensorShutdown();

    tcpSocketObj->CloseSocket();
    delete tcpSocketObj;
    delete controllerDriverL;
    delete controllerDriverR;
    controllerDriverL = NULL;
    controllerDriverR = NULL;

    DisableHooks();
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

void GetSensorData(TcpSocketClass* SocketObject,ControllerDriver* left, ControllerDriver* right) {
    SocketObject->Connect(9775);
    while (SocketObject->GetStatus()) {
        char buffer[2048];
        SocketObject->Receive(buffer);
        BufferCompression::ControllerState controllerState = BufferCompression::decryptControllerState(std::string(buffer));

        // std::ostringstream oss;
        // oss << std::boolalpha;
        // oss << "left_controller: " << controllerState.left_controller << "\n";
        // oss << "btn_system_or_menu_state: " << controllerState.btn_system_or_menu_state << "\n";
        // oss << "btn_a_or_x_state: " << controllerState.btn_a_or_x_state << "\n";
        // oss << "btn_b_or_y_state: " << controllerState.btn_b_or_y_state << "\n";
        // oss << "controller_battery_plugged: " << controllerState.controller_battery_plugged << "\n";
        // oss << "joy_in_dz: " << controllerState.joy_in_dz << "\n";
        // oss << "trigger_state: " << +controllerState.trigger_state << "\n";
        // oss << "grip_state: " << +controllerState.grip_state << "\n";
        // oss << "joy_state: " << +controllerState.joy_state << "\n";
        // oss << "batteryPercentage: " << +controllerState.batteryPercentage << "\n";
        // oss << "gyro: " << controllerState.gyro.x << ", " << controllerState.gyro.y << ", " << controllerState.gyro.z << "\n";
        // oss << "joy:  " << controllerState.joy.x << ", " << controllerState.joy.y << "\n";
        // LOG(oss.str().c_str());
        
        left->ReadBuffer(controllerState);
        right->ReadBuffer(controllerState);
    }
}

void GetPositionalData(PositionalTrackingClass* posTrackingObject) {
    while (posTrackingObject->isRunning) {
        if (posTrackingObject->isSensorInitialized()) {
            posDataRaw = posTrackingObject->getPositionalData();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }
}

void InterpolatePositionalData(PositionalTrackingClass* posTrackingObject) {
    while (posTrackingObject->isRunning) {
        auto poseDataCopy = posDataRaw;
        posDataEMA = poseDataCopy;

        posDataEMA.headPos = TrackingEMA::headFilter.update(poseDataCopy.headPos);
        posDataEMA.leftHandPos = TrackingEMA::lhFilter.update(poseDataCopy.leftHandPos);
        posDataEMA.rightHandPos = TrackingEMA::rhFilter.update(poseDataCopy.rightHandPos);

        SetHmdPositionOverride(true, posDataEMA.headPos);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

inline static vr::HmdQuaternion_t operator*(const vr::HmdQuaternion_t& lhs, const vr::HmdQuaternion_t& rhs) {
    return {
        (lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z),
        (lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.w * rhs.y) + (lhs.y * rhs.w) + (lhs.z * rhs.x) - (lhs.x * rhs.z),
        (lhs.w * rhs.z) + (lhs.z * rhs.w) + (lhs.x * rhs.y) - (lhs.y * rhs.x)
    };
}

inline static vr::HmdVector3d_t quaternionRotateVector(const vr::HmdQuaternion_t& quat, const double(&vector)[3]) {
    vr::HmdQuaternion_t vectorQuat = { 0.0, vector[0], vector[1] , vector[2] };
    vr::HmdQuaternion_t conjugate = { quat.w, -quat.x, -quat.y, -quat.z };
    auto rotatedVectorQuat = quat * vectorQuat * conjugate;
    return { rotatedVectorQuat.x, rotatedVectorQuat.y, rotatedVectorQuat.z };
}

void DeviceProvider::SetDeviceTransform(const SetDeviceTransformStruct& newTransform)
{
    auto& tf = transforms[newTransform.openVRID];
    tf.enabled = newTransform.enabled;

    if (newTransform.updateTranslation)
        tf.translation = newTransform.translation;

    if (newTransform.updateRotation)
        tf.rotation = newTransform.rotation;

    if (newTransform.updateScale)
        tf.scale = newTransform.scale;
}

bool DeviceProvider::HandleDevicePoseUpdated(uint32_t openVRID, vr::DriverPose_t& pose)
{
    auto& tf = transforms[openVRID];
    if (tf.enabled)
    {
        pose.qWorldFromDriverRotation = tf.rotation * pose.qWorldFromDriverRotation;

        pose.vecPosition[0] *= tf.scale;
        pose.vecPosition[1] *= tf.scale;
        pose.vecPosition[2] *= tf.scale;

        vr::HmdVector3d_t rotatedTranslation = quaternionRotateVector(tf.rotation, pose.vecWorldFromDriverTranslation);
        pose.vecWorldFromDriverTranslation[0] = rotatedTranslation.v[0] + tf.translation.v[0];
        pose.vecWorldFromDriverTranslation[1] = rotatedTranslation.v[1] + tf.translation.v[1];
        pose.vecWorldFromDriverTranslation[2] = rotatedTranslation.v[2] + tf.translation.v[2];
    }
    return true;
}

const char* const* DeviceProvider::GetInterfaceVersions()
{
    return k_InterfaceVersions;
}

void DeviceProvider::RunFrame()
{
    controllerDriverL->RunFrame();
    controllerDriverR->RunFrame();
}

bool DeviceProvider::ShouldBlockStandbyMode()
{
    return false;
}

void DeviceProvider::EnterStandby() {
    // unstable
    // moved to ControllerDriver VRServerDriverHost()->PullNextEvent()
    // vr::VRDriverLog()->Log("SteamVR has entered standby mode.");
}

void DeviceProvider::LeaveStandby() {
    // unstable, gets called twice, sometimes.
    // moved to ControllerDriver VRServerDriverHost()->PullNextEvent()
    // LOG("SteamVR has left the standby mode.");
}
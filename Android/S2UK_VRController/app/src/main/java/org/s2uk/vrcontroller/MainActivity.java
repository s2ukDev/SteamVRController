package org.s2uk.vrcontroller;

import static android.content.ContentValues.TAG;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.provider.Settings;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import org.s2uk.vrcontroller.databinding.ActivityMainBinding;

import java.text.MessageFormat;

public class MainActivity extends AppCompatActivity implements SensorEventListener, BatteryLevelListener {

    // Used to load the 'vr-controller-cpp' library on application startup.
    static {
        System.loadLibrary("vr-controller-cpp");
    }

    // Pairing data
    private TcpClient tcpClient;
    private String serverIP = "0.0.0.0";
    private String debugTCPConnectionStatus = "";
    private String tcpCompressedPacket = "";
    private ServerMessages inMessages = null;

    // Handlers
    private final Handler mainUpdateHandler = new Handler(Looper.getMainLooper());
    private final long UPDATE_INTERVAL_MS = 11; // ~90 FPS
    private final long HAPTIC_UPDATE_INTERVAL_MS = 1; // ~1000 FPS

    // Sensor data
    private SensorManager sensorManager;
    private Sensor gyroSensor;
    private SVec3 gyroAngle = new SVec3(0,0,0); // NOTE: Data format is (yaw, pitch, roll)
    private SVec3 gyroAngleRaw = new SVec3(0,0,0); // in Radians!
    private SVec3 gyroOffset = new SVec3(0,0,0); // in Radians!
    private SVec3 finalGyroAngle = new SVec3(0,0,0); // this gets sent to the server

    private final SVec3 leftControllerAngleCorrection = new SVec3(270f, -180f, 90f);
    private final SVec3 rightControllerAngleCorrection = new SVec3(90f, 0f, 90f);

    private SVec2 joyData = new SVec2(0.0f, 0.0f);
    private boolean joyInDZ;

    // UI elements
    private Button btnRecenter;
    private Button btnSwitchHand;
    private TextView debugConnectionStatusView;
    private TextView debugInfoView;

    // Controller Type
    private Boolean isLeftController = false;

    // Right Controller
    private JoystickView joyR;
    private ImageButton btnSystem;
    private ImageButton btnA;
    private ImageButton btnB;

    // Left Controller
    private JoystickView joyL;
    private ImageButton btnMenu;
    private ImageButton btnX;
    private ImageButton btnY;

    // Button States
    private static final long SIDE_BUTTON_SWITCH_THRESHOLD = 175; // ms
    private static final long JOY_SWITCH_THRESHOLD = 100; // ms
    private long lastTriggerPress = 0;
    private long lastGripPress = 0;
    private int triggerState = 0; // volume down
    private int gripState = 0;    // volume up
    private int joyState = 0; // from UI
    private boolean btnSystemOrMenuState = false; // from UI
    private boolean btnA_or_X_State = false; // from UI
    private boolean btnB_or_Y_State = false; // from UI

    // Battery
    private float controllerBatteryPercentage = 0.0f;
    private boolean controllerBatteryPlugged = false;
    private final BatteryMonitor bMonitor = new BatteryMonitor(this);

    @Override
    public void onBatteryLevelChanged(float batteryPercentage, boolean isPlugged) {
        Log.d(TAG, "Battery level changed: " + batteryPercentage + ", isPlugged in? :" + isPlugged);
        controllerBatteryPercentage = batteryPercentage;
        controllerBatteryPlugged = isPlugged;
    }

    @SuppressLint({"SourceLockedOrientationActivity", "ClickableViewAccessibility"})
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Get Controller Type
        SharedPreferences preferences = this.getSharedPreferences(IpConnectDialog.CONNECTION_PREFERENCES, Context.MODE_PRIVATE);
        isLeftController = preferences.getBoolean(IpConnectDialog.KEY_IS_LEFT, false);
        Log.i("VRController Init", MessageFormat.format(
                "Retrieved preference \"isLeftController\": {0}.", isLeftController));

        debugTCPConnectionStatus = getString(R.string.tcp_client_status_no_init);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        WindowInsetsControllerCompat insetsController =
                WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());

        insetsController.hide(WindowInsetsCompat.Type.systemBars());
        insetsController.setSystemBarsBehavior(
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);

        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(params);

        ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // Assign UI elements
        debugInfoView = binding.debugInfo;
        debugConnectionStatusView = binding.connectionStatusTextView;
        btnRecenter = binding.recenterBtn;
        btnSwitchHand = binding.connectToPCbtn;

        joyR = binding.controllerJoystickR;
        btnSystem = binding.controllerButtonSystem;
        btnA = binding.controllerButtonA;
        btnB = binding.controllerButtonB;

        joyL = binding.controllerJoystickL;
        btnMenu = binding.controllerButtonMenu;
        btnX = binding.controllerButtonX;
        btnY = binding.controllerButtonY;

        processControllerChange(isLeftController);

        // Init Gyro
        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        gyroSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);

        if (gyroSensor == null) {
            Toast.makeText(this, "No gyro found!", Toast.LENGTH_LONG).show();
        }

        // System Helpers
        btnRecenter.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        v.setPressed(true);
                        buttonFeedback(1);
                        gyroOffset.x = gyroAngleRaw.x;
                        gyroOffset.y = gyroAngleRaw.y;
                        gyroOffset.z = gyroAngleRaw.z;
                        Log.d("VRControllerRecenter", MessageFormat.format("Controller recentered with offset: {0}.", gyroOffset.toString()));
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        v.setPressed(false);
                        return true;
                }
                return false;
            }
        });

        btnSwitchHand.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        v.setPressed(true);
                        buttonFeedback(1);
                        IpConnectDialog.show(v.getContext(), new IpConnectDialog.ipConnectListener() {
                            @Override
                            public void onIpEntered(String ip, boolean isLeftHand) {
                                serverIP = ip;
                                isLeftController = isLeftHand;
                                processControllerChange(isLeftController);

                                if (tcpClient != null) {
                                    tcpClient.stopClient();
                                    inMessages.clear();
                                    tcpClient = null;
                                }

                                inMessages = new ServerMessages();
                                tcpClient = new TcpClient(
                                        serverIP,
                                        getApplicationContext(),
                                        inMessages,
                                        message -> {
                                            Log.w("TcpClientMainActivity", "Received from server:" + message);
                                            },
                                        status -> {
                                            debugTCPConnectionStatus = status;
                                        });
                                new Thread(tcpClient::run, "TcpClientThread").start();
                            }

                            @Override
                            public void onDisconnect(boolean isLeftHand) {
                                processControllerChange(isLeftHand);
                                if (tcpClient != null) {
                                    tcpClient.stopClient();
                                    inMessages.clear();
                                    tcpClient = null;
                                    inMessages = null;
                                }
                            }

                            @Override
                            public void onCancelled() {

                            }
                        });
                        return true;

                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        v.setPressed(false);
                        return true;
                }
                return false;
            }
        });
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == 100) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "Permission granted!", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, R.string.read_storage_permission, Toast.LENGTH_LONG).show();

                if (!ActivityCompat.shouldShowRequestPermissionRationale(
                        this, Manifest.permission.READ_EXTERNAL_STORAGE)) {

                    Intent intent;
                    intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                }
                finish();
            }
        }
    }

    private final Runnable updateRunnable = new Runnable() {
        @Override
        public void run() {
            hapticUpdate();
            mainUpdateHandler.postDelayed(this, UPDATE_INTERVAL_MS);
        }
    };

    private final Runnable hapticRunnable = new Runnable() {
        @Override
        public void run() {
            update();
            mainUpdateHandler.postDelayed(this, UPDATE_INTERVAL_MS);
        }
    };

    @Override
    protected void onResume() {
        super.onResume();
        joyInDZ = isJoyInDZ(joyData);
        bMonitor.startMonitoring(this);
        mainUpdateHandler.post(updateRunnable);
        mainUpdateHandler.post(hapticRunnable);

        if (gyroSensor != null) {
            sensorManager.registerListener((SensorEventListener) this, gyroSensor, SensorManager.SENSOR_DELAY_GAME);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        bMonitor.stopMonitoring(this);
        mainUpdateHandler.removeCallbacks(updateRunnable);
        mainUpdateHandler.removeCallbacks(hapticRunnable);

        sensorManager.unregisterListener((SensorEventListener) this);
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_GAME_ROTATION_VECTOR) {
            float[] rotationMatrix = new float[9];
            float[] orientationAngles = new float[3];

            SensorManager.getRotationMatrixFromVector(rotationMatrix, event.values);
            SensorManager.getOrientation(rotationMatrix, orientationAngles);

            // Recenter offsets (radians)
            gyroAngleRaw.x = orientationAngles[0];
            gyroAngleRaw.y = orientationAngles[1];
            gyroAngleRaw.z = orientationAngles[2];

            float yawRad   = orientationAngles[0] - gyroOffset.x;
            float pitchRad = orientationAngles[1] - gyroOffset.y;
            float rollRad  = orientationAngles[2] - gyroOffset.z;

            // Normalize
            if (yawRad < -Math.PI) yawRad += (float) (2 * Math.PI);
            if (yawRad > Math.PI)  yawRad -= (float) (2 * Math.PI);

            if (pitchRad < -Math.PI) pitchRad += (float) (2 * Math.PI);
            if (pitchRad > Math.PI)  pitchRad -= (float) (2 * Math.PI);

            if (rollRad < -Math.PI) rollRad += (float) (2 * Math.PI);
            if (rollRad > Math.PI)  rollRad -= (float) (2 * Math.PI);

            // Convert to degrees
            // will work when phone is lying flat on the ground in portrait mode
            gyroAngle.x = (isLeftController) ? (float) Math.toDegrees(yawRad)
                    : (float) Math.toDegrees(yawRad)+180f; // hack, but it at least seems to works
            gyroAngle.y = (float) Math.toDegrees(pitchRad*-1);
            gyroAngle.z = (float) Math.toDegrees(rollRad);
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        Log.w("SensorAccuracy", MessageFormat.format("accuracy:{0}", accuracy));
    }

    @SuppressLint("SetTextI18n")
    private void update() {
        // Log.wtf("UpdateLoop", MessageFormat.format("DEBUG:\nTrigger:{0}; Grip:{1}", triggerState, gripState));

        if (isLeftController) {
            finalGyroAngle = RotationUtils.INSTANCE.rotateEuler(gyroAngle, leftControllerAngleCorrection);
        } else {
            finalGyroAngle = RotationUtils.INSTANCE.rotateEuler(gyroAngle, rightControllerAngleCorrection);
        }

        //Debug Status info
        debugConnectionStatusView.setText(
                getString(R.string.tcp_client_info_template) + debugTCPConnectionStatus);
        debugInfoView.setText(MessageFormat.format("Debug info (isLeft:{0}):\nTrigger:{1}; Grip:{2}; System:{3}" +
                        "A_X: {4}; B_Y:{5}\nGyro: yaw:{6}, pitch{7}, roll{8}\n" +
                        "joystickInput:{9}\njoystickTouchState:{10}; joystickInDZ:{11};" +
                        "\nBattery: percentage:{12}; isPlugged:{13}.",
                isLeftController, triggerState, gripState, btnSystemOrMenuState, btnA_or_X_State, btnB_or_Y_State,
                finalGyroAngle.x, finalGyroAngle.y, finalGyroAngle.z, joyData, joyState, joyInDZ,
                controllerBatteryPercentage, controllerBatteryPlugged));


        // Send data to the server
        if (tcpClient != null && tcpClient.isRunning()) {
            if (System.currentTimeMillis() - tcpClient.gotConnectedTime() >= TCP_Constants.TCP_CLIENT_FIRST_PACKET_DELAY) {
                tcpCompressedPacket = compressDataBeforeSending(
                        isLeftController, triggerState, gripState, btnSystemOrMenuState,
                        btnA_or_X_State, btnB_or_Y_State,
                        finalGyroAngle,
                        joyData, joyState, joyInDZ, controllerBatteryPercentage, controllerBatteryPlugged);
                new Thread(() -> tcpClient.sendMessage(tcpCompressedPacket)).start();
            }
        }

        // String compressedData = compressDataBeforeSending(isLeftController, triggerState, gripState, btnSystemOrMenuState,
        //         btnA_or_X_State, btnB_or_Y_State, gyroAngle, joyData, joyState, joyInDZ,
        //         controllerBatteryPercentage, controllerBatteryPlugged);
        // DebugReceiver.DecodedPacket decodedData = DebugReceiver.fromBase64(compressedData);
        // Log.d("UpdateLoop", MessageFormat.format("Compressed Data:{0}\nDecodedData:{1}", compressedData, decodedData));
    }

    private void hapticUpdate() {
        // Reply to incoming data
        if (inMessages != null && inMessages.size() != 0) {
            InboundDecompressResults decompressResults = decompressInboundPacket(inMessages.readLast());
            Log.i("InboundMessagesResult", MessageFormat.format("Messages to process left: {0}", inMessages.size()));

            if(decompressResults.leftController == isLeftController) {
                generateVibration(decompressResults);
            }

            // Log.w("testRead Result", MessageFormat.format("ok:{0}; isLeft:{1}; amp:{2}; freq:{3}; durationSeconds:{4}",
            //         decompressResults.status, decompressResults.leftController, decompressResults.amplitude,
            //         decompressResults.frequency, decompressResults.durationSeconds));
        }
    }

    private final Runnable resetTrigger = () -> {
        if (triggerState == 1) triggerState = 0;
    };

    private final Runnable resetGrip = () -> {
        if (gripState == 1) gripState = 0;
    };

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // spam sucks. Now it no longer occurs.
        if (event.getRepeatCount() > 0) {
            return true;
        }

        long now = event.getEventTime();

        if (!isLeftController) {
            // IF RIGHT CONTROLLER
            switch (keyCode) {
                case KeyEvent.KEYCODE_VOLUME_DOWN: // trigger
                    if (triggerState == 2) return true;

                    mainUpdateHandler.removeCallbacks(resetTrigger);

                    if (now - lastTriggerPress <= SIDE_BUTTON_SWITCH_THRESHOLD) {
                        triggerState = 2;
                    } else {
                        triggerState = 1;
                    }
                    return true;
                case KeyEvent.KEYCODE_VOLUME_UP: // grip
                    if (gripState == 2) return true;

                    mainUpdateHandler.removeCallbacks(resetGrip);

                    if (now - lastGripPress <= SIDE_BUTTON_SWITCH_THRESHOLD) {
                        gripState = 2;
                    } else {
                        gripState = 1;
                    }
                    return true;
                default:
                    return super.onKeyDown(keyCode, event);
            }
        } else {
            // IF LEFT CONTROLLER
            switch (keyCode) {
                case KeyEvent.KEYCODE_VOLUME_UP: // trigger
                    if (triggerState == 2) return true;

                    mainUpdateHandler.removeCallbacks(resetTrigger);

                    if (now - lastTriggerPress <= SIDE_BUTTON_SWITCH_THRESHOLD) {
                        triggerState = 2;
                    } else {
                        triggerState = 1;
                    }
                    return true;
                case KeyEvent.KEYCODE_VOLUME_DOWN: // grip
                    if (gripState == 2) return true;

                    mainUpdateHandler.removeCallbacks(resetGrip);

                    if (now - lastGripPress <= SIDE_BUTTON_SWITCH_THRESHOLD) {
                        gripState = 2;
                    } else {
                        gripState = 1;
                    }
                    return true;
                default:
                    return super.onKeyDown(keyCode, event);
            }
        }
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        long now = event.getEventTime();

        if (!isLeftController) {
            // RIGHT CONTROLLER
            switch (keyCode) {
                case KeyEvent.KEYCODE_VOLUME_DOWN:
                    if (triggerState == 1) {
                        mainUpdateHandler.postDelayed(resetTrigger, SIDE_BUTTON_SWITCH_THRESHOLD);
                    } else {
                        triggerState = 0;
                    }
                    lastTriggerPress = now;
                    break;
                case KeyEvent.KEYCODE_VOLUME_UP:
                    if (gripState == 1) {
                        mainUpdateHandler.postDelayed(resetGrip, SIDE_BUTTON_SWITCH_THRESHOLD);
                    } else {
                        gripState = 0;
                    }
                    lastGripPress = now;
                    break;
            }
        } else {
            // LEFT CONTROLLER
            switch (keyCode) {
                case KeyEvent.KEYCODE_VOLUME_UP:
                    if (triggerState == 1) {
                        mainUpdateHandler.postDelayed(resetTrigger, SIDE_BUTTON_SWITCH_THRESHOLD);
                    } else {
                        triggerState = 0;
                    }
                    lastTriggerPress = now;
                    break;
                case KeyEvent.KEYCODE_VOLUME_DOWN:
                    if (gripState == 1) {
                        mainUpdateHandler.postDelayed(resetGrip, SIDE_BUTTON_SWITCH_THRESHOLD);
                    } else {
                        gripState = 0;
                    }
                    lastGripPress = now;
                    break;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    public void buttonFeedback(int type) {
        VibratorManager vibratorManager = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
        Vibrator vibrator = vibratorManager != null ? vibratorManager.getDefaultVibrator() : null;
        if (vibrator != null) {
            long[] wave_time = {0, 25};
            long[] wave_time_l = {0, 35, 0, 55};
            long[] wave_time_ll = {0, 5, 0, 10, 0, 5};
            int[] wave_ampl = {0, 100};
            int[] wave_ampl_l = {0, 100, 0, 100};
            int[] wave_ampl_ll = {0, 100, 0, 100, 0, 100};

            VibrationEffect vibrationEffect = null;
            if (type == 0) {
                vibrationEffect = VibrationEffect.createWaveform(wave_time, wave_ampl, -1);
                vibrator.vibrate(vibrationEffect);
            } else if (type == 1) {
                vibrationEffect = VibrationEffect.createWaveform(wave_time_l, wave_ampl_l, -1);
                vibrator.vibrate(vibrationEffect);
            } else {
                vibrationEffect = VibrationEffect.createWaveform(wave_time_ll, wave_ampl_ll, -1);
                vibrator.vibrate(vibrationEffect);
            }
        }
    }

    private void generateVibration(InboundDecompressResults decompressResults) {
        VibratorManager vibratorManager = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
        Vibrator vibrator = vibratorManager != null ? vibratorManager.getDefaultVibrator() : null;

        if (vibrator != null) {
            long durationMs = (long) (decompressResults.durationSeconds * 2000);

            float amp = Math.max(0f, Math.min(1f, decompressResults.amplitude));
            int ampScaled = (int)(amp * 500f); // Forced to scale. Android has different input format. Won't be 100% accurate, compared to the real hardware!
            if (ampScaled == 0) ampScaled = 1;
            if (ampScaled > 100) ampScaled = 100; // enforce a limit, app might crash otherwise.

            if (decompressResults.frequency > 0f) {
                float periodMs = 1000.0f / decompressResults.frequency;
                long on = (long) (periodMs / 2.0f);
                long off = (long) (periodMs / 2.0f);

                int cycles = (int) (durationMs / periodMs);

                if (cycles > 0 && on > 0) {
                    long[] timings = new long[cycles * 2];
                    int[] amplitudes = new int[cycles * 2];

                    for (int i = 0; i < cycles; i++) {
                        timings[i*2]   = on;
                        amplitudes[i*2] = ampScaled;
                        timings[i*2+1] = off;
                        amplitudes[i*2+1] = 0;
                    }

                    VibrationEffect effect = VibrationEffect.createWaveform(timings, amplitudes, -1);
                    vibrator.vibrate(effect);
                    return;
                }
            }

            // fallback
            VibrationEffect effect = VibrationEffect.createOneShot(durationMs, ampScaled);
            vibrator.vibrate(effect);
        }
    }

    View.OnTouchListener btnSystemOrMenuListener = new View.OnTouchListener() {
        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View v, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    btnSystemOrMenuState = true;
                    buttonFeedback(1);
                    return true;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    btnSystemOrMenuState = false;
                    return true;
            }
            return false;
        }
    };

    View.OnTouchListener btnA_Or_X_Listener = new View.OnTouchListener() {
        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View v, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    btnA_or_X_State = true;
                    buttonFeedback(0);
                    return true;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    btnA_or_X_State = false;
                    return true;
            }
            return false;
        }
    };

    View.OnTouchListener btnB_Or_Y_Listener = new View.OnTouchListener() {
        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View v, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    btnB_or_Y_State = true;
                    buttonFeedback(0);
                    return true;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    btnB_or_Y_State = false;
                    return true;
            }
            return false;
        }
    };

    private boolean joyFeedbackLock = false;
    JoystickView.OnMoveListener joystickOnMoveListener = new JoystickView.OnMoveListener() {
        @Override
        public void onMove(int angle, int strength) {
            joyData = joyConvertToVec2(angle, strength);
            joyInDZ = isJoyInDZ(joyData);

            if (!joyInDZ && !joyFeedbackLock) {
                joyFeedbackLock = true;
            }
            if (joyInDZ && !joyFeedbackLock) {
                buttonFeedback(2);
                joyFeedbackLock = true;
            }


            // Log.wtf("JAVA/CPP", MessageFormat.format("angle:{0}; strength{1}/CPP(x,y) {2}, {3}",
            //         angle, strength, joyData.x, joyData.y));
        }
    };

    View.OnTouchListener joystickTouchListener = new JoystickView.OnTouchListener() {
        private long lastReleaseTime = 0;

        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View v, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    long now = event.getEventTime();
                    if (now - lastReleaseTime <= JOY_SWITCH_THRESHOLD) {
                        joyState = 2;
                    } else {
                        joyState = 1;
                    }
                    joyFeedbackLock = false;
                    return false;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    lastReleaseTime = event.getEventTime();
                    joyState = 0;
                    return false;
            }
            return false;
        }
    };

    @SuppressLint({"SourceLockedOrientationActivity", "ClickableViewAccessibility"})
    private void processControllerChange(boolean isLeftController) {
        if(!isLeftController) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);

            // hide
            joyL.setVisibility(View.INVISIBLE);
            btnX.setVisibility(View.INVISIBLE);
            btnY.setVisibility(View.INVISIBLE);
            btnMenu.setVisibility(View.INVISIBLE);

            // show
            joyR.setVisibility(View.VISIBLE);
            btnA.setVisibility(View.VISIBLE);
            btnB.setVisibility(View.VISIBLE);
            btnSystem.setVisibility(View.VISIBLE);

            // Register Listeners
            joyR.setOnMoveListener(joystickOnMoveListener, 10);
            joyR.setOnTouchListener(joystickTouchListener);
            btnA.setOnTouchListener(btnA_Or_X_Listener);
            btnB.setOnTouchListener(btnB_Or_Y_Listener);
            btnSystem.setOnTouchListener(btnSystemOrMenuListener);

            // Unregister Listeners
            joyL.setOnMoveListener(null);
            joyL.setOnTouchListener(null);
            btnX.setOnTouchListener(null);
            btnY.setOnTouchListener(null);
            btnMenu.setOnTouchListener(null);
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

            // hide
            joyR.setVisibility(View.INVISIBLE);
            btnA.setVisibility(View.INVISIBLE);
            btnB.setVisibility(View.INVISIBLE);
            btnSystem.setVisibility(View.INVISIBLE);

            // show
            joyL.setVisibility(View.VISIBLE);
            btnX.setVisibility(View.VISIBLE);
            btnY.setVisibility(View.VISIBLE);
            btnMenu.setVisibility(View.VISIBLE);

            // Register Listeners
            joyL.setOnMoveListener(joystickOnMoveListener, 10);
            joyL.setOnTouchListener(joystickTouchListener);
            btnX.setOnTouchListener(btnA_Or_X_Listener);
            btnY.setOnTouchListener(btnB_Or_Y_Listener);
            btnMenu.setOnTouchListener(btnSystemOrMenuListener);

            // Unregister Listeners
            joyR.setOnMoveListener(null);
            joyR.setOnTouchListener(null);
            btnA.setOnTouchListener(null);
            btnB.setOnTouchListener(null);
            btnSystem.setOnTouchListener(null);
        }

        // Reset values
        joyData = new SVec2(0.0f, 0.0f);
        joyState = 0;

        btnA_or_X_State = false;
        btnB_or_Y_State = false;
        btnSystemOrMenuState = false;
    }

    public native String compressDataBeforeSending(boolean leftController, int triggerState, int gripState, boolean btnSystemOrMenuState,
                                                 boolean btnA_or_XState, boolean btnB_or_YState, SVec3 gyroAngle,
                                                 SVec2 joyData, int joyState, boolean joyInDZ,
                                                 float controllerBatteryPercentage, boolean controllerBatteryPlugged);
    public native InboundDecompressResults decompressInboundPacket(String inDataB64);
    public native SVec2 joyConvertToVec2(int angle, int strength);
    public native boolean isJoyInDZ(SVec2 joyData);
}
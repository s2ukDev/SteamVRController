package org.s2uk.vrcontroller;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Handler;
import android.os.Looper;

import java.util.Objects;

public class BatteryMonitor extends BroadcastReceiver {
    private final BatteryLevelListener listener;
    private final Handler handler = new Handler(Looper.getMainLooper());
    public BatteryMonitor(BatteryLevelListener listener) {
        this.listener = listener;
    }

    public void startMonitoring(Context context) {
        // Register BroadcastReceiver to monitor battery level changes
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        context.registerReceiver(this, filter);
    }

    public void stopMonitoring(Context context) {
        // Unregister the BroadcastReceiver
        context.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Objects.equals(intent.getAction(), Intent.ACTION_BATTERY_CHANGED)) {
            // Get the current battery level and scale
            int level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
            int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);

            // Calculate the battery percentage as a float value between 0 and 1
            float batteryPercentage = (float) level / scale;

            // Get the charging state
            int chargingState = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
            boolean isCharging =
                    chargingState == BatteryManager.BATTERY_PLUGGED_AC
                            || chargingState == BatteryManager.BATTERY_PLUGGED_USB
                            || chargingState == BatteryManager.BATTERY_PLUGGED_WIRELESS;

            // Notify the listener with the current battery percentage and charging state
            if (listener != null) {
                listener.onBatteryLevelChanged(batteryPercentage, isCharging);
            }
        }
    }
}
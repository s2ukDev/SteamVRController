package org.s2uk.vrcontroller;

public interface BatteryLevelListener {
    void onBatteryLevelChanged(float batteryPercentage, boolean plugged);
}
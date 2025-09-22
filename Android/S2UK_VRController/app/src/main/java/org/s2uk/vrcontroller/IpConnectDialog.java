package org.s2uk.vrcontroller;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.InputFilter;
import android.text.InputType;
import android.util.Log;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.appcompat.app.AlertDialog;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.switchmaterial.SwitchMaterial;

import java.text.MessageFormat;

public class IpConnectDialog {
    public static final String CONNECTION_PREFERENCES = "ip_connect_prefs";
    public static final String KEY_IP = "saved_ip";
    public static final String KEY_IS_LEFT = "is_left_controller";

    public interface ipConnectListener {
        void onIpEntered(String ip, boolean isLeftHand);
        void onDisconnect(boolean isLeftHand);
        void onCancelled();
    }

    public static void show(Context ctx, ipConnectListener listener) {
        // Load old preferences
        SharedPreferences preferences = ctx.getSharedPreferences(CONNECTION_PREFERENCES, Context.MODE_PRIVATE);
        String savedIp = preferences.getString(KEY_IP, "");
        boolean savedIsLeftController = preferences.getBoolean(KEY_IS_LEFT, false);

        // Create lyout
        LinearLayout layout = new LinearLayout(ctx);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (14 * ctx.getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);

        EditText ipAddress = new EditText(ctx);
        InputFilter[] filters = new InputFilter[1];
        filters[0] = new InputFilter() {
            @Override
            public CharSequence filter(CharSequence source, int start, int end,
                                       android.text.Spanned dest, int dstart, int dend) {
                if (end > start) {
                    String destTxt = dest.toString();
                    String resultingTxt = destTxt.substring(0, dstart) + source.subSequence(start, end) + destTxt.substring(dend);
                    if (!resultingTxt.matches ("^\\d{1,3}(\\.(\\d{1,3}(\\.(\\d{1,3}(\\.(\\d{1,3})?)?)?)?)?)?")) {
                        return "";
                    } else {
                        String[] splits = resultingTxt.split("\\.");
                        for (int i=0; i<splits.length; i++) {
                            if (Integer.valueOf(splits[i]) > 255) {
                                return "";
                            }
                        }
                    }
                }
                return null;
            }

        };
        ipAddress.setFilters(filters);
        ipAddress.setHint(R.string.tcp_client_dialog_ip_hint);
        ipAddress.setInputType(InputType.TYPE_CLASS_PHONE);

        if (!savedIp.isEmpty()) ipAddress.setText(savedIp);

        // Switch
        SwitchMaterial isLeftHandToggle = new SwitchMaterial(ctx);
        isLeftHandToggle.setText(R.string.connect_client_is_left_hand);
        isLeftHandToggle.setChecked(savedIsLeftController);

        // Add elements
        layout.addView(ipAddress);
        layout.addView(isLeftHandToggle);

        // Build dialog
        MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(ctx);

        builder.setTitle(R.string.tcp_client_dialog_name)
                .setMessage(R.string.tcp_client_dialog_description)
                .setView(layout)
                .setPositiveButton(R.string.tcp_client_dialog_connect, (dialog, which) -> {
                    String ip = ipAddress.getText().toString().trim();
                    boolean isLeft = isLeftHandToggle.isChecked();

                    preferences.edit()
                            .putString(KEY_IP, ip)
                            .putBoolean(KEY_IS_LEFT, isLeft)
                            .apply();

                    Log.i("VRController Init", MessageFormat.format("Retrieved preference \"isLeftController\": {0}.", isLeft));

                    listener.onIpEntered(ip, isLeft);
                })
                .setNeutralButton(R.string.generic_cancel, (dialog, which) -> {
                    dialog.cancel();
                    listener.onCancelled();
                })
                .setNegativeButton(R.string.generic_disconnect, (dialog, which) -> {
                    dialog.cancel();
                    boolean isLeft = isLeftHandToggle.isChecked();

                    preferences.edit()
                            .putBoolean(KEY_IS_LEFT, isLeft)
                            .apply();

                    listener.onDisconnect(isLeft);
                });


        AlertDialog dialog = builder.create();
        dialog.show();
    }
}

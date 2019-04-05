package net.zeevox.myhome.ui;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RadioGroup;
import android.widget.TextView;

import com.google.gson.Gson;

import net.zeevox.myhome.Heater;
import net.zeevox.myhome.R;
import net.zeevox.myhome.json.CustomJsonObject;
import net.zeevox.myhome.json.Methods;
import net.zeevox.myhome.json.Params;

import okhttp3.WebSocket;

public class DashboardFragment extends Fragment {

    private View view;
    private Heater heater;
    private final static String HEATER_SELECTION_OFF = "off";
    private final static String HEATER_SELECTION_AUTO = "auto";
    private final static String HEATER_SELECTION_ON = "on";

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.activity_dashboard, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View v, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(v, savedInstanceState);

        this.view = v;

        view.findViewById(R.id.heater_status_layout).setOnClickListener(v5 -> {

            final String[] selection = new String[1];

            if (heater == null) {
                Snackbar.make(view.findViewById(android.R.id.content), R.string.error_no_heater, Snackbar.LENGTH_SHORT).show();
                return;
            }

            Dialog dialog = new Dialog(getActivity());
            dialog.setContentView(R.layout.dialog_heater_pick);

            RadioGroup heaterSelection = dialog.findViewById(R.id.heater_type_selection);
            View divider = dialog.findViewById(R.id.heater_divider);

            if (heater.isInAutomaticMode()) {
                selection[0] = HEATER_SELECTION_AUTO;
                heaterSelection.check(R.id.selection_heater_auto);
                divider.setVisibility(View.GONE);
                dialog.findViewById(R.id.number_pick_layout).setVisibility(View.GONE);
            } else if (heater.isOn()) {
                selection[0] = HEATER_SELECTION_ON;
                heaterSelection.check(R.id.selection_heater_on);
            } else {
                selection[0] = HEATER_SELECTION_OFF;
                heaterSelection.check(R.id.selection_heater_off);
            }

            heaterSelection.setOnCheckedChangeListener((group, checkedId) -> {
                switch (checkedId) {
                    case R.id.selection_heater_auto:
                        divider.setVisibility(View.GONE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.GONE);
                        selection[0] = HEATER_SELECTION_AUTO;
                        break;
                    case R.id.selection_heater_off:
                        divider.setVisibility(View.VISIBLE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.VISIBLE);
                        selection[0] = HEATER_SELECTION_OFF;
                    case R.id.selection_heater_on:
                        divider.setVisibility(View.VISIBLE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.VISIBLE);
                        selection[0] = HEATER_SELECTION_ON;
                }
            });

            final int[] duration = {30};

            TextView dialogValue = dialog.findViewById(R.id.dialog_number_pick_value);
            dialogValue.setText(String.valueOf(duration[0]));

            ImageView addButton = dialog.findViewById(R.id.dialog_button_plus);
            addButton.setOnClickListener(v12 -> {
                duration[0] += 30;
                dialogValue.setText(String.valueOf(duration[0]));
            });
            addButton.setOnLongClickListener(v14 -> {
                duration[0] += 5;
                dialogValue.setText(String.valueOf(duration[0]));
                return true;
            });
            ImageView minusButton = dialog.findViewById(R.id.dialog_button_minus);
            minusButton.setOnClickListener(v13 -> {
                duration[0] -= 30;
                dialogValue.setText(String.valueOf(duration[0]));
            });
            minusButton.setOnLongClickListener(v14 -> {
                duration[0] -= 5;
                dialogValue.setText(String.valueOf(duration[0]));
                return true;
            });

            Button positiveButton = dialog.findViewById(R.id.dialog_button_ok);
            positiveButton.setOnClickListener(v1 -> {
                WebSocket webSocket = MainActivity.webSocketUtils.getWebSocket();
                CustomJsonObject customJsonObject = new CustomJsonObject().setId(53).setMethod(Methods.HEATER_SET_STATUS);
                Params params = new Params();
                switch (selection[0]) {
                    case HEATER_SELECTION_OFF:
                        params.setDuration(duration[0] * 60).setHeaterOn(false);
                        break;
                    case HEATER_SELECTION_AUTO:
                        params.setDuration(1).setHeaterOn(false);
                        break;
                    case HEATER_SELECTION_ON:
                        params.setDuration(duration[0] * 60).setHeaterOn(true);
                        break;
                }
                customJsonObject.setParams(params);
                webSocket.send(new Gson().toJson(customJsonObject));
                dialog.dismiss();
            });
            dialog.show();
        });
    }

    public void onHeaterUpdated(@NonNull Activity activity,  @NonNull Heater heater) {
        Log.i("DashboardFragment", "onHeaterUpdated");
        this.heater = heater;
        activity.runOnUiThread(() -> {
            TextView heaterStatus = view.findViewById(R.id.heater_status);
            if (!heater.isOn() && heater.isInAutomaticMode()) {
                heaterStatus.setText(R.string.state_off_auto);
            } else if (!heater.isOn() && heater.isInManualMode()) {
                heaterStatus.setText(R.string.state_off_manual);
            } else if (heater.isInAutomaticMode()) {
                heaterStatus.setText(R.string.state_on_auto);
            } else if (heater.isInManualMode()) {
                heaterStatus.setText(R.string.state_on_manual);
            }
        });
    }
}

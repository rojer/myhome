package net.zeevox.myhome.ui;

import android.app.Dialog;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RadioGroup;
import android.widget.TextView;

import com.google.gson.Gson;

import net.zeevox.myhome.R;
import net.zeevox.myhome.json.CustomJsonObject;
import net.zeevox.myhome.json.Methods;
import net.zeevox.myhome.json.Params;

import okhttp3.WebSocket;

public class DashboardFragment extends Fragment {

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.activity_dashboard, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (MainActivity.heater != null) {
            TextView heaterStatus = view.findViewById(R.id.heater_status);
            if (!MainActivity.heater.isOn() && MainActivity.heater.isInAutomaticMode()) {
                heaterStatus.setText(R.string.state_off_auto);
            } else if (!MainActivity.heater.isOn() && MainActivity.heater.isInManualMode()) {
                heaterStatus.setText(R.string.state_off_manual);
            } else if (MainActivity.heater.isInAutomaticMode()) {
                heaterStatus.setText(R.string.state_on_auto);
            } else if (MainActivity.heater.isInManualMode()) {
                heaterStatus.setText(R.string.state_on_manual);
            }
        }

        view.findViewById(R.id.heater_status_layout).setOnClickListener(v -> {

            final String[] selection = new String[1];

            if (MainActivity.heater == null) {
                Snackbar.make(view.findViewById(android.R.id.content), "Please refresh and try again", Snackbar.LENGTH_SHORT).show();
                return;
            }

            Dialog dialog = new Dialog(getActivity());
            dialog.setContentView(R.layout.dialog_heater_pick);

            RadioGroup heaterSelection = dialog.findViewById(R.id.heater_type_selection);
            View divider = dialog.findViewById(R.id.heater_divider);

            if (MainActivity.heater.isInAutomaticMode()) {
                selection[0] = "auto";
                heaterSelection.check(R.id.selection_heater_auto);
                divider.setVisibility(View.GONE);
                dialog.findViewById(R.id.number_pick_layout).setVisibility(View.GONE);
            } else if (MainActivity.heater.isOn()) {
                selection[0] = "on";
                heaterSelection.check(R.id.selection_heater_on);
            } else {
                selection[0] = "off";
                heaterSelection.check(R.id.selection_heater_off);
            }

            heaterSelection.setOnCheckedChangeListener((group, checkedId) -> {
                switch (checkedId) {
                    case R.id.selection_heater_auto:
                        divider.setVisibility(View.GONE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.GONE);
                        selection[0] = "auto";
                        break;
                    case R.id.selection_heater_off:
                        divider.setVisibility(View.VISIBLE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.VISIBLE);
                        selection[0] = "off";
                    case R.id.selection_heater_on:
                        divider.setVisibility(View.VISIBLE);
                        dialog.findViewById(R.id.number_pick_layout).setVisibility(View.VISIBLE);
                        selection[0] = "on";
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
                    case "off":
                        params.setDuration(duration[0] * 60).setHeater_on(false);
                        break;
                    case "auto":
                        params.setDuration(1).setHeater_on(false);
                        break;
                    case "on":
                        params.setDuration(duration[0] * 60).setHeater_on(true);
                        break;
                }
                customJsonObject.setParams(params);
                webSocket.send(new Gson().toJson(customJsonObject));
                dialog.dismiss();
            });
            dialog.show();
        });
    }
}

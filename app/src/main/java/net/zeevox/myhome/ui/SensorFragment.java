package net.zeevox.myhome.ui;

import android.app.Dialog;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import net.zeevox.myhome.R;
import net.zeevox.myhome.Sensor;

import java.text.DecimalFormat;
import java.util.Objects;

public class SensorFragment extends Fragment {

    private final DecimalFormat oneDecimal = new DecimalFormat("#.#");
    private final DecimalFormat oneDecimalExact = new DecimalFormat("0.0");
    private final String TAG = getClass().getSimpleName();

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.activity_sensor, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        assert getArguments() != null;

        Sensor sensor = MainActivity.sensors.getBySID(getArguments().getInt(Sensor.SID));

        assert sensor != null;
        double currentTemp = sensor.getValue(Sensor.TEMP_SUBID, 0.0);
        double relativeHumidity = sensor.getValue(Sensor.RH_SUBID, -1000.0);
        final double[] targetTemp = {sensor.getValue(2, -1000.0)};
        double timestamp = sensor.getTimestamp();

        TextView tempUnitsTextView = view.findViewById(R.id.sensor_units);
        tempUnitsTextView.setText(R.string.units_celsius);

        TextView currentTempValue = view.findViewById(R.id.sensor_value);
        if (!(currentTemp == -1000)) {
            currentTempValue.setText(String.valueOf(oneDecimal.format(currentTemp)));
            view.findViewById(R.id.sensor_value_layout).setOnClickListener(v -> startGraphFragment(sensor.getSID(), Sensor.TEMP_SUBID));
        }

        if (relativeHumidity == -1000) {
            view.findViewById(R.id.sensor_rh_layout).setVisibility(View.GONE);
        } else {
            ((TextView) view.findViewById(R.id.sensor_rh)).setText(
                    String.format("%s%%", String.valueOf(oneDecimal.format(relativeHumidity))));
            view.findViewById(R.id.sensor_rh_layout).setOnClickListener(v -> startGraphFragment(sensor.getSID(), Sensor.RH_SUBID));
        }

        if (targetTemp[0] == -1000) {
            view.findViewById(R.id.sensor_target_layout).setVisibility(View.GONE);
        } else {
            ((TextView) view.findViewById(R.id.sensor_target)).setText(
                    String.format("%s %s", String.valueOf(oneDecimalExact.format(targetTemp[0])), getString(R.string.units_celsius)));
            view.findViewById(R.id.sensor_target_layout).setOnClickListener(v -> {
                assert getActivity() != null;
                Dialog dialog = new Dialog(getActivity());
                dialog.setContentView(R.layout.dialog_number_pick);

                TextView dialogValue = dialog.findViewById(R.id.dialog_number_pick_value);
                dialogValue.setText(oneDecimalExact.format(targetTemp[0]));

                ImageView addButton = dialog.findViewById(R.id.dialog_button_plus);
                addButton.setOnClickListener(v12 -> {
                    targetTemp[0] += 0.1;
                    dialogValue.setText(oneDecimalExact.format(targetTemp[0]));
                });
                addButton.setOnLongClickListener(v14 -> {
                    targetTemp[0] += 1.0;
                    dialogValue.setText(oneDecimalExact.format(targetTemp[0]));
                    return true;
                });
                ImageView minusButton = dialog.findViewById(R.id.dialog_button_minus);
                minusButton.setOnClickListener(v13 -> {
                    targetTemp[0] -= 0.1;
                    dialogValue.setText(oneDecimalExact.format(targetTemp[0]));
                });
                minusButton.setOnLongClickListener(v14 -> {
                    targetTemp[0] -= 1.0;
                    dialogValue.setText(oneDecimalExact.format(targetTemp[0]));
                    return true;
                });

                Button positiveButton = dialog.findViewById(R.id.dialog_button_ok);
                positiveButton.setOnClickListener(v1 -> {
                    ((TextView) view.findViewById(R.id.sensor_target)).setText(String.format("%s %s", oneDecimalExact.format(targetTemp[0]), getString(R.string.units_celsius)));
                    // TODO The value ought to be sent to the Hub and thus set the target temperature
                    // This is not yet possible as the implementation on the side of the Hub is not ready
                    dialog.dismiss();
                });
                dialog.show();
            });
        }

        if (timestamp == 0) {
            view.findViewById(R.id.last_updated).setVisibility(View.INVISIBLE);
        } else {
            String status;
            final double diff = System.currentTimeMillis() - timestamp * 1000;
            if (diff < 60000) {
                status = getString(R.string.ts_less_than_minute);
            } else if (diff < 120000) {
                status = getString(R.string.ts_one_minute);
            } else if (diff > 3600000) {
                status = getString(R.string.ts_hour);
            } else {
                status = String.format(getString(R.string.ts_minutes), String.valueOf(((Double) Math.floor(diff / 1000 / 60)).intValue()));
            }
            ((TextView) view.findViewById(R.id.last_updated)).setText(status);
        }
    }

    private void startGraphFragment(int sid, int subid) {
        try {
            Fragment fragment = GraphFragment.class.newInstance();
            Bundle bundle = new Bundle();
            bundle.putInt(Sensor.SID, sid);
            bundle.putInt(Sensor.SUBID, subid);
            fragment.setArguments(bundle);
            FragmentTransaction fragmentTransaction = Objects.requireNonNull(getActivity()).getSupportFragmentManager().beginTransaction();
            fragmentTransaction.setCustomAnimations(android.R.anim.fade_in, android.R.anim.fade_out);
            fragmentTransaction.replace(R.id.frame_layout, fragment, TAG)
                    .addToBackStack(TAG)
                    .commit();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (java.lang.InstantiationException e) {
            e.printStackTrace();
        }
    }
}

package net.zeevox.myhome.ui;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.preference.EditTextPreference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.view.View;
import android.widget.ListView;

import net.zeevox.myhome.R;

public class SettingsFragment extends PreferenceFragmentCompat {

    public static final String DATA_URL = "data_url";
    public static final String HUB_URL = "hub_url";
    public static final String SHOW_EXTREMES = "show_extremes";
    public static final String SHOW_LIMIT_LINE = "show_limit_line";
    public static final String VALUE_UNITS = "value_units";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.preferences);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        try {
            getActivity().findViewById(R.id.dashboard_swipe_refresh).setEnabled(false);
        } catch (NullPointerException npe) {
            npe.printStackTrace();
        }

        EditTextPreference HubUriPreference = (EditTextPreference) findPreference(HUB_URL);
        HubUriPreference.setSummary(HubUriPreference.getText());
        HubUriPreference.setOnPreferenceChangeListener((preference, newValue) -> {
            preference.setSummary((String) newValue);
            return true;
        });

        EditTextPreference DataUrlPreference = (EditTextPreference) findPreference(DATA_URL);
        DataUrlPreference.setSummary(DataUrlPreference.getText());
        DataUrlPreference.setOnPreferenceChangeListener((preference, newValue) -> {
            preference.setSummary((String) newValue);
            return true;
        });
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Hide divider line between preferences
        View rootView = getView();
        if (rootView != null) {
            ListView list = rootView.findViewById(android.R.id.list);
            list.setDivider(null);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        try {
            getActivity().findViewById(R.id.dashboard_swipe_refresh).setEnabled(true);
        } catch (NullPointerException npe) {
            npe.printStackTrace();
        }
    }
}

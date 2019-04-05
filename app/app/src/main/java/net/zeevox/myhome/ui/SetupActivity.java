package net.zeevox.myhome.ui;

import android.app.Dialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.design.widget.TextInputEditText;
import android.support.design.widget.TextInputLayout;
import android.support.v7.app.AppCompatActivity;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.Button;

import net.zeevox.myhome.R;

public class SetupActivity extends AppCompatActivity {

    SharedPreferences preferences;
    SharedPreferences.Editor editor;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        preferences = PreferenceManager.getDefaultSharedPreferences(this);
        editor = preferences.edit();

        Dialog dialog = new Dialog(this);
        dialog.setContentView(R.layout.dialog_url_input);

        TextInputEditText hubURL = dialog.findViewById(R.id.text_edit_hub_url);
        TextInputEditText dataURL = dialog.findViewById(R.id.text_edit_data_url);

        Button positiveButton = dialog.findViewById(R.id.dialog_button_ok);
        positiveButton.setOnClickListener(v1 -> {
            Editable hubUrlText = hubURL.getText();
            Editable dataUrlText = dataURL.getText();
            if (hubUrlText == null) {
                TextInputLayout hubUrlLayout = dialog.findViewById(R.id.text_input_hub_url);
                hubUrlLayout.setError("Please input something!");
                hubURL.addTextChangedListener(new TextWatcher() {
                    @Override
                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {

                    }

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        hubUrlLayout.setErrorEnabled(false);
                    }

                    @Override
                    public void afterTextChanged(Editable s) {

                    }
                });
            } else if (dataUrlText == null) {
                TextInputLayout hubDataLayout = dialog.findViewById(R.id.text_input_data_url);
                hubDataLayout.setError("Please input something!");
                dataURL.addTextChangedListener(new TextWatcher() {
                    @Override
                    public void beforeTextChanged(CharSequence s, int start, int count, int after) {

                    }

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        hubDataLayout.setErrorEnabled(false);
                    }

                    @Override
                    public void afterTextChanged(Editable s) {

                    }
                });
            } else {
                editor.putString(SettingsFragment.HUB_URL, hubUrlText.toString());
                editor.putString(SettingsFragment.DATA_URL, dataUrlText.toString());
                editor.apply();
                dialog.dismiss();
                startActivity(new Intent(SetupActivity.this, MainActivity.class));
                finish();
            }
        });

        dialog.setCancelable(false);
        dialog.setCanceledOnTouchOutside(false);

        dialog.show();
    }
}

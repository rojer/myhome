package net.zeevox.myhome.graph;

import com.github.mikephil.charting.components.AxisBase;
import com.github.mikephil.charting.components.XAxis;
import com.github.mikephil.charting.formatter.ValueFormatter;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class TimeValueFormatter extends ValueFormatter {

    public TimeValueFormatter() {
    }

    @Override
    public String getFormattedValue(float value) {
        Date date = new Date((long) value * 1000);
        return new SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(date);
    }

    @Override
    public String getAxisLabel(float value, AxisBase axis) {
        if (axis instanceof XAxis) {
            Date date = new Date((long) value * 1000);
            return new SimpleDateFormat("HH:00", Locale.getDefault()).format(date);
        } else {
            return super.getAxisLabel(value, axis);
        }
    }
}

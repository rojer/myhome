package net.zeevox.myhome.graph;

import com.github.mikephil.charting.formatter.ValueFormatter;

import java.text.SimpleDateFormat;
import java.util.Date;

public class TimeValueFormatter extends ValueFormatter {

    public TimeValueFormatter() {
    }

    @Override
    public String getFormattedValue(float value) {
        Date date = new Date((long) value);
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("HH:mm:ss");
        return simpleDateFormat.format(date);
    }
}

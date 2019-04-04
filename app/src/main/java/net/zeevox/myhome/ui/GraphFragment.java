package net.zeevox.myhome.ui;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.XAxis;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.github.mikephil.charting.utils.EntryXComparator;

import net.zeevox.myhome.R;
import net.zeevox.myhome.Sensor;
import net.zeevox.myhome.graph.TimeValueFormatter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

public class GraphFragment extends Fragment {

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.activity_graph, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        assert getArguments() != null;

        int sid = getArguments().getInt(Sensor.SID);
        int subid = getArguments().getInt(Sensor.SUBID);

        String name;
        switch (subid) {
            case Sensor.TEMP_SUBID:
                name = "Temperature";
                break;
            case Sensor.RH_SUBID:
                name = "Relative Humidity";
                break;
            default:
                name = "Sensor";
        }

        LineChart chart = view.findViewById(R.id.chart);

        List<Entry> entries = new ArrayList<>();

        // TODO Data for graph is randomly generated, receive data from server instead
        for (int i = 0; i < 100; i++) {
            entries.add(new Entry(System.currentTimeMillis() - i * 180000, i + new Random().nextFloat()));
        }
        Collections.sort(entries, new EntryXComparator());

        LineDataSet dataSet = new LineDataSet(entries, name);

        LineData lineData = new LineData(dataSet);
        chart.setData(lineData);

        chart.setTouchEnabled(true);
        chart.setDragXEnabled(true);
        chart.setScaleXEnabled(true);
        chart.setScaleYEnabled(false);
        chart.setDoubleTapToZoomEnabled(false);

        chart.getXAxis().setValueFormatter(new TimeValueFormatter());
        chart.getXAxis().setPosition(XAxis.XAxisPosition.BOTTOM);


        chart.getDescription().setEnabled(false);

        chart.invalidate();
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        try {
            getActivity().findViewById(R.id.dashboard_swipe_refresh).setEnabled(false);
        } catch (NullPointerException npe) {
            npe.printStackTrace();
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        try {
            getActivity().findViewById(R.id.dashboard_swipe_refresh).setEnabled(true);
        } catch (NullPointerException npe) {
            npe.printStackTrace();
        }
    }
}

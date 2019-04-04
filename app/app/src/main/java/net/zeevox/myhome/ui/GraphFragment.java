package net.zeevox.myhome.ui;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.XAxis;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.github.mikephil.charting.highlight.Highlight;
import com.github.mikephil.charting.listener.OnChartValueSelectedListener;
import com.github.mikephil.charting.utils.EntryXComparator;
import com.google.gson.Gson;

import net.zeevox.myhome.R;
import net.zeevox.myhome.Sensor;
import net.zeevox.myhome.WebSocketUtils;
import net.zeevox.myhome.graph.TimeValueFormatter;

import java.io.EOFException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class GraphFragment extends Fragment {

    public static final String DATA = "data";
    List<Entry> entries = new ArrayList<>();
    private WebSocketUtils webSocketUtils = new WebSocketUtils();
    private WebSocket webSocket;
    private int sid;
    private int subid;
    private int daysBack = 0;
    private String name;
    private LineChart chart;
    private float[] extremes;
    private long launchSecond = System.currentTimeMillis() / 1000;
    private View view;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.activity_graph, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        this.view = view;

        assert getArguments() != null;

        sid = getArguments().getInt(Sensor.SID);
        subid = getArguments().getInt(Sensor.SUBID);

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

        chart = view.findViewById(R.id.chart);

        fetchData();

        chart.setTouchEnabled(true);
        chart.setDragXEnabled(true);
        chart.setScaleXEnabled(true);
        chart.setScaleYEnabled(false);
        chart.setDoubleTapToZoomEnabled(false);
        chart.setOnChartValueSelectedListener(new OnChartValueSelectedListener() {
            @Override
            public void onValueSelected(Entry e, Highlight h) {
                String time = new SimpleDateFormat("EEE d MMM yyyy 'at' HH:mm:ss", Locale.getDefault()).format(e.getX() * 1000);
                ((TextView) view.findViewById(R.id.graph_point_info)).setText(String.format(Locale.getDefault(), "%s | %.2f", time, e.getY()));
            }

            @Override
            public void onNothingSelected() {

            }
        });

        chart.getAxisRight().setEnabled(false);

        chart.getXAxis().setGranularity(7200f);
        chart.getXAxis().setValueFormatter(new TimeValueFormatter());
        chart.getXAxis().setPosition(XAxis.XAxisPosition.BOTTOM);
        chart.getXAxis().setDrawGridLines(false);

        if (subid == Sensor.RH_SUBID) {
            chart.getAxisLeft().setAxisMaximum(100f);
        }

        chart.getLegend().setEnabled(false);
        chart.getDescription().setEnabled(false);

        ImageView buttonBack = view.findViewById(R.id.graph_chevron_left);
        ImageView buttonForward = view.findViewById(R.id.graph_chevron_right);
        TextView date = view.findViewById(R.id.graph_date);

        buttonBack.setOnClickListener(v -> {
            daysBack += 1;
            date.setText(daysBack == 1 ? getString(R.string.graph_yesterday) : String.format(Locale.getDefault(), getString(R.string.graph_days_ago), daysBack));
            buttonForward.setEnabled(true);
            buttonForward.setVisibility(View.VISIBLE);
            fetchData();
        });

        buttonForward.setOnClickListener(v -> {
            daysBack -= 1;
            if (daysBack == 0) {
                buttonForward.setEnabled(false);
                buttonForward.setVisibility(View.INVISIBLE);
                date.setText(getResources().getString(R.string.graph_today));
            } else {
                date.setText(daysBack == 1 ? getString(R.string.graph_yesterday) :
                        String.format(Locale.getDefault(), getString(R.string.graph_days_ago), daysBack));
            }
            fetchData();
        });
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
        webSocket.cancel();
        try {
            getActivity().findViewById(R.id.dashboard_swipe_refresh).setEnabled(true);
        } catch (NullPointerException npe) {
            npe.printStackTrace();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        webSocket.close(4255, null);
    }

    private void fetchData() {
        extremes = null;
        long timeFrom = launchSecond - 86400 * (daysBack + 1);
        long timeTo = launchSecond - 86400 * daysBack;
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        webSocketUtils.connectWebSocket(preferences.getString(SettingsFragment.DATA_URL, "wss://rojer.me/ss/rpc"), new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                super.onOpen(webSocket, response);
                GraphFragment.this.webSocket = webSocket;
                webSocket.send("{\"method\": \"Sensor.GetData\", \"id\": 100, \"params\": {\"sid\": "
                        + sid + ", \"limit\": 10000, \"subid\": " + subid + ", \"ts_from\": " + timeFrom + ", \"ts_to\": " + timeTo + "}}");
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                super.onMessage(webSocket, text);
                Log.d("GraphFragment", text);
                final Gson gson = new Gson();
                Map response = gson.fromJson(text, Map.class);
                int messageID = MainActivity.toInt(response.get("id"));
                Log.d("GraphFragment", "Reading message with id " + messageID);
                switch (messageID) {
                    case 100:
                        webSocket.close(4256, null);
                        entries.clear();
                        ArrayList<Map<String, Double>> data = (ArrayList<Map<String, Double>>) ((Map) response.get(Sensor.RESULT)).get(DATA);
                        if (data != null) {
                            Log.d("GraphFragment", "Data received, processing...");
                            for (Map<String, Double> point : data) {
                                float time = point.get("ts").floatValue();
                                float value = point.get("v").floatValue();
                                if (extremes == null) {
                                    extremes = new float[]{time, value};
                                }
                                entries.add(new Entry(time, value));
                                if (value < extremes[0]) extremes[0] = value;
                                if (value > extremes[1]) extremes[1] = value;
                            }
                        } else {
                            Log.e(getClass().getSimpleName(), "Data is empty");
                        }
                        Collections.sort(entries, new EntryXComparator());

                        getActivity().runOnUiThread(() -> {
                            LineDataSet dataSet = new LineDataSet(entries, name);
                            dataSet.setDrawCircles(false);
                            dataSet.setColor(getResources().getColor(R.color.colorPrimary));

                            if (extremes[0] < 0) {
                                chart.getAxisLeft().setAxisMinimum(extremes[0]);
                            } else {
                                chart.getAxisLeft().setAxisMinimum(0f);
                            }

                            LineData lineData = new LineData(dataSet);

                            chart.setData(lineData);
                            chart.invalidate();

                            TextView extremesInfo = view.findViewById(R.id.graph_extremes_info);
                            if (preferences.getBoolean(SettingsFragment.SHOW_EXTREMES, true)) {
                                extremesInfo.setText(String.format(Locale.getDefault(), "Min: %.2f | Max: %.2f", extremes[0], extremes[1]));
                            } else {
                                extremesInfo.setVisibility(View.GONE);
                            }
                        });

                        break;
                }
            }

            @Override
            public void onClosed(WebSocket webSocket, int code, String reason) {
                super.onClosed(webSocket, code, reason);
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                super.onFailure(webSocket, t, response);
                if (!(t instanceof EOFException)) {
                    t.printStackTrace();
                }
            }
        });
    }
}

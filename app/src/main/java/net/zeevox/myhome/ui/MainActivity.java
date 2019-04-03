package net.zeevox.myhome.ui;

import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.NavigationView;
import android.support.design.widget.Snackbar;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.view.GravityCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v4.widget.SwipeRefreshLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.TextView;

import com.google.gson.Gson;

import net.zeevox.myhome.BuildConfig;
import net.zeevox.myhome.Heater;
import net.zeevox.myhome.R;
import net.zeevox.myhome.Sensor;
import net.zeevox.myhome.Sensors;
import net.zeevox.myhome.WebSocketUtils;

import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Map;
import java.util.Objects;

import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class MainActivity extends AppCompatActivity
        implements NavigationView.OnNavigationItemSelectedListener {

    public static final Sensors sensors = new Sensors();
    public static Heater heater;
    public static WebSocketUtils webSocketUtils;
    private final boolean[] web_socket_connected = {false};
    private WebSocketListener webSocketListener;
    private Fragment fragment;
    private NavigationView navigationView;
    private MenuItem menuItem;
    private SharedPreferences preferences;
    private SwipeRefreshLayout swipeRefreshLayout;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        preferences = PreferenceManager.getDefaultSharedPreferences(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            getWindow().setNavigationBarColor(getResources().getColor(R.color.colorPrimary));
        }

        FragmentTransaction fragmentTransaction = getSupportFragmentManager().beginTransaction();
        fragmentTransaction.replace(R.id.frame_layout, new DashboardFragment());
        fragmentTransaction.commit();

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        DrawerLayout drawer = findViewById(R.id.drawer_layout);
        ActionBarDrawerToggle toggle = new ActionBarDrawerToggle(
                this, drawer, toolbar, R.string.nav_drawer_open, R.string.nav_drawer_close);
        drawer.addDrawerListener(toggle);
        toggle.syncState();

        Objects.requireNonNull(getSupportActionBar()).setElevation(0);

        navigationView = findViewById(R.id.nav_view);
        navigationView.setNavigationItemSelectedListener(this);
        ((TextView) navigationView.getHeaderView(0).findViewById(R.id.nav_header_app_version)).setText(String.format("v%s", BuildConfig.VERSION_NAME));

        swipeRefreshLayout = findViewById(R.id.dashboard_swipe_refresh);
        swipeRefreshLayout.setOnRefreshListener(() -> {
            swipeRefreshLayout.setRefreshing(false);
            if (!refreshData()) {
                Snackbar.make(findViewById(android.R.id.content), "Please wait while the device connects", Snackbar.LENGTH_SHORT).show();
            } else {
                if (menuItem != null) {
                    onNavigationItemSelected(menuItem);
                } else {
                    try {
                        getSupportFragmentManager().beginTransaction().replace(R.id.frame_layout, DashboardFragment.class.newInstance()).commit();
                    } catch (Exception e) {
                        e.printStackTrace();
                        Snackbar.make(findViewById(android.R.id.content), "Please switch to another tab and try again", Snackbar.LENGTH_LONG).show();
                    }
                }
            }
        });
        swipeRefreshLayout.setColorSchemeColors(getResources().getColor(R.color.colorAccent));

        final Gson gson = new Gson();

        webSocketUtils = new WebSocketUtils();
        webSocketListener = new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                super.onOpen(webSocket, response);
                web_socket_connected[0] = true;
                MainActivity.this.runOnUiThread(() -> swipeRefreshLayout.setRefreshing(false));
                webSocket.send("{\"method\": \"Hub.Data.List\", \"id\": 1157}");
                refreshData();
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                super.onMessage(webSocket, text);
                Log.d("WebSocket onMessage", text);
                Map data = gson.fromJson(text, Map.class);
                Log.d("WebSocket message id", String.valueOf(toInt(data.get("id"))));
                if (data.get("error") != null) {
                    Map error = (Map) data.get("error");
                    assert error != null;
                    Snackbar.make(findViewById(android.R.id.content), "Error " +
                            toInt(error.get("code"))
                            + ": " + error.get("message"), Snackbar.LENGTH_LONG).show();
                    return;
                }

                switch (toInt(data.get("id"))) {
                    case 1157: // A message listing the available sensors
                        try {
                            ArrayList<Map> sensorsList = (ArrayList<Map>) data.get(Sensor.RESULT);
                            assert sensorsList != null;
                            for (Map sensorInfo : sensorsList) {
                                if (sensorInfo.get("name") != null) {
                                    if (sensors.getBySID(toInt(sensorInfo.get(Sensor.SID))) == null) {
                                        Sensor sensorToAdd = new Sensor(toInt(sensorInfo.get(Sensor.SID)));
                                        sensorToAdd.setName((String) sensorInfo.get(Sensor.NAME));
                                        sensorToAdd.addSubID(toInt(sensorInfo.get(Sensor.SUBID)));
                                        sensors.getList().add(sensorToAdd);
                                    } else {
                                        Sensor sensorToManage = sensors.getBySID(toInt(sensorInfo.get(Sensor.SID)));
                                        sensorToManage.addSubID(toInt(sensorInfo.get(Sensor.SUBID)));
                                        sensorToManage.setName((String) sensorInfo.get(Sensor.NAME));
                                    }
                                }
                            }
                        } catch (NullPointerException npe) {
                            npe.printStackTrace();
                        }
                        Menu menu = navigationView.getMenu();
                        for (Sensor sensor : sensors.getList()) {
                            MainActivity.this.runOnUiThread(() -> {
                                menu.removeItem(sensor.getSID());
                                MenuItem menuItem = menu.add(R.id.group_sensors, sensor.getSID(), 0, sensor.getName());
                                menuItem.setIcon(R.drawable.ic_thermometer_black);
                                menuItem.setCheckable(true);
                            });
                        }
                        break;
                    case 637:
                        Map resultSensor = (Map) data.get(Sensor.RESULT);
                        assert resultSensor != null;
                        Sensor sensor = sensors.getBySID(toInt(resultSensor.get(Sensor.SID)));
                        sensor.setTimestamp((Double) resultSensor.get(Sensor.TIMESTAMP));
                        sensor.newValue(toInt(resultSensor.get(Sensor.SUBID)), (Double) resultSensor.get(Sensor.VALUE));
                        break;
                    case 8347:
                        Map resultHeater = (Map) data.get(Sensor.RESULT);
                        assert resultHeater != null;
                        heater = new Heater((boolean) resultHeater.get(Heater.ON), toInt(resultHeater.get(Heater.DURATION)));
                        break;
                    case 53:
                        //webSocket.send("{\"method\": \"Hub.Heater.GetStatus\", \"id\": 8347}");
                        break;
                }
            }

            @Override
            public void onClosed(WebSocket webSocket, int code, String reason) {
                super.onClosed(webSocket, code, reason);
                Log.w("WebSocket.onClosed", reason);
                web_socket_connected[0] = false;
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                super.onFailure(webSocket, t, response);
                if (t instanceof SocketTimeoutException) {
                    Snackbar.make(findViewById(android.R.id.content), "Connection failure", Snackbar.LENGTH_SHORT)
                            .setAction("REFRESH", v -> refreshData())
                            .show();
                }
                t.printStackTrace();
                web_socket_connected[0] = false;
            }
        };
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshData();
    }

    private boolean refreshData() {
        Log.d(getClass().getSimpleName(), "refreshData called");

        if (!web_socket_connected[0]) {
            swipeRefreshLayout.setRefreshing(true);
            webSocketUtils.connectWebSocket(preferences.getString(SettingsFragment.HUB_URL, "ws://192.168.1.10/rpc"), webSocketListener);
            return false;
        } else {
            WebSocket webSocket = webSocketUtils.getWebSocket();
            for (Sensor sensor : sensors.getList()) {
                for (Integer SubID : sensor.getValues().keySet()) {
                    Log.d("WebSocket", "Requesting data from sensor " + sensor.getSID() + " #" + SubID);
                    webSocket.send("{\"method\": \"Hub.Data.Get\", \"id\": 637, \"params\": {\"sid\": "
                            + sensor.getSID() + ", \"subid\": " + SubID + "}}");
                }
            }
            webSocket.send("{\"method\": \"Hub.Heater.GetStatus\", \"id\": 8347}");
            return true;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        webSocketUtils.getWebSocket().close(4522, "onPause");
    }

    @Override
    public void onBackPressed() {
        DrawerLayout drawer = findViewById(R.id.drawer_layout);
        if (drawer.isDrawerOpen(GravityCompat.START)) {
            drawer.closeDrawer(GravityCompat.START);
        } else {
            super.onBackPressed();
        }
    }

    @SuppressWarnings("StatementWithEmptyBody")
    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        // Handle navigation view item clicks here.
        int id = item.getItemId();

        menuItem = item;

        Class fragmentClass;
        Bundle bundle = new Bundle();

        switch (id) {
            case R.id.nav_dashboard:
                fragmentClass = DashboardFragment.class;
                break;
            case R.id.nav_settings:
                fragmentClass = SettingsFragment.class;
                break;
            default:
                fragmentClass = SensorFragment.class;
                bundle.putInt(SensorFragment.SID, item.getItemId());
        }

        try {
            fragment = (Fragment) fragmentClass.newInstance();
            fragment.setArguments(bundle);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InstantiationException e) {
            e.printStackTrace();
        }

        assert fragment != null;
        getSupportFragmentManager().beginTransaction().replace(R.id.frame_layout, fragment).commit();

        if (item.isCheckable()) {
            navigationView.setCheckedItem(item);
            setTitle(item.getTitle());
        }
        DrawerLayout drawer = findViewById(R.id.drawer_layout);
        drawer.closeDrawer(GravityCompat.START);
        return true;
    }

    private int toInt(Object o) {
        Double d = (Double) o;
        return (int) Math.round(d);
    }
}

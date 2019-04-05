package net.zeevox.myhome;

import com.google.gson.Gson;

import net.zeevox.myhome.json.CustomJsonObject;
import net.zeevox.myhome.json.Methods;
import net.zeevox.myhome.json.Params;
import net.zeevox.myhome.ui.MainActivity;

import java.util.HashMap;

import okhttp3.WebSocket;

public class Sensor {
    public static final String SID = "sid";
    public static final String VALUE = "value";
    public static final String TIMESTAMP = "ts";
    public static final String SUBID = "subid";
    public static final String RESULT = "result";
    public static final String NAME = "name";
    public static final String TARGET_ENABLE = "enable";
    public static final String TARGET_MIN = "min";
    public static final String TARGET_MAX = "max";
    public static final int TEMP_SUBID = 0;
    public static final int RH_SUBID = 1;
    private int sensorSID;
    private HashMap<Integer, Double> values = new HashMap<>();
    private double timestamp;
    private String name;
    private boolean targetEnabled;
    private int targetSubID;
    private double targetMin = 0;
    private double targetMax = 0;

    public Sensor(int SID, HashMap<Integer, Double> values, double timestamp) {
        this.sensorSID = SID;
        this.values = values;
        this.timestamp = timestamp;
    }

    public Sensor(int SID, double timestamp) {
        this.sensorSID = SID;
        this.timestamp = timestamp;
    }

    public Sensor(int SID) {
        this.sensorSID = SID;
    }

    public int getSID() {
        return sensorSID;
    }

    public void setSID(int SID) {
        this.sensorSID = SID;
    }

    public double getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(double timestamp) {
        this.timestamp = timestamp;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public HashMap<Integer, Double> getValues() {
        return values;
    }

    public Double getValue(Integer subID, Double defaultValue) {
        if (values == null) {
            return defaultValue;
        } else if (values.containsKey(subID)) {
            return values.get(subID);
        } else {
            return defaultValue;
        }
    }

    public void newValue(Integer subID, Double value) {
        values.remove(subID);
        values.put(subID, value);
    }

    public void addSubID(Integer subID) {
        values.put(subID, 0.0);
    }

    public void setTarget(boolean enabled, int subid, double min, double max) {
        targetEnabled = enabled;
        targetSubID = subid;
        targetMin = min;
        targetMax = max;
    }

    public boolean isTargetEnabled() {
        return targetEnabled;
    }

    public void setTargetEnabled(boolean enabled) {
        targetEnabled = enabled;
        onHeaterChanged(MainActivity.webSocketUtils.getWebSocket());
    }

    public double[] getTargets() {
        return new double[]{targetMin, targetMax};
    }

    public void setTargets(double[] targets) {
        targetMin = targets[0];
        targetMax = targets[1];
        onHeaterChanged(MainActivity.webSocketUtils.getWebSocket());
    }

    private void onHeaterChanged(WebSocket webSocket) {
        webSocket.send(new Gson().toJson(
                new CustomJsonObject().setMethod(Methods.HEATER_SET_LIMITS).setId(639).setParams(new Params()
                        .setSID(sensorSID)
                        .setSubID(targetSubID)
                        .setMin(targetMin)
                        .setMax(targetMax)
                        .setEnable(targetEnabled))));
    }
}

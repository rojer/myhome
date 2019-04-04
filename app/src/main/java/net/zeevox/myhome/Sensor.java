package net.zeevox.myhome;

import java.util.HashMap;

public class Sensor {
    public static final String SID = "sid";
    public static final String VALUE = "value";
    public static final String TIMESTAMP = "ts";
    public static final String SUBID = "subid";
    public static final String RESULT = "result";
    public static final String NAME = "name";
    public static final int TEMP_SUBID = 0;
    public static final int RH_SUBID = 1;
    private int sensorSID;
    private HashMap<Integer, Double> values = new HashMap<>();
    private double timestamp;
    private String name;

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
}

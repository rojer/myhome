package net.zeevox.myhome.json;

@SuppressWarnings({"FieldCanBeLocal", "unused"})
public class Params {
    private int sid;
    private int subid;
    private boolean heater_on;
    private int duration;
    private int limit;
    private float ts_from;
    private float ts_to;
    private double min;
    private double max;
    private boolean enable;

    public Params() {
    }

    public Params setSID(int sid) {
        this.sid = sid;
        return this;
    }

    public Params setSubID(int subid) {
        this.subid = subid;
        return this;
    }

    public Params setHeaterOn(boolean heater_on) {
        this.heater_on = heater_on;
        return this;
    }

    public Params setDuration(int duration) {
        this.duration = duration;
        return this;
    }

    public Params setLimit(int limit) {
        this.limit = limit;
        return this;
    }

    public Params setTimeFrom(float ts_from) {
        this.ts_from = ts_from;
        return this;
    }

    public Params setTimeTo(float ts_to) {
        this.ts_to = ts_to;
        return this;
    }

    public Params setMin(double min) {
        this.min = min;
        return this;
    }

    public Params setMax(double max) {
        this.max = max;
        return this;
    }

    public Params setEnable(boolean enable) {
        this.enable = enable;
        return this;
    }
}

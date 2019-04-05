package net.zeevox.myhome.json;

public class Params {
    int sid;
    int subid;
    boolean heater_on;
    int duration;
    int limit;
    float ts_from;
    float ts_to;
    double min;
    double max;
    boolean enable;

    public Params() {
    }

    public Params setSid(int sid) {
        this.sid = sid;
        return this;
    }

    public Params setSubid(int subid) {
        this.subid = subid;
        return this;
    }

    public Params setHeater_on(boolean heater_on) {
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

    public Params setTs_from(float ts_from) {
        this.ts_from = ts_from;
        return this;
    }

    public Params setTs_to(float ts_to) {
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

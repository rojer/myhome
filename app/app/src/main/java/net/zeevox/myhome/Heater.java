package net.zeevox.myhome;

public class Heater {
    public static final String ON = "heater_on";
    public static final String DURATION = "duration";
    private final boolean on;
    private final int duration;

    public Heater(boolean on, int duration) {
        this.on = on;
        this.duration = duration;
    }

    public boolean isOn() {
        return on;
    }

    public int getDuration() {
        return duration;
    }

    public boolean isInAutomaticMode() {
        return duration == -1;
    }

    public boolean isInManualMode() {
        return duration >= 0;
    }
}

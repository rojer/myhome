package net.zeevox.myhome;

import java.util.ArrayList;
import java.util.List;

public class Sensors {
    private final List<Sensor> list = new ArrayList<>();

    public List<Sensor> getList() {
        return list;
    }

    public Sensor getBySID(int SID) {
        for (Sensor tempSensor : list) {
            if (tempSensor.getSID() == SID) {
                return tempSensor;
            }
        }
        return null;
    }
}

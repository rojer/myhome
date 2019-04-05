package net.zeevox.myhome.json;

@SuppressWarnings({"FieldCanBeLocal", "unused"})
public class CustomJsonObject {
    private String method;
    private int id;
    private Params params;

    public CustomJsonObject setMethod(String method) {
        this.method = method;
        return this;
    }

    public CustomJsonObject setId(int id) {
        this.id = id;
        return this;
    }

    public CustomJsonObject setParams(Params params) {
        this.params = params;
        return this;
    }
}
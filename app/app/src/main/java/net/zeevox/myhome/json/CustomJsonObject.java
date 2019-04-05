package net.zeevox.myhome.json;

public class CustomJsonObject {
    String method;
    int id;
    Params params;

    public CustomJsonObject setMethod(String method) {
        this.method = method;
        return this;
    }

    public CustomJsonObject setId(int id) {
        this.id = id;
        return this;
    }

    public Params initParams() {
        params = new Params();
        return params;
    }

    public Params getParams() {
        return params;
    }

    public CustomJsonObject setParams(Params params) {
        this.params = params;
        return this;
    }
}
package net.zeevox.myhome;

import android.util.Log;

import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class WebSocketUtils {

    private WebSocket webSocket;

    public void connectWebSocket(String url, WebSocketListener webSocketListener) {
        Request request = new Request.Builder().url(url).build();
        OkHttpClient okHttpClient = new OkHttpClient.Builder()
                .connectTimeout(1000, TimeUnit.MILLISECONDS).build();
        webSocket = okHttpClient.newWebSocket(request, webSocketListener);
        Log.d(getClass().getSimpleName(), "WebSocket created");
    }

    public WebSocket getWebSocket() {
        return webSocket;
    }
}

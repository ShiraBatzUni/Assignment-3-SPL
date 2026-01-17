package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionHandler;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;

public class ConnectionsImpl<T> implements Connections<T> {

    private final Map<Integer, ConnectionHandler<T>> clients = new ConcurrentHashMap<>();
    private final Map<String, ConcurrentHashMap<Integer, String>> channelToSubscribers = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = clients.get(connectionId);
        if (handler != null) {
            handler.send(msg); 
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentHashMap<Integer, String> subscribers = channelToSubscribers.get(channel);
        if (subscribers != null) {
            for (Integer connId : subscribers.keySet()) {
                send(connId, msg);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        clients.remove(connectionId);
        for (Map<Integer, String> subscribers : channelToSubscribers.values()) {
            subscribers.remove(connectionId);
        }
    }

    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        clients.put(connectionId, handler);
    }

    public void subscribe(String channel, int connectionId, String subscriptionId) {
        channelToSubscribers.computeIfAbsent(channel, k -> new ConcurrentHashMap<>())
                            .put(connectionId, subscriptionId);
    }

    public void unsubscribe(String channel, int connectionId) {
        Map<Integer, String> subscribers = channelToSubscribers.get(channel);
        if (subscribers != null) {
            subscribers.remove(connectionId);
        }
    }

    public Map<Integer, String> getSubscribers(String channel) {
    return channelToSubscribers.get(channel);
}

}
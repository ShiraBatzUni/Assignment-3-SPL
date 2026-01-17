package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionHandler;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Set;
import java.util.Map;

public class ConnectionsImpl<T> implements Connections<T> {
    private Map<Integer, ConnectionHandler<T>> clients = new ConcurrentHashMap<>();
    private Map<String, Set<Integer>> channelToSubscribers = new ConcurrentHashMap<>();

    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = clients.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        } else {
            return false;
        }

    }

    public void send(String channel, T msg) {
        Set<Integer> subscribers = channelToSubscribers.get(channel);
        for (Integer id : subscribers) {
            send(id, msg);
        }
    }

    public void disconnect(int connectionId) {
        clients.remove(connectionId);
        for (Set<Integer> subscribers : channelToSubscribers.values()) {
            subscribers.remove(connectionId);
        }
    }

    public void register(int connectionId, ConnectionHandler<T> handler) {
        clients.put(connectionId, handler);
    }

    public void subscribe(int connectionId, String channel) {
        channelToSubscribers.computeIfAbsent(channel, k -> ConcurrentHashMap.newKeySet()).add(connectionId);
    }

    public void unsubscribe(int connectionId, String channel) {
        Set<Integer> subscribers = channelToSubscribers.get(channel);
        if (subscribers != null) {
            subscribers.remove(connectionId);
            if (subscribers.isEmpty()) {
                channelToSubscribers.remove(channel);
            }
        }
    }
}

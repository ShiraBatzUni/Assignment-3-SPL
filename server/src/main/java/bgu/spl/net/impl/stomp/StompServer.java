package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {

        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <tpc/reactor>");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args[1];
        int numOfThreads = 4; 

        if (serverType.equals("tpc")) {
            Server.threadPerClient(
                port,
                StompMessagingProtocolImpl::new, 
                StompMessageEncoderDecoder::new
            ).serve();
        } 
        else if (serverType.equals("reactor")) { 
            Server.reactor(
                numOfThreads,
                port,
                StompMessagingProtocolImpl::new, 
                StompMessageEncoderDecoder::new
            ).serve();
        } 
        else {
            System.out.println("Unknown server type. Use 'tpc' or 'reactor'.");
        }
    }
}
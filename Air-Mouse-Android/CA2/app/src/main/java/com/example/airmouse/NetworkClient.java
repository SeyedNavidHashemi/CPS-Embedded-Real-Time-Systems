package com.example.airmouse;

import android.util.Log;
import android.os.Trace;

import org.json.JSONObject;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class NetworkClient {

    // Network configuration
    private String serverIpAddress;
    private int serverPort;
    private DatagramSocket udpSocket;
    private InetAddress serverAddress;

    // Thread management
    private ExecutorService networkExecutor;
    private boolean isRunning = false;

    // Flag to drop standard movement packets while awaiting critical ACKs
    private volatile boolean isWaitingForAck = false;

    public NetworkClient(String ipAddress, int port) {
        this.serverIpAddress = ipAddress;
        this.serverPort = port;
    }

    // Starts the UDP socket on a background thread.
    public void start() {
        isRunning = true;
        networkExecutor = Executors.newSingleThreadExecutor();

        networkExecutor.execute(() -> {
            try {
                udpSocket = new DatagramSocket();
                serverAddress = InetAddress.getByName(serverIpAddress);
                Log.d("AirMouseNetwork", "UDP socket started -> " + serverIpAddress + ":" + serverPort);
            } catch (Exception e) {
                Log.e("AirMouseNetwork", "Socket initialization failed", e);
            }
        });
    }

    // Stops the network executor and closes the UDP socket.
    public void stop() {
        isRunning = false;

        if (networkExecutor != null) {
            networkExecutor.shutdown();
        }

        if (udpSocket != null && !udpSocket.isClosed()) {
            udpSocket.close();
            Log.d("AirMouseNetwork", "UDP socket closed");
        }
    }

    public void sendMouseData(double deltaX, double deltaY, int clickEvent, int scrollEvent) {
        if (!isRunning || networkExecutor == null) return;

        // Drop normal movement packets while waiting for ACK from a critical click/scroll packet.
        if (isWaitingForAck && clickEvent == 0 && scrollEvent == 0) {
            return;
        }

        networkExecutor.execute(() -> {
            Trace.beginSection("AirMouse_UDP_Send");

            try {
                JSONObject payload = new JSONObject();
                payload.put("DeltaX", deltaX);
                payload.put("DeltaY", deltaY);
                payload.put("Click", clickEvent);
                payload.put("Scroll", scrollEvent);

                String message = payload.toString();
                byte[] buffer = message.getBytes();

                if (udpSocket != null && serverAddress != null) {
                    DatagramPacket packet = new DatagramPacket(
                            buffer,
                            buffer.length,
                            serverAddress,
                            serverPort
                    );

                    udpSocket.send(packet);
                    Log.d("AirMouseNetwork", "Sent: " + message);

                    // Wait for ACK and retry critical click/scroll packets if needed.
                    if (clickEvent != 0 || scrollEvent != 0) {

                        isWaitingForAck = true; // // Block normal movement packets
                        boolean ackReceived = false;
                        int retryCount = 0;

                        udpSocket.setSoTimeout(200);

                        byte[] receiveBuffer = new byte[256];
                        DatagramPacket receivePacket = new DatagramPacket(receiveBuffer, receiveBuffer.length);

                        while (!ackReceived && retryCount < 3) {
                            try {
                                udpSocket.receive(receivePacket);

                                String ackMessage = new String(
                                        receivePacket.getData(),
                                        0,
                                        receivePacket.getLength()
                                );

                                if (ackMessage.contains("ACK")) {
                                    ackReceived = true;
                                    Log.d("AirMouseNetwork", "ACK received");
                                }

                            } catch (SocketTimeoutException e) {
                                retryCount++;
                                Log.d("AirMouseNetwork", "ACK timeout retry " + retryCount);

                                if (retryCount < 3) {
                                    udpSocket.send(packet);
                                    Log.d("AirMouseNetwork", "Resending packet");
                                }
                            }
                        }

                        udpSocket.setSoTimeout(0);
                        isWaitingForAck = false; // Allow normal movement packets again
                    }
                }

            } catch (Exception e) {
                Log.e("AirMouseNetwork", "Send failed", e);
            } finally {
                Trace.endSection();
            }
        });
    }
}
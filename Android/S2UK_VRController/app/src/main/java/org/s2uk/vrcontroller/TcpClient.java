package org.s2uk.vrcontroller;

import android.content.Context;
import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class TcpClient {
    private static final String TAG = "TcpClientJVM";
    public static final int SERVER_PORT = 9775;
    private final ServerMessages inMessages;

    private String mServerIp;
    private String mServerMessage;
    private OnMessageReceived mMessageListener = null;
    private OnStatusChanged mStatusListener = null;

    // runtime control
    private volatile boolean mRun = false;
    private volatile long connectionTimestamp = 0L;
    private final Context context;

    // socket / streams
    private PrintWriter mBufferOut;
    private BufferedReader mBufferIn;
    private Socket mSocket;

    // sending queue
    private final LinkedBlockingQueue<String> sendQueue = new LinkedBlockingQueue<>(1000); // bounded for safety
    private Thread senderThread = null;

    /**
     * Default constructor
     */
    public TcpClient(Context context, ServerMessages messages, OnMessageReceived listener) {
        this(null, context, messages, listener, null);
    }

    /**
     * Constructor with server IP and message listener
     */
    public TcpClient(String serverIp, Context context, ServerMessages messages, OnMessageReceived messageListener) {
        this(serverIp, context, messages, messageListener, null);
    }

    /**
     * Full constructor with optional status listener
     */
    public TcpClient(String serverIp, Context context, ServerMessages messages, OnMessageReceived messageListener, OnStatusChanged statusListener) {
        this.inMessages = messages;
        this.mServerIp = serverIp;
        this.context = context;
        this.mMessageListener = messageListener;
        this.mStatusListener = statusListener;
    }

    /**
     * Change the server IP for this client instance at runtime
     */
    public synchronized void setServerIp(String serverIp) {
        this.mServerIp = serverIp;
    }

    public synchronized String getServerIp() {
        return mServerIp;
    }

    public void setOnStatusChanged(OnStatusChanged listener) {
        this.mStatusListener = listener;
    }

    public void setOnMessageReceived(OnMessageReceived listener) {
        this.mMessageListener = listener;
    }

    /**
     * Enqueue a message to be sent. Non-blocking from caller's perspective.
     * This is safe to call from UI thread.
     */
    public void sendMessage(String message) {
        if (message == null) return;
        boolean offered = sendQueue.offer(message);
        if (!offered) {
            notifyStatus("send failed, queue is full");
        }
    }

    /**
     * Close the connection and release the members
     */
    public void stopClient() {
        if(!mRun) return;
        try {
            sendMessage(TCP_Constants.TCP_CLIENT_CLOSED_CONNECTION);
        } catch (Exception ignored) { }

        mRun = false;
        connectionTimestamp = 0L;

        if (senderThread != null) {
            try {
                senderThread.join(800);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        // close socket to unblock any blocking reads
        synchronized (this) {
            if (mSocket != null) {
                try {
                    // signal EOF for the remote peer (optional)
                    try { mSocket.shutdownOutput(); } catch (IOException ignored) {}
                    mSocket.close();
                } catch (IOException e) {
                    Log.w(TAG, "stopClient: error closing socket", e);
                } finally {
                    mSocket = null;
                }
            }
        }

        // close writer
        synchronized (this) {
            if (mBufferOut != null) {
                try { mBufferOut.flush(); } catch (Exception ignored) {}
                try { mBufferOut.close(); } catch (Exception ignored) {}
                mBufferOut = null;
            }
            mBufferIn = null;
        }

        // clear queue to free resources
        sendQueue.clear();

        mMessageListener = null;
        mServerMessage = null;

        notifyStatus(context.getString(R.string.tcp_client_status_disconnected));
    }

    /**
     * Call this method to run the client connection loop.
     * Intended to be called from a background thread (e.g. Thread or Executor).
     */
    public void run() {
        if (mServerIp == null || mServerIp.isEmpty()) {
            notifyStatus("server IP wasn't set.");
            return;
        }

        mRun = true;
        notifyStatus(context.getString(R.string.tcp_client_status_connecting));

        try {
            InetAddress serverAddr = InetAddress.getByName(mServerIp);

            Socket socket = new Socket();
            try {
                socket.connect(new InetSocketAddress(serverAddr, SERVER_PORT), TCP_Constants.TCP_CLIENT_CONNECTION_TIMEOUT);
            } catch (SocketTimeoutException ste) {
                notifyStatus(context.getString(R.string.tcp_client_status_connection_timeout));
                try { socket.close(); } catch (IOException ignored) {}
                return;
            }

            synchronized (this) {
                mSocket = socket;
            }

            notifyStatus(context.getString(R.string.tcp_client_status_connected));
            connectionTimestamp = System.currentTimeMillis();

            try {
                synchronized (this) {
                    mBufferOut = new PrintWriter(new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8)), true);
                    mBufferIn = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                }

                // send login message
                if (mBufferOut != null && !mBufferOut.checkError()) {
                    mBufferOut.println(TCP_Constants.TCP_CLIENT_LOGIN_MSG);
                    mBufferOut.flush();
                } else {
                    notifyStatus("Sender output stream isn't available");
                }

                senderThread = new Thread(() -> {
                    try {
                        while (mRun || !sendQueue.isEmpty()) {
                            String msg = sendQueue.poll(50, TimeUnit.MILLISECONDS);
                            if (msg == null) continue;

                            try {
                                synchronized (this) {
                                    if (mBufferOut != null && !mBufferOut.checkError()) {
                                        mBufferOut.println(msg);
                                        mBufferOut.flush();
                                    } else {
                                        notifyStatus("Sender output stream isn't available");
                                    }
                                }
                            } catch (Exception e) {
                                notifyStatus("send error: " + e.getMessage());
                            }
                        }
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                    }
                }, "TcpClient-Sender");
                senderThread.start();

                // Listen for server messages
                while (mRun) {
                    try {
                        mServerMessage = mBufferIn.readLine();
                    } catch (IOException ioe) {
                        // readLine failed - probably socket closed
                        Log.w(TAG, "readLine failed (socket closed?)", ioe);
                        notifyStatus(context.getString(R.string.tcp_client_status_connection_null));
                        break;
                    }

                    if (mServerMessage != null && mMessageListener != null) {
                        mMessageListener.messageReceived(mServerMessage);
                        if(mServerMessage != null) {
                            inMessages.enqueue(mServerMessage);
                            mServerMessage = null;
                        }
                    } else if (mServerMessage == null) {
                        // remote end closed connection — break loop and update status
                        notifyStatus(context.getString(R.string.tcp_client_status_connection_closed));
                        Log.i(TAG, "Server closed connection (readLine null)");
                        break;
                    }
                }

                Log.i(TAG, "Previous Response: \"" + mServerMessage + "\".");
            } catch (Exception e) {
                notifyStatus("ERROR: " + e.getMessage());
            } finally {
                mRun = false;
                connectionTimestamp = 0L;

                if (senderThread != null) {
                    try {
                        senderThread.join(1000);
                    } catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                    }
                }

                synchronized (this) {
                    if (mSocket != null) {
                        try { mSocket.close(); } catch (IOException e) {
                            Log.w(TAG, "run -> error closing socket in finally", e);
                        } finally {
                            mSocket = null;
                        }
                    }
                }
            }

        } catch (Exception e) {
            notifyStatus(context.getString(R.string.tcp_client_status_fail));
        } finally {
            mRun = false;
            connectionTimestamp = 0L;
            // notifyStatus("DISCONNECTED");
        }
    }

    private void notifyStatus(String status) {
        Log.i(TAG, "Tcp-Client Status: " + status);
        if (mStatusListener != null) {
            try {
                mStatusListener.statusChanged(status);
            } catch (Exception e) {
                Log.w(TAG, "notifyStatus: status listener threw", e);
            }
        }
    }

    public boolean isRunning() {
        return mRun;
    }

    public long gotConnectedTime() {
        return connectionTimestamp;
    }

    public String getLastServerMessage() {
        return mServerMessage;
    }

    //Declare the interface. The method messageReceived(String message) must be implemented
    // by the calling class (e.g. activity) that listens for server messages.
    public interface OnMessageReceived {
        void messageReceived(String message);
    }

    // Interface to receive human-readable per-instance status updates
    public interface OnStatusChanged {
        void statusChanged(String status);
    }
}

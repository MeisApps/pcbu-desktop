package com.meisapps.pcbiounlock.server

import com.meisapps.pcbiounlock.utils.AESUtils
import com.meisapps.pcbiounlock.utils.io.Console
import java.net.InetAddress
import java.net.ServerSocket
import java.util.LinkedList

abstract class BasicServer(private val ip: String, private val port: Int, protected val encryptionKey: String) {
    private lateinit var serverSocket: ServerSocket
    private lateinit var acceptThread: Thread

    private val clients = LinkedList<ConnectedClient>()

    protected var isRunning = false

    fun start() {
        if(isRunning)
            return

        serverSocket = ServerSocket(port, 0, InetAddress.getByName(ip))
        acceptThread = Thread {
            try {
                while (isRunning) {
                    val socket = serverSocket.accept()
                    val client = ConnectedClient(socket, encryptionKey) { client, b -> onDataReceived(client, b) }
                    clients.add(client)
                }
            } catch (e: Exception) {
                isRunning = false
            }
        }

        isRunning = true
        acceptThread.start()

        Console.println("Server started.")
    }

    fun stop() {
        if(!isRunning)
            return

        isRunning = false
        for(client in clients)
            client.close()

        serverSocket.close()
        acceptThread.join()
        Console.println("Server stopped.")
    }

    protected abstract fun onPacketReceived(client: ConnectedClient, packetId: Int, packetData: ByteArray)

    private fun onDataReceived(client: ConnectedClient, b: ByteArray) {
        val packetId = b[0]
        val packetData = b.copyOfRange(1, b.size)
        if(packetData.isEmpty()) {
            Console.println("Malformed packet")
            return
        }

        val decryptedData = AESUtils.decryptPacketData(packetData, encryptionKey)
        onPacketReceived(client, packetId.toInt(), decryptedData)
    }
}
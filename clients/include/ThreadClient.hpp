#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

#include "Client.hpp"

class ThreadClient
{
	public:
		using ReadCallback         = std::function<void(std::string const& data)>;
		using WriteCallback        = std::function<void(size_t bytesWritten)>;
		using ErrorCallback        = std::function<void(std::string const& what)>;
		using DisconnectCallback   = std::function<void(void)>;

		// le callback vengono fissate alla costruzione, non sono più riassegnabili:
		// nessun mutex necessario per proteggerle
		ThreadClient(ReadCallback onRead,
					WriteCallback onWrite,
					ErrorCallback onError,
					DisconnectCallback onDisconnect);
		~ThreadClient(void);

		ThreadClient(ThreadClient const&) = delete;
		ThreadClient& operator=(ThreadClient const&) = delete;

		// connessione sincrona + avvio del worker thread
		void connect(std::string const& host, uint32_t port);

		// ferma il thread e chiude il socket
		void disconnect(void);

		// accoda un messaggio da inviare in modo asincrono (non blocca)
		void send(std::string const& data);

		// alternativa "pull" per il thread principale: aspetta dati in arrivo
		// ritorna false se il timeout scade senza dati (timeoutMs < 0 = infinito)
		bool waitForData(std::string& out, int timeoutMs = -1);

	private:
		void run(void);
		void wakeup(void);
		bool handleReadable(int sock, char* buf, size_t bufSize);
		void handleWritable(int sock);

		Client               client;
		std::thread          worker;
		std::atomic<bool>    running{false};

		// self-pipe per svegliare poll() da un altro thread (nuovo invio o stop)
		int wakeupFds[2] = {-1, -1};

		std::mutex              sendMutex;
		std::queue<std::string> sendQueue;
		std::string             currentSend;   // messaggio in corso di invio
		size_t                  currentOffset = 0;

		std::mutex              recvMutex;
		std::condition_variable recvCond;
		std::queue<std::string> recvQueue;

		// impostate una sola volta alla costruzione, mai più riassegnate
		const ReadCallback        readCb;
		const WriteCallback       writeCb;
		const ErrorCallback       errorCb;
		const DisconnectCallback  disconnectCb;
};
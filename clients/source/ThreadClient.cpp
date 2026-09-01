#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>

#include "ThreadClient.hpp"
#include "Exceptions.hpp"

ThreadClient::ThreadClient(ReadCallback onRead,
                                WriteCallback onWrite,
                                ErrorCallback onError,
                                DisconnectCallback onDisconnect) :
	readCb(std::move(onRead)),
	writeCb(std::move(onWrite)),
	errorCb(std::move(onError)),
	disconnectCb(std::move(onDisconnect))
{
}

ThreadClient::~ThreadClient(void)
{
	this->disconnect();
}

void ThreadClient::connect(std::string const& host, uint32_t port)
{
	// connessione sincrona riusando la logica già esistente in Client
	this->client.connect(host, port);

	if (pipe(this->wakeupFds) == -1)
		throw(ClientException("Failed to create wakeup pipe: " + std::string(strerror(errno))));

	// entrambe le estremità della pipe non-blocking
	for (int fd : {this->wakeupFds[0], this->wakeupFds[1]})
	{
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}

	this->running.store(true);
	this->worker = std::thread(&ThreadClient::run, this);
}

void ThreadClient::disconnect(void)
{
	if (!this->running.exchange(false))
		return; // già fermo

	this->wakeup();
	if (this->worker.joinable())
		this->worker.join();

	if (this->wakeupFds[0] != -1) close(this->wakeupFds[0]);
	if (this->wakeupFds[1] != -1) close(this->wakeupFds[1]);
	this->wakeupFds[0] = this->wakeupFds[1] = -1;

	// distrugge il Client corrente (chiude il socket) e lo rimpiazza con uno pulito
	this->client = Client();
}

void ThreadClient::send(std::string const& data)
{
	{
		std::lock_guard<std::mutex> lock(this->sendMutex);
		this->sendQueue.push(data + "\n");
	}
	this->wakeup(); // sblocca la poll() così il worker vede subito il nuovo dato da inviare
}

void ThreadClient::wakeup(void)
{
	if (this->wakeupFds[1] != -1)
	{
		char byte = 'x';
		ssize_t ret = write(this->wakeupFds[1], &byte, 1);
		(void)ret; // se la pipe è piena va bene comunque, basta un byte per svegliare la poll
	}
}

bool ThreadClient::waitForData(std::string& out, int timeoutMs)
{
	std::unique_lock<std::mutex> lock(this->recvMutex);
	auto hasData = [this] { return !this->recvQueue.empty(); };

	if (timeoutMs < 0)
		this->recvCond.wait(lock, hasData);
	else if (!this->recvCond.wait_for(lock, std::chrono::milliseconds(timeoutMs), hasData))
		return false;

	out = std::move(this->recvQueue.front());
	this->recvQueue.pop();
	return true;
}

bool ThreadClient::handleReadable(int sock, char* buf, size_t bufSize)
{
	std::string accumulated;

	while (true)
	{
		ssize_t n = recv(sock, buf, bufSize, 0);

		if (n > 0)
		{
			accumulated.append(buf, static_cast<size_t>(n));
			continue; // magari ci sono altri dati subito disponibili
		}
		if (n == 0)
		{
			// il peer ha chiuso la connessione
			if (!accumulated.empty())
			{
				{
					std::lock_guard<std::mutex> lock(this->recvMutex);
					this->recvQueue.push(accumulated);
				}
				this->recvCond.notify_one();
				if (this->readCb)
					this->readCb(accumulated);
			}
			if (this->disconnectCb)
				this->disconnectCb();
			return false;
		}
		// n == -1
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			break; // niente altro da leggere per ora
		if (errno == EINTR)
			continue;

		if (this->errorCb)
			this->errorCb("recv failed: " + std::string(strerror(errno)));
		return false;
	}

	if (!accumulated.empty())
	{
		{
			std::lock_guard<std::mutex> lock(this->recvMutex);
			this->recvQueue.push(accumulated);
		}
		this->recvCond.notify_one();
		if (this->readCb)
			this->readCb(accumulated);
	}
	return true;
}

void ThreadClient::handleWritable(int sock)
{
	while (true)
	{
		if (this->currentSend.empty())
		{
			std::lock_guard<std::mutex> lock(this->sendMutex);
			if (this->sendQueue.empty())
				return; // nulla da inviare
			this->currentSend = std::move(this->sendQueue.front());
			this->sendQueue.pop();
			this->currentOffset = 0;
		}

		ssize_t n = ::send(sock, this->currentSend.data() + this->currentOffset,
		                    this->currentSend.size() - this->currentOffset, 0);

		if (n > 0)
		{
			this->currentOffset += static_cast<size_t>(n);
			if (this->currentOffset == this->currentSend.size())
			{
				if (this->writeCb)
					this->writeCb(this->currentSend.size());
				this->currentSend.clear();
				this->currentOffset = 0;
				continue; // magari c'è già un altro messaggio in coda
			}
			continue; // invio parziale, continua a scrivere
		}

		// n == -1
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return; // aspetta il prossimo POLLOUT, il messaggio parziale resta salvato
		if (errno == EINTR)
			continue;

		if (this->errorCb)
			this->errorCb("send failed: " + std::string(strerror(errno)));
		return;
	}
}

void ThreadClient::run(void)
{
	int sock = this->client.getSocket();
	constexpr size_t bufSize = 4096;
	char buf[bufSize];

	while (this->running.load())
	{
		struct pollfd fds[2];

		fds[0].fd = sock;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		{
			std::lock_guard<std::mutex> lock(this->sendMutex);
			if (!this->currentSend.empty() || !this->sendQueue.empty())
				fds[0].events |= POLLOUT;
		}

		fds[1].fd = this->wakeupFds[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		int ret = poll(fds, 2, -1);
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			if (this->errorCb)
				this->errorCb("poll failed: " + std::string(strerror(errno)));
			break;
		}

		if (fds[1].revents & POLLIN)
		{
			char tmp[64];
			while (read(this->wakeupFds[0], tmp, sizeof(tmp)) > 0)
				; // svuota la pipe di wakeup
		}

		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			if (this->disconnectCb)
				this->disconnectCb();
			break;
		}

		if (fds[0].revents & POLLOUT)
			this->handleWritable(sock);

		if (fds[0].revents & POLLIN)
		{
			if (!this->handleReadable(sock, buf, bufSize))
				break;
		}
	}
}
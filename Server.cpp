#include "Server.hpp"
#include "commands/commands.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <algorithm>
#include <iostream>

// Constructor
Server::Server(int port, const std::string& password) 
    : _port(port), _password(password), _socket(-1), _running(false)
{
    createServerSocket();
}

Server::~Server()
{
    stop();
}

void Server::createServerSocket()
{
    _socket = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket < 0)
        throw std::runtime_error("socket creation failed");

    int opt = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(_socket);
        _socket = -1;
        throw std::runtime_error("setsockopt failed");
    }

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(_port);

    if (bind(_socket, (struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        close(_socket);
        _socket = -1;
        throw std::runtime_error("bind failed");
    }

    if (listen(_socket, 42) < 0)
    {
        close(_socket);
        _socket = -1;
        throw std::runtime_error("listen failed");
    }

    // Set non-blocking
	if (fcntl(_socket, F_SETFL, O_NONBLOCK) < 0)
	{
        close(_socket);
        _socket = -1;
        throw std::runtime_error("failed to set server socket non-blocking");
    }
}

void Server::acceptClients()
{
    struct sockaddr_in csin;
    socklen_t csin_len = sizeof(csin);
    std::memset(&csin, 0, sizeof(csin));
    
    int cs = accept(_socket, (struct sockaddr*)&csin, &csin_len);
    if (cs < 0)
		return;

	if (fcntl(_socket, F_SETFL, O_NONBLOCK) < 0)
	{
		close(cs);
        return;
    }

    std::cout << "New client #" << cs << " from " 
              << inet_ntoa(csin.sin_addr) << ":" 
              << ntohs(csin.sin_port) << std::endl;

    // Crea il nuovo client
    Client newClient(cs, inet_ntoa(csin.sin_addr));
    _clients.push_back(newClient);
    
    // Aggiorna il poll set
    buildPollSet();
}

void Server::broadcastOtherChannelMembers(Channel* channel, Client* sender, const std::string& message)
{
    if (!channel)
        return;
    
    const std::vector<Client*>& clients = channel->getClients();
    for (std::vector<Client*>::const_iterator it = clients.begin(); 
         it != clients.end(); ++it)
    {
        Client* client = *it;
        if (client && client->getFd() != sender->getFd())
            sendReply(*client, message);
    }
}

void Server::broadcastToChannel(Channel* channel, const std::string& message)
{
    if (!channel)
        return;

    const std::vector<Client*>& clients = channel->getClients();
    for (std::vector<Client*>::const_iterator it = clients.begin();
        it != clients.end(); ++it)
    {
        if (*it)
			sendReply(**it, message);
    }
}

void Server::readFromClient(Client& client)
{
    int cs = client.getFd();
    int r = recv(cs, _readBuffer, BUF_SIZE, 0);
    
    if (r <= 0)
    {
        std::cout << "client #" << cs << " gone away" << std::endl;
        
        // Rimuovi il client da tutti i channel
        for (std::vector<Channel>::iterator it = _channels.begin();
             it != _channels.end(); ++it)
            it->removeClient(client);
        
        removeClient(cs);
    }
    else
    {
        _readBuffer[r] = '\0';
        std::cerr << "[IRC RECV fd=" << cs << "] " << r << " bytes" << std::endl;
        client.appendInput(std::string(_readBuffer, r));
        if (client.getInput().size() > MAX_CLIENT_BUFFER_SIZE)
        {
            client.clearInput();
            client.setClosing(true);
            return;
        }
        
        // Accept both RFC-compliant CRLF and clients that send LF only
        std::string input = client.getInput();
        size_t pos;
        while ((pos = input.find('\n')) != std::string::npos)
        {
            std::string command = input.substr(0, pos + 1);
            executeCommand(*this, client, command);
            input.erase(0, pos + 1);
            if (client.isClosing())
                break;
        }
        if (!client.isClosing())
            client.setInput(input);
    }
}

void Server::writeToClient(Client& client)
{
    if (client.hasOutput())
    {
        std::string output = client.getOutput();
        int sent = send(client.getFd(), output.c_str(), output.length(), MSG_NOSIGNAL);
        if (sent > 0)
            client.removeOutput(static_cast<std::size_t>(sent));
        else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            removeClient(client.getFd());
    }
}

void Server::removeClient(int fd)
{
    for (std::deque<Client>::iterator it = _clients.begin(); 
         it != _clients.end(); ++it)
    {
        if (it->getFd() == fd)
        {
            for (std::vector<Channel>::iterator channel = _channels.begin();
                 channel != _channels.end();)
            {
                channel->removeClient(*it);
                if (channel->getClientCount() == 0)
                    channel = _channels.erase(channel);
                else
                    ++channel;
            }
            close(fd);
            _clients.erase(it);
            break;
        }
    }
    buildPollSet();
}

void Server::buildPollSet()
{
    _pollSet.clear();
    
    // Aggiungi il socket del server
    struct pollfd serverPfd;
    serverPfd.fd = _socket;
    serverPfd.events = POLLIN;
    serverPfd.revents = 0;
    _pollSet.push_back(serverPfd);
    
    // Aggiungi i client
    for (std::deque<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        struct pollfd clientPfd;
        clientPfd.fd = it->getFd();
        clientPfd.events = POLLIN;
        if (it->hasOutput())
            clientPfd.events |= POLLOUT;
        clientPfd.revents = 0;
        _pollSet.push_back(clientPfd);
    }
}

void Server::run()
{
    _running = true;
    buildPollSet();
    
    while (_running)
    {
        int ret = poll(&_pollSet[0], _pollSet.size(), -1);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        
        // server socket
        if (_pollSet[0].revents & POLLIN)
            acceptClients();
        
        // clients
        for (std::size_t i = 1; i < _pollSet.size(); ++i)
        {
            if (_pollSet[i].revents & POLLIN)
            {
                Client* client = findClient(_pollSet[i].fd);
                if (client)
                    readFromClient(*client);
            }
            if (_pollSet[i].revents & POLLOUT)
            {
                Client* client = findClient(_pollSet[i].fd);
                if (client)
                    writeToClient(*client);
            }
            if (_pollSet[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                removeClient(_pollSet[i].fd);
        }
        for (std::deque<Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        {
            if (it->isClosing() && !it->hasOutput())
            {
                removeClient(it->getFd());
                break;
            }
        }
        buildPollSet();
    }
}

Client* Server::findClient(int fileDescriptor)
{
    for (std::deque<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (it->getFd() == fileDescriptor)
            return &(*it);
    }
    return NULL;
}

Client* Server::findClientByNickname(const std::string& nickname)
{
    for (std::deque<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (it->getNickname() == nickname)
            return &(*it);
    }
    return NULL;
}

Channel* Server::findChannel(const std::string& name)
{
    for (std::vector<Channel>::iterator it = _channels.begin();
         it != _channels.end(); ++it)
    {
        if (it->getName() == name)
            return &(*it);
    }
    return NULL;
}

void Server::stop()
{
    _running = false;
    if (_socket >= 0)
    {
        close(_socket);
        _socket = -1;
    }
    for (std::deque<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
        close(it->getFd());
    _clients.clear();
    _channels.clear();
    _pollSet.clear();
}

int Server::getPort() const { return _port; }
int Server::getSocket() const { return _socket; }
const std::string& Server::getPassword() const { return _password; }
const std::deque<Client>& Server::getClients() const { return _clients; }
std::vector<Channel>& Server::getChannels() { return _channels; }
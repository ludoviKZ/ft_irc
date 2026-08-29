#include "Server.hpp"
#include "commands/commands.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <algorithm>

// Constructor
Server::Server(int port, const std::string& password) 
    : _port(port), _password(password), _running(false)
{
    createServerSocket();
}

Server::~Server()
{
    stop();
}

Server::Server(const Server& other)
    : _port(other._port), _password(other._password), _socket(other._socket),
      _running(other._running), _clients(other._clients), _channels(other._channels),
      _pollSet(other._pollSet)
{
    std::memcpy(_readBuffer, other._readBuffer, BUF_SIZE + 1);
}

Server& Server::operator=(const Server& other)
{
    if (this != &other)
    {
        _port = other._port;
        _password = other._password;
        _socket = other._socket;
        _running = other._running;
        _clients = other._clients;
        _channels = other._channels;
        _pollSet = other._pollSet;
        std::memcpy(_readBuffer, other._readBuffer, BUF_SIZE + 1);
    }
    return *this;
}

void Server::createServerSocket()
{
    _socket = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket < 0)
        throw std::runtime_error("socket creation failed");

    int opt = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt failed");

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(_port);

    if (bind(_socket, (struct sockaddr*)&sin, sizeof(sin)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(_socket, 42) < 0)
        throw std::runtime_error("listen failed");

    // Set non-blocking
    int flags = fcntl(_socket, F_GETFL, 0);
    fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
}

void Server::acceptClients()
{
    struct sockaddr_in csin;
    socklen_t csin_len = sizeof(csin);
    std::memset(&csin, 0, sizeof(csin));
    
    int cs = accept(_socket, (struct sockaddr*)&csin, &csin_len);
    if (cs < 0)
        return;

    std::cout << "New client #" << cs << " from " 
              << inet_ntoa(csin.sin_addr) << ":" 
              << ntohs(csin.sin_port) << std::endl;

    // Crea il nuovo client
    Client newClient(cs, inet_ntoa(csin.sin_addr));
    _clients.push_back(newClient);
    
    // Aggiorna il poll set
    rebuildPollSet();
}

void Server::broadcastToChannel(Channel* channel, Client* sender, const std::string& message)
{
    if (!channel)
        return;
    
    const std::vector<Client*>& clients = channel->getClients();
    for (std::vector<Client*>::const_iterator it = clients.begin(); 
         it != clients.end(); ++it)
    {
        Client* client = *it;
        if (client && client->getFd() != sender->getFd())
        {
            send(client->getFd(), message.c_str(), message.length(), 0);
        }
    }
}


void Server::handleJoinCommand(Client* client, const std::string& channelName)
{
    if (!client)
        return;
    
    // Cerca il channel
    Channel* channel = findChannel(channelName);
    
    if (!channel)
    {
        // Crea un nuovo channel
        Channel newChannel(channelName);
        _channels.push_back(newChannel);
        channel = &_channels[_channels.size() - 1];
    }
    
    // Aggiungi il client al channel
    channel->addClient(*client);
    
    // Invia conferma al client
    std::string confirm = ":" + client->getNickname() + "!" + client->getUsername() + 
                          "@localhost JOIN " + channelName + "\r\n";
    send(client->getFd(), confirm.c_str(), confirm.length(), 0);
    
    // Invia il topic se presente
    if (!channel->getTopic().empty())
    {
        std::string topicMsg = ":localhost 332 " + client->getNickname() + 
                               " " + channelName + " :" + channel->getTopic() + "\r\n";
        send(client->getFd(), topicMsg.c_str(), topicMsg.length(), 0);
    }
}

void Server::handlePrivMsgCommand(Client* client, const std::string& target, const std::string& message)
{
    if (!client)
        return;
    
    // Costruisci il messaggio da inviare
    std::string msg = ":" + client->getNickname() + "!" + client->getUsername() + 
                      "@localhost PRIVMSG " + target + " :" + message + "\r\n";
    
    // Cerca se è un channel o un utente
    Channel* channel = findChannel(target);
    if (channel)
    {
        // È un channel - invia a tutti i membri del channel
        const std::vector<Client*>& members = channel->getClients();
        for (std::vector<Client*>::const_iterator it = members.begin();
             it != members.end(); ++it)
        {
            Client* member = *it;
            if (member && member->getFd() != client->getFd())
            {
                send(member->getFd(), msg.c_str(), msg.length(), 0);
            }
        }
    }
    else
    {
        // È un utente specifico
        Client* targetClient = findClientByNickname(target);
        if (targetClient)
        {
            send(targetClient->getFd(), msg.c_str(), msg.length(), 0);
        }
        else
        {
            // Utente non trovato
            std::string error = ":localhost 401 " + client->getNickname() + 
                               " " + target + " :No such nick/channel\r\n";
            send(client->getFd(), error.c_str(), error.length(), 0);
        }
    }
}

void Server::processCommand(Client* client, const std::string& command)
{
    if (!client)
        return;
    
    // Parser semplice dei comandi IRC
    std::string cmd = command;
    size_t pos = cmd.find("\r\n");
    if (pos != std::string::npos)
        cmd = cmd.substr(0, pos);
    
    // Tokenizza il comando
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(cmd);
    while (ss >> token)
        tokens.push_back(token);
    
    if (tokens.empty())
        return;
    
    // Comando NICK
    if (tokens[0] == "NICK" && tokens.size() >= 2)
    {
        client->setNickname(tokens[1]);
        client->updateRegistrationStatus();
        
        std::string msg = ":" + client->getNickname() + " NICK " + tokens[1] + "\r\n";
        // Broadcast del cambio nickname a tutti i client
        for (std::vector<Client>::iterator it = _clients.begin();
             it != _clients.end(); ++it)
        {
            if (it->getFd() != client->getFd())
                send(it->getFd(), msg.c_str(), msg.length(), 0);
        }
    }
    // Comando USER
    else if (tokens[0] == "USER" && tokens.size() >= 4)
    {
        client->setUsername(tokens[1]);
        client->updateRegistrationStatus();
    }
    // Comando JOIN
    else if (tokens[0] == "JOIN" && tokens.size() >= 2)
    {
        handleJoinCommand(client, tokens[1]);
    }
    // Comando PRIVMSG
    else if (tokens[0] == "PRIVMSG" && tokens.size() >= 3)
    {
        // Trova il messaggio (potrebbe contenere spazi)
        size_t start = cmd.find(tokens[1]) + tokens[1].length();
        while (cmd[start] == ' ')
            start++;
        if (cmd[start] == ':')
            start++;
        std::string msg = cmd.substr(start);
        
        handlePrivMsgCommand(client, tokens[1], msg);
    }
}

void Server::readFromClient(Client& client)
{
    int cs = client.getFd();
    int r = recv(cs, _readBuffer, BUF_SIZE, 0);
    
    if (r <= 0)
    {
        // Client disconnesso
        std::cout << "client #" << cs << " gone away" << std::endl;
        
        // Rimuovi il client da tutti i channel
        for (std::vector<Channel>::iterator it = _channels.begin();
             it != _channels.end(); ++it)
        {
            it->removeClient(client);
        }
        
        removeClient(cs);
    }
    else
    {
        _readBuffer[r] = '\0';
        client.appendInput(std::string(_readBuffer, r));
        
        // Processa i comandi (gestisce comandi multipli separati da \r\n)
        std::string input = client.getInput();
        size_t pos;
        while ((pos = input.find("\r\n")) != std::string::npos)
        {
            std::string command = input.substr(0, pos + 2);
            executeCommand(*this, client, command);
            input.erase(0, pos + 2);
        }
        client.setInput(input);
    }
}

void Server::writeToClient(Client& client)
{
    // Implementazione per inviare dati pendenti al client
    if (client.hasOutput())
    {
        std::string output = client.getOutput();
        int sent = send(client.getFd(), output.c_str(), output.length(), 0);
        if (sent > 0 && sent < static_cast<int>(output.length()))
        {
            // Se non tutto è stato inviato, mantieni il resto
            client.prependOutput(output.substr(sent));
        }
        else if (sent < 0)
        {
            // Errore di invio
            removeClient(client.getFd());
        }
    }
}

void Server::removeClient(int fd)
{
    for (std::vector<Client>::iterator it = _clients.begin(); 
         it != _clients.end(); ++it)
    {
        if (it->getFd() == fd)
        {
            close(fd);
            _clients.erase(it);
            break;
        }
    }
    rebuildPollSet();
}

void Server::removeClient(std::size_t index)
{
    if (index < _clients.size())
    {
        close(_clients[index].getFd());
        _clients.erase(_clients.begin() + index);
        rebuildPollSet();
    }
}

void Server::rebuildPollSet()
{
    _pollSet.clear();
    
    // Aggiungi il socket del server
    struct pollfd serverPfd;
    serverPfd.fd = _socket;
    serverPfd.events = POLLIN;
    serverPfd.revents = 0;
    _pollSet.push_back(serverPfd);
    
    // Aggiungi i client
    for (std::vector<Client>::iterator it = _clients.begin();
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

void Server::setupPollSet()
{
    rebuildPollSet();
}

void Server::run()
{
    _running = true;
    setupPollSet();
    
    while (_running)
    {
        int ret = poll(&_pollSet[0], _pollSet.size(), -1);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        
        // Controlla il socket del server
        if (_pollSet[0].revents & POLLIN)
        {
            acceptClients();
        }
        
        // Controlla i client
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
        }
    }
}

Client* Server::findClient(int fileDescriptor)
{
    for (std::vector<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if (it->getFd() == fileDescriptor)
            return &(*it);
    }
    return NULL;
}

Client* Server::findClientByNickname(const std::string& nickname)
{
    for (std::vector<Client>::iterator it = _clients.begin();
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

std::vector<Channel>& Server::getChannels()
{
    return _channels;
}

void Server::start()
{
    run();
}

void Server::stop()
{
    _running = false;
    if (_socket >= 0)
    {
        close(_socket);
        _socket = -1;
    }
    for (std::vector<Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        close(it->getFd());
    }
    _clients.clear();
}

int Server::getPort() const { return _port; }
int Server::getSocket() const { return _socket; }
const std::string& Server::getPassword() const { return _password; }
const std::vector<Client>& Server::getClients() const { return _clients; }
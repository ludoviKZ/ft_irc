#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include "Client.hpp"
#include "Channel.hpp"

#define BUF_SIZE 4096

class Server
{
public:
    Server(int port, const std::string& password);
    ~Server();
    Server(const Server& other);
    Server& operator=(const Server& other);
    
    void start();
    void run();
    void stop();

    // Getters
    int getPort() const;
    int getSocket() const;
    const std::string& getPassword() const;
    Client* findClient(int fileDescriptor);
    const std::vector<Client>& getClients() const;
    Channel* findChannel(const std::string& name);/////////
    std::vector<Channel>& getChannels();/////////////////

    // Metodi per la gestione dei client
    void acceptClients();
    void readFromClient(Client& client);
    void writeToClient(Client& client);
    void removeClient(int fd);
    void removeClient(std::size_t index);
    void rebuildPollSet();

private:
    Server();
    
    void createServerSocket();
    void setupPollSet();
    void broadcastToChannel(Channel* channel, Client* sender, const std::string& message);
    Client* findClientByNickname(const std::string& nickname);
    
    int _port;
    std::string _password;
    int _socket;
    bool _running;
    std::vector<Client> _clients;
    std::vector<Channel> _channels;
    std::vector<struct pollfd> _pollSet;
    
    char _readBuffer[BUF_SIZE + 1];
};

#endif
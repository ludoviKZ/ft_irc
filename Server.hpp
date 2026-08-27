#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>


#include "Client.hpp"
#include "Channel.hpp"

class Server
{
public:
	Server(int port, const std::string& password);
	~Server();
	Server(const Server& other);
	Server& operator=(const Server& other);
	//start the server
	void start();
	void run();
	void stop();


	//getters
	int getPort() const;
	int getSocket() const;
	const std::string& getPassword() const;
	Client* findClient(int fileDescriptor);
	const std::vector<Client>& getClients() const;

private:
	Server();
	void acceptClients();
	void readFromClient(Client& client);
	void writeToClient(Client& client);
	void removeClient(std::size_t index);
	void rebuildPollSet();

	int _port;
	std::string _password;
	int _socket;
	bool _running;
	std::vector<Client> _clients;
	std::vector<Channel> _channels;
	std::vector<struct pollfd> _pollSet;
};

#endif
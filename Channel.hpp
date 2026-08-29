#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "Client.hpp"

class Client;

class Channel
{
public:
	Channel();
	Channel(const std::string& name);
	~Channel();
	Channel(const Channel& other);
	Channel& operator=(const Channel& other);

	//getters
	const std::string& getName() const;
	const std::string& getTopic() const;
	const std::string& getKey() const;
	const std::string& getCreationTime() const;
	const std::vector<Client*>& getClients() const;
	const std::vector<Client*>& getOperators() const;
	int getClientCount() const;
	bool isInviteOnly() const;
	bool isTopicRestricted() const;
	bool hasKey() const;
	bool hasUserLimit() const;
	int getUserLimit() const;

	//setters
	void setName(const std::string& name);
	void setTopic(const std::string& topic);
	void setKey(const std::string& key);
	void setUserLimit(int limit);
	void setInviteOnly(bool enabled);
	void setTopicRestricted(bool enabled);

	//client management
	void addClient(Client& client);
	void removeClient(Client& client);
	void addOperator(Client& client);
	void removeOperator(Client& client);

private:
	std::string _name;
	std::string _topic;
	std::string _key;
	std::string _creationTime;
	int _userLimit;
	bool _inviteOnly;
	bool _topicRestricted;
	std::vector<Client*> _clients;
	std::vector<Client*> _operators;
};

#endif


#include "Channel.hpp"
#include "commands.hpp"
#include <ctime>
#include <cstring>

Channel::Channel() : _userLimit(0), _inviteOnly(false), _topicRestricted(false)
{
}

Channel::Channel(const std::string& name) 
    : _name(name), _userLimit(0), _inviteOnly(false), _topicRestricted(false)
{
    time_t now = time(NULL);
    char* timeStr = ctime(&now);
    if (timeStr)
    {
        _creationTime = std::string(timeStr);
        if (!_creationTime.empty())
            _creationTime.erase(_creationTime.size() - 1); // Rimuovi newline
    }
}

Channel::~Channel()
{
}

Channel::Channel(const Channel& other)
    : _name(other._name), _topic(other._topic), _key(other._key),
      _creationTime(other._creationTime), _userLimit(other._userLimit),
      _inviteOnly(other._inviteOnly), _topicRestricted(other._topicRestricted),
      _clients(other._clients), _operators(other._operators)
{
}

Channel& Channel::operator=(const Channel& other)
{
    if (this != &other)
    {
        _name = other._name;
        _topic = other._topic;
        _key = other._key;
        _creationTime = other._creationTime;
        _userLimit = other._userLimit;
        _inviteOnly = other._inviteOnly;
        _topicRestricted = other._topicRestricted;
        _clients = other._clients;
        _operators = other._operators;
    }
    return *this;
}

// Getters
const std::string& Channel::getName() const { return _name; }
const std::string& Channel::getTopic() const { return _topic; }
const std::string& Channel::getKey() const { return _key; }
const std::string& Channel::getCreationTime() const { return _creationTime; }
int Channel::getClientCount() const { return _clients.size(); }
bool Channel::isInviteOnly() const { return _inviteOnly; }
bool Channel::isTopicRestricted() const { return _topicRestricted; }
bool Channel::hasKey() const { return !_key.empty(); }
bool Channel::hasUserLimit() const { return _userLimit > 0; }
int Channel::getUserLimit() const { return _userLimit; }
const std::vector<Client*>& Channel::getClients() const { return _clients; }

// Setters
void Channel::setName(const std::string& name) { _name = name; }
void Channel::setTopic(const std::string& topic) { _topic = topic; }
void Channel::setKey(const std::string& key) { _key = key; }
void Channel::setUserLimit(Client& client, int limit)
{
	if (limit >= getClientCount())
		_userLimit = limit;
	else
		sendReply(client, "Cannot set. Users are already too many for this user limit.");
}
void Channel::setInviteOnly(bool enabled) { _inviteOnly = enabled; }
void Channel::setTopicRestricted(bool enabled) { _topicRestricted = enabled; }

// Client management
void Channel::addClient(Client& client)
{
    _clients.push_back(&client);
}

void Channel::removeClient(Client& client)
{
    for (std::vector<Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        if ((*it)->getFd() == client.getFd())
        {
            _clients.erase(it);
            break;
        }
    }
    removeOperator(client);
}

void Channel::addOperator(Client& client)
{
    _operators.push_back(&client);
}

void Channel::removeOperator(Client& client)
{
    for (std::vector<Client*>::iterator it = _operators.begin();
         it != _operators.end(); ++it)
    {
        if ((*it)->getFd() == client.getFd())
        {
            _operators.erase(it);
            break;
        }
    }
}

#include "../Server.hpp"
#include "../Channel.hpp"
#include "../Client.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <sys/socket.h>

void sendReply(Client& client, const std::string& message)
{
    if (client.getFd() < 0)
        return;
    send(client.getFd(), message.c_str(), message.length(), 0);
}

static Client* findClientByNickname(Server& server, const std::string& nickname)
{
    const std::vector<Client>& clients = server.getClients();
    for (std::vector<Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->getNickname() == nickname)
            return const_cast<Client*>(&(*it));
    }
    return NULL;
}

static std::string trimTrailingCRLF(const std::string& input)
{
    std::string result = input;
    while (!result.empty() && (result[result.length() - 1] == '\n' || result[result.length() - 1] == '\r'))
        result.erase(result.length() - 1);
    return result;
}

static std::vector<std::string> splitCommandLine(const std::string& input)
{
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    while (pos < input.length())
    {
        while (pos < input.length() && input[pos] == ' ')
            ++pos;
        if (pos >= input.length())
            break;

        if (input[pos] == ':')
        {
            tokens.push_back(input.substr(pos + 1));
            break;
        }

        std::size_t end = input.find(' ', pos);
        if (end == std::string::npos)
        {
            tokens.push_back(input.substr(pos));
            break;
        }
        tokens.push_back(input.substr(pos, end - pos));
        pos = end + 1;
    }
    return tokens;
}

static std::string clientNameOrStar(const Client& client)
{
    if (client.getNickname().empty())
        return "*";
    return client.getNickname();
}

static bool isRegistrationReady(const Client& client)
{
    return !client.getNickname().empty() && !client.getUsername().empty();
}

static bool isPasswordAccepted(const Server& server, const Client& client)
{
    return server.getPassword().empty() || client.isAuthenticated();
}

static void maybeCompleteRegistration(Server& server, Client& client)
{
    if (client.isRegistered() || !isRegistrationReady(client) || !isPasswordAccepted(server, client))
        return;

    sendReply(client, ":localhost 001 " + client.getNickname() + " :Welcome to the IRC server, " + client.getNickname() + "!" + client.getUsername() + "@localhost\r\n");
    sendReply(client, ":localhost 002 " + client.getNickname() + " :Your host is localhost, running version ft_irc\r\n");
    sendReply(client, ":localhost 003 " + client.getNickname() + " :This server was created today\r\n");
    sendReply(client, ":localhost 004 " + client.getNickname() + " localhost ft_irc o o\r\n");
    sendReply(client, ":localhost 422 " + client.getNickname() + " :MOTD File is missing\r\n");
    client.setRegistered(true);
}

static std::string joinParameters(const std::vector<std::string>& parameters, std::size_t start)
{
    std::string result;
    for (std::size_t i = start; i < parameters.size(); ++i)
    {
        if (i > start)
            result += " ";
        result += parameters[i];
    }
    return result;
}

static void handleJoin(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (parameters.empty())
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " JOIN :Not enough parameters\r\n");
        return;
    }

    std::string channelName = parameters[0];
    Channel* channel = server.findChannel(channelName);
    if (channel == NULL)
    {
        Channel newChannel(channelName);
        server.getChannels().push_back(newChannel);
        channel = &server.getChannels()[server.getChannels().size() - 1];
    }

    if (channel->isInviteOnly())
    {
        sendReply(client, ":localhost 473 " + client.getNickname() + " " + channelName + " :Cannot join channel (+i)\r\n");
        return;
    }

    channel->addClient(client);
    sendReply(client, ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName + "\r\n");

    if (!channel->getTopic().empty())
    {
        sendReply(client, ":localhost 332 " + client.getNickname() + " " + channelName + " :" + channel->getTopic() + "\r\n");
    }
}

static void handlePart(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (parameters.empty())
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " PART :Not enough parameters\r\n");
        return;
    }

    std::string channelName = parameters[0];
    Channel* channel = server.findChannel(channelName);
    if (channel == NULL)
    {
        sendReply(client, ":localhost 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n");
        return;
    }

    channel->removeClient(client);
    sendReply(client, ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PART " + channelName + "\r\n");
}

static void handleNick(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (parameters.empty())
    {
        sendReply(client, ":localhost 431 :No nickname given\r\n");
        return;
    }

    std::string newNick = parameters[0];
    if (newNick.empty())
    {
        sendReply(client, ":localhost 431 :No nickname given\r\n");
        return;
    }

    Client* existing = findClientByNickname(server, newNick);
    if (existing != NULL && existing->getFd() != client.getFd())
    {
        sendReply(client, ":localhost 433 " + clientNameOrStar(client) + " " + newNick + " :Nickname is already in use\r\n");
        return;
    }

    std::string oldNick = client.getNickname();
    client.setNickname(newNick);
    if (!oldNick.empty())
        sendReply(client, ":" + oldNick + " NICK :" + newNick + "\r\n");
    maybeCompleteRegistration(server, client);
}

static void handleUser(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (client.isRegistered())
    {
        sendReply(client, ":localhost 462 " + client.getNickname() + " :You may not reregister\r\n");
        return;
    }

    if (parameters.size() < 4)
    {
        sendReply(client, ":localhost 461 " + clientNameOrStar(client) + " USER :Not enough parameters\r\n");
        return;
    }

    client.setUsername(parameters[0]);
    maybeCompleteRegistration(server, client);
}

static void handlePass(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (client.isRegistered())
    {
        sendReply(client, ":localhost 462 " + client.getNickname() + " :You may not reregister\r\n");
        return;
    }

    if (parameters.empty())
    {
        sendReply(client, ":localhost 461 " + clientNameOrStar(client) + " PASS :Not enough parameters\r\n");
        return;
    }

    if (server.getPassword() != parameters[0])
    {
        sendReply(client, ":localhost 464 " + clientNameOrStar(client) + " :Password incorrect\r\n");
        return;
    }

    client.setAuthenticated(true);
    maybeCompleteRegistration(server, client);
}

static void handleCap(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    (void)server;
    if (parameters.empty())
    {
        sendReply(client, ":localhost CAP " + clientNameOrStar(client) + " LS :\r\n");
        return;
    }

    std::string subCommand = parameters[0];
    std::transform(subCommand.begin(), subCommand.end(), subCommand.begin(), (int (*)(int))std::toupper);
    if (subCommand == "LS" || subCommand == "LIST")
        sendReply(client, ":localhost CAP " + clientNameOrStar(client) + " " + subCommand + " :\r\n");
    else if (subCommand == "REQ")
        sendReply(client, ":localhost CAP " + clientNameOrStar(client) + " NAK :" + (parameters.size() > 1 ? parameters[1] : "") + "\r\n");
}

static void handlePrivmsg(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    if (parameters.size() < 2)
    {
        sendReply(client, ":localhost 411 " + client.getNickname() + " :No recipient given\r\n");
        return;
    }

    std::string target = parameters[0];
    std::string message = joinParameters(parameters, 1);
    if (message.empty())
    {
        sendReply(client, ":localhost 412 " + clientNameOrStar(client) + " :No text to send\r\n");
        return;
    }

    std::string fullMessage = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost PRIVMSG " + target + " :" + message + "\r\n";

    Channel* channel = server.findChannel(target);
    if (channel != NULL)
    {
        const std::vector<Client*>& members = channel->getClients();
        for (std::vector<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
        {
            Client* member = *it;
            if (member != NULL && member->getFd() != client.getFd())
                sendReply(*member, fullMessage);
        }
        return;
    }

    Client* targetClient = findClientByNickname(server, target);
    if (targetClient != NULL)
    {
        sendReply(*targetClient, fullMessage);
        return;
    }

    sendReply(client, ":localhost 401 " + client.getNickname() + " " + target + " :No such nick/channel\r\n");
}

static void handlePing(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    (void)server;
    if (parameters.empty())
    {
        sendReply(client, ":localhost 409 " + client.getNickname() + " :No origin specified\r\n");
        return;
    }

    sendReply(client, "PONG :" + parameters[0] + "\r\n");
}

static void handleQuit(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    std::string reason = (parameters.empty()) ? "Client quit" : joinParameters(parameters, 0);
    sendReply(client, ":localhost ERROR :Closing Link: " + client.getNickname() + " (" + reason + ")\r\n");

    // Placeholder: call the server-side client removal logic after the socket is closed.
    // server.removeClient(client.getFd());
    (void)server;
}

static void handleMode(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    (void)server;
	if (!client.isOperator())
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " MODE :Only operators can use MODE command\r\n");
        return;
    }
    if (parameters.size() < 2)
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " MODE :Not enough parameters\r\n");
        return;
    }
	Client *targetClient;
	Channel *channel = server.findChannel(parameters[0]);
	if (parameters[1] == "+i")
		channel->setInviteOnly(true);
	else if (parameters[1] == "-i")
		channel->setInviteOnly(false);
	else if (parameters[1] == "+t")
		channel->setTopicRestricted(true);
	else if (parameters[1] == "-t")
		channel->setTopicRestricted(false);
	else if (parameters[1] == "+k")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE +k :Password parameter required\r\n");
        	return;
    	}
		channel->setKey(parameters[2]);
	}
	else if (parameters[1] == "-k")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE -k :Password parameter required\r\n");
        	return;
    	}
		if (channel->getKey() == parameters[2])
			channel->setKey("");
	}
	else if (parameters[1] == "+k")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE -k :Password parameter required\r\n");
        	return;
    	}
		if (channel->getKey() == parameters[2])
			channel->setKey("");
	}
	else if (parameters[1] == "+o")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE +o :User parameter required\r\n");
        	return;
    	}
		targetClient = findClientByNickname(server, parameters[2]);
		if (!targetClient)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE +o :No user with this Nick\r\n");
        	return;
    	}
		targetClient->setOperator(true);
	}
	else if (parameters[1] == "-o")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE -o :User parameter required\r\n");
        	return;
    	}
		targetClient = findClientByNickname(server, parameters[2]);
		if (!targetClient)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE -o :No user with this Nick\r\n");
        	return;
    	}
		targetClient->setOperator(false);
	}
	else if (parameters[1] == "+l")
	{
		if (parameters.size() < 3)
		{
       		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE +l :User limit value parameter required\r\n");
        	return;
    	}
		channel->setUserLimit(client, std::stoi(parameters[2]));
	}
	else if (parameters[1] == "-l")
		channel->setUserLimit(client, 0);
	else
		sendReply(client, ":localhost 461 " + client.getNickname() + " MODE :Invalid parameter\r\n");
    // Placeholder for channel/user mode handling.
    // server.handleModeCommand(&client, parameters);
    sendReply(client, ":localhost 324 " + client.getNickname() + " " + parameters[0] + " +\r\n");
}

static void handleWho(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    (void)server;
    if (parameters.empty())
    {
        sendReply(client, ":localhost 315 " + client.getNickname() + " * :End of /WHO list\r\n");
        return;
    }

    // Placeholder for WHO channel/user listing logic.
    // server.handleWhoCommand(&client, parameters);
    sendReply(client, ":localhost 352 " + client.getNickname() + " " + parameters[0] + " " + client.getUsername() + " localhost localhost " + client.getNickname() + " H :0 realname\r\n");
    sendReply(client, ":localhost 315 " + client.getNickname() + " " + parameters[0] + " :End of /WHO list\r\n");
}

void executeCommand(Server& server, Client& client, const std::string& rawCommand)
{
    std::string command = trimTrailingCRLF(rawCommand);
    if (command.empty())
        return;

    std::vector<std::string> tokens = splitCommandLine(command);
    if (tokens.empty())
        return;

    std::string name = tokens[0];
    std::transform(name.begin(), name.end(), name.begin(), (int (*)(int))std::toupper);

    std::vector<std::string> parameters;
    for (std::size_t i = 1; i < tokens.size(); ++i)
        parameters.push_back(tokens[i]);

    if (name == "CAP")
        handleCap(server, client, parameters);
    else if (name == "PASS")
        handlePass(server, client, parameters);
    else if (name == "NICK")
        handleNick(server, client, parameters);
    else if (name == "USER")
        handleUser(server, client, parameters);
    else if (name == "PONG")
        return;
    else if (!client.isRegistered())
        sendReply(client, ":localhost 451 " + clientNameOrStar(client) + " :You have not registered\r\n");
    else if (name == "JOIN")
        handleJoin(server, client, parameters);
    else if (name == "PART")
        handlePart(server, client, parameters);
    else if (name == "PRIVMSG")
        handlePrivmsg(server, client, parameters);
    else if (name == "PING")
        handlePing(server, client, parameters);
    else if (name == "QUIT")
        handleQuit(server, client, parameters);
    else if (name == "MODE")
        handleMode(server, client, parameters);
    else if (name == "WHO")
        handleWho(server, client, parameters);
    else
        sendReply(client, ":localhost 421 " + client.getNickname() + " " + name + " :Unknown command\r\n");
}

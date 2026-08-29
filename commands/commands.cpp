#include "../Server.hpp"
#include "../Channel.hpp"
#include "../Client.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

static void sendReply(Client& client, const std::string& message)
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
    std::istringstream stream(input);
    std::string token;

    while (stream >> token)
        tokens.push_back(token);
    return tokens;
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
    (void)server;
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

    client.setNickname(newNick);
    client.updateRegistrationStatus();
    sendReply(client, ":" + client.getNickname() + " NICK " + newNick + "\r\n");
}

static void handleUser(Server& server, Client& client, const std::vector<std::string>& parameters)
{
    (void)server;
    if (parameters.size() < 4)
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " USER :Not enough parameters\r\n");
        return;
    }

    client.setUsername(parameters[0]);
    client.updateRegistrationStatus();
    sendReply(client, ":localhost 001 " + client.getNickname() + " :Welcome to the IRC server, " + client.getNickname() + "!" + client.getUsername() + "@localhost\r\n");
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

    sendReply(client, ":localhost PONG " + parameters[0] + "\r\n");
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
    if (parameters.empty())
    {
        sendReply(client, ":localhost 461 " + client.getNickname() + " MODE :Not enough parameters\r\n");
        return;
    }

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

    if (name == "NICK")
        handleNick(server, client, parameters);
    else if (name == "USER")
        handleUser(server, client, parameters);
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

#include "registration.hpp"

#include "../Server.hpp"
#include "../Client.hpp"
#include "../commands/commands.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>

static Client* findClientByNickname(Server& server, const std::string& nickname)
{
    const std::deque<Client>& clients = server.getClients();
    for (std::deque<Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->getNickname() == nickname)
            return const_cast<Client*>(&(*it));
    }
    return NULL;
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

static bool hasOperator(const Server& server)
{
    const std::deque<Client>& clients = server.getClients();
    for (std::deque<Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->isOperator())
            return true;
    }
    return false;
}

static void sendMotd(Client& client)
{
    std::ifstream motdFile("motd");
    if (!motdFile)
    {
        sendReply(client, ":localhost 422 " + client.getNickname() + " :MOTD File is missing\r\n");
        return;
    }

    std::string line;
    while (std::getline(motdFile, line))
        sendReply(client, ":localhost 372 " + client.getNickname() + " :- " + line + "\r\n");
    sendReply(client, ":localhost 376 " + client.getNickname() + " :End of MOTD command\r\n");
}

static void maybeCompleteRegistration(Server& server, Client& client)
{
    if (client.isRegistered() || !isRegistrationReady(client) || !isPasswordAccepted(server, client))
        return;

    sendReply(client, ":localhost 001 " + client.getNickname() + " :Welcome to the IRC server, " + client.getNickname() + "!" + client.getUsername() + "@localhost\r\n");
    sendReply(client, ":localhost 002 " + client.getNickname() + " :Your host is localhost, running version ft_irc\r\n");
    sendReply(client, ":localhost 003 " + client.getNickname() + " :This server was created today\r\n");
    sendReply(client, ":localhost 004 " + client.getNickname() + " localhost ft_irc o o\r\n");
    sendMotd(client);
    client.setRegistered(true);
    if (!hasOperator(server))
        client.setOperator(true);
}

void handleNick(Server& server, Client& client, const std::vector<std::string>& parameters)
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

void handleUser(Server& server, Client& client, const std::vector<std::string>& parameters)
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

void handlePass(Server& server, Client& client, const std::vector<std::string>& parameters)
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

void handleCap(Server& server, Client& client, const std::vector<std::string>& parameters)
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
    else if (subCommand == "END")
        return;
    else
        sendReply(client, ":localhost CAP " + clientNameOrStar(client) + " NAK :" + subCommand + "\r\n");
}

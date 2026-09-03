#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>

class Server;
class Client;

void sendReply(Client& client, const std::string& message);
// static Client* findClientByNickname(Server& server, const std::string& nickname);
// static std::string trimTrailingCRLF(const std::string& input);
// static std::vector<std::string> splitCommandLine(const std::string& input);
// static std::string joinParameters(const std::vector<std::string>& parameters, std::size_t start);
// static void handleJoin(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handlePart(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handlePrivmsg(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handlePing(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handleQuit(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handleMode(Server& server, Client& client, const std::vector<std::string>& parameters);
// static void handleWho(Server& server, Client& client, const std::vector<std::string>& parameters);
void executeCommand(Server& server, Client& client, const std::string& rawCommand);

#endif
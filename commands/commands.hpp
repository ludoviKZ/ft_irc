#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <string>

class Server;
class Client;

void executeCommand(Server& server, Client& client, const std::string& rawCommand);

#endif

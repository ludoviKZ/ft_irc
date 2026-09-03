#ifndef REGISTRATION_HPP
#define REGISTRATION_HPP

#include <string>
#include <vector>

class Server;
class Client;

void handleCap(Server& server, Client& client, const std::vector<std::string>& parameters);
void handlePass(Server& server, Client& client, const std::vector<std::string>& parameters);
void handleNick(Server& server, Client& client, const std::vector<std::string>& parameters);
void handleUser(Server& server, Client& client, const std::vector<std::string>& parameters);

#endif

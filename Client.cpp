#include "Client.hpp"

Client::Client()
    : fd(-1), address(""), nickname(""), username(""), authenticated(false),
      operator_status(false), input(""), output("")
{
}

Client::Client(int fileDescriptor, const std::string& address)
    : fd(fileDescriptor), address(address), nickname(""), username(""),
      authenticated(false), operator_status(false), input(""), output("")
{
}

Client::~Client()
{
}

Client::Client(const Client& other)
    : fd(other.fd), address(other.address), nickname(other.nickname),
      username(other.username), authenticated(other.authenticated),
      operator_status(other.operator_status), input(other.input), output(other.output)
{
}

Client& Client::operator=(const Client& other)
{
    if (this != &other)
    {
        fd = other.fd;
        address = other.address;
        nickname = other.nickname;
        username = other.username;
        authenticated = other.authenticated;
        operator_status = other.operator_status;
        input = other.input;
        output = other.output;
    }
    return *this;
}

int Client::getFd() const
{
    return fd;
}

const std::string Client::getAddress() const
{
    return address;
}

const std::string Client::getNickname() const
{
    return nickname;
}

const std::string Client::getUsername() const
{
    return username;
}

bool Client::isRegistered() const
{
    return !nickname.empty() && !username.empty();
}

bool Client::isAuthenticated() const
{
    return authenticated;
}

bool Client::isOperator() const
{
    return operator_status;
}

void Client::appendInput(const std::string& data)
{
    input += data;
}

void Client::setNickname(const std::string nickname)
{
    this->nickname = nickname;
}

void Client::setUsername(const std::string username)
{
    this->username = username;
}

void Client::setAuthenticated(bool authenticated)
{
    this->authenticated = authenticated;
}

void Client::setOperator(bool operatorStatus)
{
    this->operator_status = operatorStatus;
}

void Client::updateRegistrationStatus()
{
    if (!nickname.empty() && !username.empty())
        authenticated = true;
    else
        authenticated = false;
}

bool Client::hasInput() const
{
    return !input.empty();
}

bool Client::hasOutput() const
{
    return !output.empty();
}

void Client::setInput(const std::string& data)
{
    input = data;
}

void Client::appendOutput(const std::string& data)
{
    output += data;
}

void Client::prependOutput(const std::string& data)
{
    output = data + output;
}

const std::string& Client::getInput() const
{
    return input;
}

const std::string& Client::getOutput() const
{
    return output;
}

void Client::clearInput()
{
    input.clear();
}

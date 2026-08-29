#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <cstddef>
#include <string>

class Client
{
public:
	//constructors and destructors
	Client();
	Client(int fileDescriptor, const std::string& address);
	~Client();
	Client(const Client& other);
	Client& operator=(const Client& other);

	//getters
	int getFd() const;
	const std::string getAddress() const;
	const std::string getNickname() const;
	const std::string getUsername() const;
	bool isRegistered() const;
	bool isAuthenticated() const;
	bool isOperator() const;

	//setters
	void setregistered(bool registered);
	void setAuthenticated(bool authenticated);
	void appendInput(const std::string& data);
	void setNickname(const std::string nickname);
	void setUsername(const std::string username);
	void setRegistered(bool registered);
	void setOperator(bool operatorStatus);
	void updateRegistrationStatus();
	

	//input/output management
	bool hasInput() const;
	bool hasOutput() const;
	void setInput(const std::string& data);
	void appendOutput(const std::string& data);
	void prependOutput(const std::string& data);
	const std::string& getInput() const;
	const std::string& getOutput() const;
	void clearInput();

private:
	int fd;
	std::string address;
	std::string nickname;
	std::string username;
	bool authenticated;
	bool registered;
	bool operator_status;
	std::string input;
	std::string output;
};

#endif
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include <string>
#include <iostream>
#include <fstream>
#include "http_connection.h"
#include <Windows.h>
#include "settings.h"



std::string settings::host;
std::string settings::port;
std::string settings::database;
std::string settings::name;
std::string settings::password;
unsigned int settings::local_port;

void httpServer(tcp::acceptor& acceptor, tcp::socket& socket)
{
	acceptor.async_accept(socket,
		[&](beast::error_code ec)
		{
			if (!ec)
				std::make_shared<HttpConnection>(std::move(socket))->start();
			httpServer(acceptor, socket);
		});
}


void get_set(std::string temp) {
	std::string word;
	std::string parameter;
	std::string value;

	for (auto it = temp.begin(); it != temp.end(); ++it) {
		if (*it != '=') {
			word += *it;
		}
		else {
			parameter = word;
			word.clear();
		}
	}
	value = word;
	if (parameter == "host") {
		settings::host = value;
	}
	else if (parameter == "port") {
		settings::port = value;
	}
	else if (parameter == "database") {
		settings::database = value;
	}
	else if (parameter == "name") {
		settings::name = value;
	}
	else if (parameter == "password") {
		settings::password = value;
	}
	else if (parameter == "finder_port") {
		settings::local_port = stoi(value);
	}
}

int main(int argc, char* argv[])
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	try
	{
		std::ifstream set("settings.ini");
		if (set.is_open()) {
			while (!set.eof()) {
				std::string temp;
				set >> temp;
				get_set(temp);
			}
		}

		auto const address = net::ip::make_address("0.0.0.0");
		unsigned int port = 8080;
		net::io_context ioc{ 1 };

		tcp::acceptor acceptor{ ioc, { address, 8080 } };
		tcp::socket socket{ ioc };
		httpServer(acceptor, socket);

		std::cout << "Open browser and connect to http://localhost:" << settings::local_port << " to see the web server operating" << std::endl;

		ioc.run();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
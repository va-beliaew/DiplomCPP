#include <iostream>
#include <vector>
#include <thread>
#include <pqxx/pqxx>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "http_utils.h"
#include <functional>
#include <fstream>
#include <map>
#include <cstring>
#include <cctype>
#include <boost/locale.hpp>
#include <Windows.h>
#include "settings.h"
#include "Parser.h"


std::mutex sql_mtx;
std::mutex mtx;
std::condition_variable cv;
std::queue<std::function<void()>> tasks;
bool exitThreadPool = false;
std::unique_ptr<pqxx::connection> conn;

std::string settings::host;
std::string settings::port;
std::string settings::database;
std::string settings::name;
std::string settings::password;
std::string settings::page;
int settings::rec_depth;
int settings::finder_port;


std::map<std::string, unsigned int> counting (const std::vector<std::string>& text) {
	std::map<std::string, unsigned int> result;
	for (auto str : text) {
		auto it = result.find(str);
		if (it == result.end()) {
			result.insert({ str, 1 });
		}
		else {
			it->second++;
		}
	}
	return result;
}

Link div_html_adress(std::string adr) {
	std::string temp;
	std::string protocol;
	std::string adress;
	std::string query;
	Link link;
	for (auto i = adr.begin(); i != adr.end(); i++) {
		temp += *i;
		if (*i == ':') {
			temp.pop_back();
			protocol = temp;
			temp.clear();
			i += 2;
		}
		else if (*i == '/') {
			temp.pop_back();
			adress = temp;
			temp.clear();
			for (auto it = i; it != adr.end(); ++it) {
				query += *it;
			}
			i = adr.end()-1;
		}
		
	}
	if (protocol == "http") {
		link.protocol = ProtocolType::HTTP;
	}
	else if (protocol == "https") {
		link.protocol = ProtocolType::HTTPS;
	}
	link.hostName = adress;
	link.query = query;
	return link;
}


void threadPoolWorker() {
	std::unique_lock<std::mutex> lock(mtx);
	while (!exitThreadPool || !tasks.empty()) {
		if (tasks.empty()) {
			cv.wait(lock);
		}
		else {
			auto task = tasks.front();
			tasks.pop();
			lock.unlock();
			task();
			lock.lock();
		}
	}
}
void parseLink(const Link& link, int depth, const std::string& connection_parameters, Parser& parser)
{
	try {
		
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		std::string html = getHtmlContent(link);

		if (html.size() == 0)
		{
			std::cout << "Failed to get HTML Content" << std::endl;
			return;
		}

		std::vector<std::string> text = parser.parsing(html);
		std::map<std::string, unsigned int> outcome = counting(text);
		std::set<std::string> got_links = parser.get_link(html, link);

		std::string path;
		link.protocol == ProtocolType::HTTPS ? path = "https://" : path = "http://";
		path += (link.hostName + link.query);

		pqxx::connection conn(connection_parameters.c_str());
			pqxx::work worker(conn);
			worker.exec("INSERT INTO Document (doc) VALUES (" + worker.quote(path) +") ON CONFLICT (doc) DO NOTHING");
			for (auto it = outcome.begin(); it != outcome.end(); ++it) {
				std::string word = it->first;
				int appearance = it->second;
				worker.exec("INSERT INTO Words (word) VALUES (" + worker.quote(word) +") ON CONFLICT (word) DO NOTHING");
				pqxx::result id_doc = worker.exec("SELECT doc_id FROM Document WHERE doc = " + worker.quote(path));
				pqxx::result id_word = worker.exec("SELECT word_id FROM Words WHERE word = " + worker.quote(word));
				int doc_id = id_doc[0]["doc_id"].as<int>();
				int word_id = id_word[0]["word_id"].as<int>();
				worker.exec("INSERT INTO Docword (word_id, doc_id, appearance) VALUES (" + worker.quote(word_id) + ", " +worker.quote(doc_id) + ", " + worker.quote(appearance) + ") ON CONFLICT (doc_id, word_id) DO NOTHING");

			}
			worker.commit();

		std::cout << "html content:" << std::endl;
		std::cout << html << std::endl;

		std::vector<Link> links;
		for (auto it = got_links.begin(); it != got_links.end(); ++it) {
			links.push_back(div_html_adress(*it));
		}
		if (depth > 0) {
			std::lock_guard<std::mutex> lock(mtx);

			size_t count = links.size();
			size_t index = 0;
			for (auto& subLink : links)
			{
				tasks.push([subLink, depth, &connection_parameters, &parser]() { parseLink(subLink, depth - 1, connection_parameters, parser); });
			}
			cv.notify_one();
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

}

void get_set(std::string temp) {
	std::string word;
	std::string parameter;
	std::string value;
	
	
		for(auto it = temp.begin(); it != temp.end(); ++it) {
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
		else if (parameter == "page") {
			settings::page = value;
		}
		else if (parameter == "rec_depth") {
			settings::rec_depth = stoi(value);
		}
		else if (parameter == "finder_port") {
			settings::finder_port = stoi(value);
		}
}



int main()
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	try {
		Parser parser;
		std::ifstream set("settings.ini");
		if (set.is_open()) {
			while (!set.eof()) {
				std::string temp;
				set >> temp;
				get_set(temp);
			}
		}
		else {
			throw std::exception("settings.ini isn't open!");
		}
		std::string connection_parameters = "host=" + settings::host + " port=" + settings::port + " dbname=" + settings::database + " user=" + settings::name + " password=" + settings::password;
		pqxx::connection conn (connection_parameters.c_str());
		pqxx::work worker(conn);
		pqxx::result r = worker.exec(
			"CREATE TABLE IF NOT EXISTS Words (word_id SERIAL PRIMARY KEY NOT NULL, word VARCHAR(100) UNIQUE);"
			"CREATE TABLE IF NOT EXISTS Document(doc_id SERIAL PRIMARY KEY, doc VARCHAR(200) UNIQUE);"
			"CREATE TABLE IF NOT EXISTS Docword (word_id INT, doc_id INT, PRIMARY KEY (word_id, doc_id), "
			"FOREIGN KEY (word_id) REFERENCES Words(word_id), FOREIGN KEY (doc_id) REFERENCES Document(doc_id), appearance INT);"
			
		);
		worker.commit();

		int numThreads = std::thread::hardware_concurrency();
		std::vector<std::thread> threadPool;

		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}

		Link link = div_html_adress(settings::page);
		

		{
			std::lock_guard<std::mutex> lock(mtx);
			tasks.push([link, &connection_parameters, &parser]() { parseLink(link, settings::rec_depth, connection_parameters, parser); });
			cv.notify_one();
		}


		std::this_thread::sleep_for(std::chrono::seconds(2));


		{
			std::lock_guard<std::mutex> lock(mtx);
			exitThreadPool = true;
			cv.notify_all();
		}

		for (auto& t : threadPool) {
			t.join();
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		
	}
	return 0;
}
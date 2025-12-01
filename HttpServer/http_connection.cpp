#include "http_connection.h"

#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <iostream>
#include <pqxx/pqxx>
#include <set>
#include <unordered_map>
#include "settings.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct Compare {
	bool operator()(std::pair<int, int> first, std::pair<int, int> second) {
		return first.second > second.second;
	}
};

std::string url_decode(const std::string& encoded) {
	std::string res;
	std::istringstream iss(encoded);
	char ch;

	while (iss.get(ch)) {
		if (ch == '%') {
			int hex;
			iss >> std::hex >> hex;
			res += static_cast<char>(hex);
		}
		else {
			res += ch;
		}
	}

	return res;
}

std::string convert_to_utf8(const std::string& str) {
	std::string url_decoded = url_decode(str);
	return url_decoded;
}

HttpConnection::HttpConnection(tcp::socket socket)
	: socket_(std::move(socket))
{
}

std::set<std::string> HttpConnection::parsing(const std::string& str) {

	std::set<std::string> words;
	std::string temp;
	int n = 0;
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
		if (*it == ' ' && n > 1) {
			words.insert(temp);
			temp.clear();
			n = -1;
		}
		else if (*it == ',' || *it == '+' && n > 1) {
			words.insert(temp);
			temp.clear();
			n = -1;
		}
		else if (*it == ' ', *it == ',', *it == '+' && n < 2) {
			--n;
			temp.clear();
		}
		else {
			temp += *it;
			++n;
		}
		
	}
	if (temp.size() > 1) {
		words.insert(temp);
	}
	return words;
}

void HttpConnection::using_database(const std::set<std::string>& words, std::vector<std::string>& results) {

	std::string connection_string = "host=" + settings::host + " port=" + settings::port + " dbname=" + settings::database + " user=" + settings::name + " password=" + settings::password;
	pqxx::connection conn(connection_string);
	std::vector<std::unordered_map<int, int>> query;
	for (auto& s : words) {
		std::unordered_map<int, int> temp;
		pqxx::work worker(conn);
		pqxx::result result = worker.exec(
			"SELECT d.doc_id, d.appearance FROM docword d "
			"WHERE(SELECT word_id FROM words WHERE word = " + worker.quote(s) + ") = word_id "
			"ORDER BY appearance DESC;"
			);
		if (result.empty()) {
			beast::ostream(response_.body()) << "Nothig found!";
			return;
		}
		else {
			for (auto r : result) {
	
				temp.emplace(r["doc_id"].as<int>(), r["appearance"].as<int>());
			}
			
		}
		worker.commit();
		query.push_back(temp);
	}
	
	std::unordered_map<int, int> included;
		for (auto& map : query) {
			for(auto& pair : map){
				if (included.find(pair.first) != included.end()) {
					included[pair.first] += pair.second;
				}
				else {
					included.emplace(pair.first, pair.second);
				}
			}
	}
		std::vector<std::pair<int, int>> sorted;
		for (auto& i : included) {
			sorted.emplace_back(i);
		}
		std::sort(sorted.begin(), sorted.end(), Compare());
		for (auto& pair : sorted) {
			pqxx::work worker(conn);
			pqxx::result result = worker.exec(
				"SELECT doc FROM document "
				"WHERE doc_id = " + worker.quote(pair.first)
			);
			for (auto s : result) {
				results.emplace_back(s["doc"].as<std::string>());
			}
			worker.commit();
		}
		
}

void HttpConnection::start()
{
	readRequest();
	checkDeadline();
}


void HttpConnection::readRequest()
{
	auto self = shared_from_this();

	http::async_read(
		socket_,
		buffer_,
		request_,
		[self](beast::error_code ec,
			std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);
			if (!ec)
				self->processRequest();
		});
}

void HttpConnection::processRequest()
{
	response_.version(request_.version());
	response_.keep_alive(false);

	switch (request_.method())
	{
	case http::verb::get:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponseGet();
		break;
	case http::verb::post:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponsePost();
		break;

	default:
		response_.result(http::status::bad_request);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body())
			<< "Invalid request-method '"
			<< std::string(request_.method_string())
			<< "'";
		break;
	}

	writeResponse();
}


void HttpConnection::createResponseGet()
{
	if (request_.target() == "/")
	{
		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Welcome!<p>\n"
			<< "<form action=\"/\" method=\"post\">\n"
			<< "    <label for=\"search\">Search:</label><br>\n"
			<< "    <input type=\"text\" id=\"search\" name=\"search\"><br>\n"
			<< "    <input type=\"submit\" value=\"Search\">\n"
			<< "</form>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::createResponsePost()
{
	if (request_.target() == "/")
	{
		std::string s = buffers_to_string(request_.body().data());

		std::cout << "POST data: " << s << std::endl;

		size_t pos = s.find('=');
		if (pos == std::string::npos)
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		std::string key = s.substr(0, pos);
		std::string value = s.substr(pos + 1);

		std::string utf8value = convert_to_utf8(value);

		
		if (key != "search")
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		std::vector<std::string> searchResult;
		std::set<std::string> find_words = parsing(utf8value);
		
		if (find_words.empty()) {
			beast::ostream(response_.body()) << "Uncorrect request! Try again!";
		}
		else {
			using_database(find_words, searchResult);
			
		}

		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Response:<p>\n"
			<< "<ul>\n";

		for (const auto& url : searchResult) {

			beast::ostream(response_.body())
				<< "<li><a href=\""
				<< url << "\">"
				<< url << "</a></li>";
		}

		beast::ostream(response_.body())
			<< "</ul>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::writeResponse()
{
	auto self = shared_from_this();

	response_.content_length(response_.body().size());

	http::async_write(
		socket_,
		response_,
		[self](beast::error_code ec, std::size_t)
		{
			self->socket_.shutdown(tcp::socket::shutdown_send, ec);
			self->deadline_.cancel();
		});
}

void HttpConnection::checkDeadline()
{
	auto self = shared_from_this();

	deadline_.async_wait(
		[self](beast::error_code ec)
		{
			if (!ec)
			{
				self->socket_.close(ec);
			}
		});
}
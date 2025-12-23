#include "http_utils.h"

#include <regex>
#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;

using tcp = boost::asio::ip::tcp;

bool isText(const boost::beast::multi_buffer::const_buffers_type& b)
{
	for (auto itr = b.begin(); itr != b.end(); itr++)
	{
		for (int i = 0; i < (*itr).size(); i++)
		{
			if (*((const char*)(*itr).data() + i) == 0)
			{
				return false;
			}
		}
	}

	return true;
}


std::string getHtmlContent(const Link& link_)
{

	std::string result;
	bool flagExit = false;
	const int maxTry = 3;
	int currentTry = 0;
	Link link = link_;

	try
	{
		while (!flagExit || currentTry < maxTry) {
			std::string host = link.hostName;
			std::string query = link.query;

			net::io_context ioc;

			if (link.protocol == ProtocolType::HTTPS)
			{

				ssl::context ctx(ssl::context::tlsv13_client);
				ctx.set_default_verify_paths();

				beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
				stream.set_verify_mode(ssl::verify_none);

				stream.set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
					return true;
					});


				if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
					beast::error_code ec{ static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
					throw beast::system_error{ ec };
				}

				ip::tcp::resolver resolver(ioc);
				get_lowest_layer(stream).connect(resolver.resolve(host, "https"));
				get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

				stream.handshake(ssl::stream_base::client);

				http::request<http::empty_body> req{ http::verb::get, query, 11 };

				req.set(http::field::host, host);
				req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

				http::write(stream, req);

				beast::flat_buffer buffer;
				http::response<http::dynamic_body> res;
				http::read(stream, buffer, res);
				int code = res.result_int();
				if (code == 301 || code == 302 || code == 307) {
					auto location = res.find(http::field::location);
					if (location != res.end()) {
						std::string new_location = location->value();
						link.div_html_adress(new_location);
						beast::error_code ec;
						stream.shutdown(ec);
						continue;
					}
				}
				else {

					flagExit = true;

					if (isText(res.body().data()))
					{
						result = buffers_to_string(res.body().data());
						break;
					}
					else
					{
						std::cout << "This is not a text link, bailing out..." << std::endl;
					}
				}
				
				beast::error_code ec;
				stream.shutdown(ec);
				if (ec == net::error::eof) {
					ec = {};
				}

				if (ec) {
					throw beast::system_error{ ec };
				}
			}
			else
			{
				tcp::resolver resolver(ioc);
				beast::tcp_stream stream(ioc);

				auto const results = resolver.resolve(host, "http");

				stream.connect(results);

				http::request<http::string_body> req{ http::verb::get, query, 11 };
				req.set(http::field::host, host);
				req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);


				http::write(stream, req);

				beast::flat_buffer buffer;

				http::response<http::dynamic_body> res;


				http::read(stream, buffer, res);
				int code = res.result_int();
				if (code == 301 || code == 302 || code == 307) {
					auto location = res.find(http::field::location);
					if (location != res.end()) {
						std::string new_location = location->value();
						link.div_html_adress(new_location);
						beast::error_code ec;
						stream.socket().shutdown(tcp::socket::shutdown_both, ec);
						continue;
					}
				}
				else {

					flagExit = true;

					if (isText(res.body().data()))
					{
						result = buffers_to_string(res.body().data());
					}
					else
					{
						std::cout << "This is not a text link, bailing out..." << std::endl;
					}
				}
					beast::error_code ec;
					stream.socket().shutdown(tcp::socket::shutdown_both, ec);

					if (ec && ec != beast::errc::not_connected)
						throw beast::system_error{ ec };

				
				
			}
			++currentTry;
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return result;
}
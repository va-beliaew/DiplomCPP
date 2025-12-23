#pragma once
#include <string>
#include <unordered_set>

enum class ProtocolType
{
	HTTP = 0,
	HTTPS = 1
};

struct Link
{
	ProtocolType protocol;
	std::string hostName;
	std::string query;
	std::string text;

	Link() {};
	inline Link(const std::string& str) : text(str){
		div_html_adress(text);
	}
	
	inline void div_html_adress(const std::string& adr) {
		std::string temp;
		std::string Protocol;
		std::string adress;
		std::string Query;

		for (auto i = adr.begin(); i != adr.end(); i++) {
			temp += *i;
			if (*i == ':') {
				temp.pop_back();
				Protocol = temp;
				temp.clear();
				i += 2;
			}
			else if (*i == '/') {
				temp.pop_back();
				adress = temp;
				temp.clear();
				
				for (auto it = i; it != adr.end(); ++it) {
					Query += *it;
				}
				
				i = adr.end() - 1;
			}

		}
		if (Query.empty() && adress.empty()) {
			adress = temp;
			Query = '/';
		}
		if (Protocol == "http") {
			protocol = ProtocolType::HTTP;
		}
		else if (Protocol == "https") {
			protocol = ProtocolType::HTTPS;
		}
		hostName = adress;
		query = Query;
		
	}


	bool operator==(const Link& l) const
	{
		return protocol == l.protocol
			&& hostName == l.hostName
			&& query == l.query;
	}

	Link& operator= (const Link& l) {
		if (l != *this) {
			protocol = l.protocol;
			hostName = l.hostName;
			query = l.query;
		}
		return *this;
	}
};

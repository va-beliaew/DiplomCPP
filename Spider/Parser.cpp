#include "Parser.h"
#include "link.h"


Parser::Parser() {
	xmlInitParser();
}

Parser::~Parser() {
	xmlCleanupParser();
}

void Parser::check_nodes(xmlNodePtr node, std::string & str) {
	for (xmlNodePtr current_node = node; current_node; current_node = current_node->next) {
		if (current_node->type == XML_TEXT_NODE) {
			xmlChar* content = xmlNodeGetContent(current_node);
			if (content) {
				std::string text = reinterpret_cast<const char*>(content);
				text = clean_text(text);
				if (!text.empty()) {
					if (!str.empty()) {
						str += " ";
					}
					str += text;
				}
				xmlFree(content);
			}
		}
		else if (current_node->type == XML_ELEMENT_NODE) {
			std::string node_name = reinterpret_cast<const char*>(current_node->name);
			if (node_name != "script" && node_name != "style") {
				check_nodes(current_node->children, str);
			}
		}
	}

}

std::string Parser::clean_text(const std::string& str) {
	std::string text = str;
	text.erase(0, text.find_first_not_of("\t\n\r"));
	text.erase(text.find_last_not_of("\t\n\r") + 1);
	std::string text_cleaned;
	bool last_space = false;
	for (auto ch : text) {
		(ch > 255 || ch < 0) ? ch = 0 : 0;
		if (std::isspace(ch)) {
			if (!last_space) {
				text_cleaned += " ";
				last_space = true;
			}
		}
		else {
			text_cleaned += ch;
			last_space = false;
		}
	}
	return text_cleaned;
}

void Parser::final_clean(const std::string& text, std::vector<std::string>& words) {

	std::vector<std::string> prev;
	bool next_word = true;
	std::string temp;
	int num = 0;
	for (char ch : text) {
		if (ch == ',' || ch == '.' || ch == '(' || ch == ')' || ch == '%' || ch == ' ' || ch == '[' || ch == ']' || ch == '\"' || ch == ';' || ch == '\t' || ch == '\b' || ch == ':' || ch == '#' || ch == '?' || ch == '!' || ch == '@' || ch == '+' || ch == '-' || ch == '*' || ch == '\\' || ch == '/' || ch == '\n' || ch == '\r' || ch == '\0' || ch == '\v' || ch == '=' || ch == '&' || ch == '^') {
			next_word = true;
		}
		else if (ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' || ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9') {

		}
		else {
			if (ch == '\'' && next_word) {

			}
			else if (ch == '\'' && !next_word) {
				temp += ch;
			}
			else {
				temp += ch;
				num++;
			}
		}
		if (next_word && num > 1) {
			if (*(temp.end() - 1) == '\'') {
				temp.erase(temp.end() - 1);
			}
			if (*(temp.begin()) == '\'') {
				temp.erase(temp.begin());
			}
			prev.push_back(temp);
			num = 0;
			temp.clear();
			next_word = false;
		}
		else if (next_word && num <= 1) {
			next_word = false;
			num = 0;
			temp.clear();
		}
	}

	boost::locale::generator gen;
	std::locale loc = gen("En_US.UTF8");
	for (auto i = prev.begin(); i != prev.end(); ++i) {
		words.push_back(boost::locale::to_lower(*i, loc));
	}

}

	std::vector<std::string> Parser::parsing(const std::string& html) {
		
		std::string result;
		std::vector<std::string> words;
		htmlDocPtr doc = htmlReadDoc(reinterpret_cast<const xmlChar*>(html.c_str()), nullptr, nullptr, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

		if (!doc) {
			return words;
		}

		check_nodes(doc->children, result);
		final_clean(result, words);
		xmlFreeDoc(doc);
		return words;
	}

	std::set<std::string> Parser::get_link(const std::string& str, const Link& link) {
		std::set<std::string> ordered_links;
		std::vector<std::string> links;
		htmlDocPtr doc = htmlReadDoc(reinterpret_cast<const xmlChar*>(str.c_str()), nullptr, nullptr, HTML_PARSE_RECOVER | HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR);
		if (!doc) {
			std::cerr << "Failed to parse HTML";
			return ordered_links;
		}
		xmlXPathContextPtr context = xmlXPathNewContext(doc);
		if (!context) {
			xmlFreeDoc(doc);
			return ordered_links;
		}
		xmlXPathObjectPtr result = xmlXPathEvalExpression((const xmlChar*)("//a/@href"), context);
		if (result && result->type == XPATH_NODESET) {
			xmlNodeSetPtr node = result->nodesetval;
			for (int i = 0; i < node->nodeNr; ++i) {
				xmlChar* content = xmlNodeGetContent(node->nodeTab[i]);
				if (content) {
					links.push_back(std::string(reinterpret_cast<const char*>(content)));
					xmlFree(content);
				}
			}
		}
		if (result) {
			xmlXPathFreeObject(result);
			xmlXPathFreeContext(context);
			xmlFreeDoc(doc);
		}

		for (auto it = links.begin(); it < links.end(); ) {
			if (!(*it).empty()) {
				std::string temp = *it;
				if (*temp.begin() == '#') {
					it = links.erase(it);
				}
				else if (*temp.begin() == '/') {
					std::string protocol;
					link.protocol == ProtocolType::HTTPS ? protocol = "https://" : protocol = "http://";
					*it = protocol + link.hostName + temp;
					++it;
				}
				else if (*temp.begin() == 'h' || *temp.begin() == 'H') {
					std::string prot;
					for (int i = 0; i < 4; ++i) {
						prot += temp[i];
					}
					if (prot != "http" && prot != "HTTP") {
						it = links.erase(it);
					}
					else {
						++it;
					}
				}
				else {
					*it = "https://" + temp;
					++it;
				}

			}
			else {
				it = links.erase(it);
			}
		}

		for (auto& s : links) {
			ordered_links.insert(s);
		}
		return ordered_links;
	}
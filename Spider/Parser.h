#pragma once
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <string>
#include <set>
#include <vector>
#include <boost/locale.hpp>
#include <iostream>
#include "link.h"

class Parser {

private:

	void check_nodes(xmlNodePtr node, std::string& str);
	std::string clean_text(const std::string& str);
	void final_clean(const std::string& text, std::vector<std::string>& words);

public:

	Parser();
	Parser(const Parser& p) = delete;
	Parser& operator=(const Parser& p) = delete;
	std::vector<std::string> parsing(const std::string& html);
	std::set<std::string> get_link(const std::string& str, const Link& link);
	~Parser();
};
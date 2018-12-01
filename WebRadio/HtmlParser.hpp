/*
 Copyright 2018 - Ivan Landry

 This file is part of WebRadio.

WebRadio is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

WebRadio is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with WebRadio.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef HTML_PARSER_HPP_
#define HTML_PARSER_HPP_

#include <unordered_map>
#include <string>
#include "Http.hpp"

namespace HtmlParser
{

std::unordered_map<std::string, std::string> parse(const std::string & html);
Http::Url extractVideoUrl(Http::Client &, const std::string & response);


}

#endif /* HTML_PARSER_HPP_ */






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



#ifndef UTILS_HPP
#define UTILS_HPP

#include <fstream>
#include <boost/utility/string_view.hpp>

#define LOG Utils::Logger::getLogger(Utils::fileName(__FILE__), __LINE__)


namespace Utils
{
constexpr const char * endChars(const char * chars)
{
    return *chars == '\0' ? chars : endChars(++chars);
}

constexpr const char * rfindSlash(const char * chars)
{
    return *chars == '/' ? ++chars : rfindSlash(--chars);
}

constexpr bool hasSlash(const char * chars)
{
    return *chars == '/' ? 
        true : 
        (*chars == '\0' ? false : hasSlash(++chars));
}

constexpr const char * fileName(const char * file)
{
    return hasSlash(file) ? 
        rfindSlash(endChars(file)) : file;
}

void saveFile(const std::string & filePath, const std::string & fileContent, std::ios_base::openmode);

std::string readFile(const std::string & fileName);

class Logger
{
    std::ofstream _ofs;

    public:
    Logger();


    static Logger& getLogger(boost::string_view fileName, int line);

    // no concurrent logging needed at the moment
    template<typename T>
    Logger& operator<<(const T& t)
    {
        _ofs << t; 
        // for debug
        //_ofs.flush();
        return *this;
    }
};

}


#endif /* UTILS_HPP */

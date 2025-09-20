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

#include "Utils.hpp"

#include <ctime>
#include <iomanip>

namespace Utils {
Logger::Logger()
    : _ofs("WebRadio.log", std::ofstream::trunc | std::ofstream::out) {}

Logger& Logger::getLogger(boost::string_view fileName, int line) {
  static Logger logger;
  const std::time_t now = std::time(nullptr);
  const std::tm tm = *std::localtime(&now);
  return logger << "\n"
                << "[" << std::put_time(&tm, "%T") << "]" << "[" << fileName
                << ":" << line << "]";
}

void saveFile(const std::string& filePath, const std::string& fileContent,
              std::ios_base::openmode mode) {
  std::ofstream ofs(filePath, mode);
  ofs << fileContent;
  ofs.close();
}

std::string readFile(const std::string& fileName) {
  std::ifstream ifs(fileName, std::ios::in | std::ios::ate);
  const std::size_t size = ifs.tellg();
  if (size >= 0) {
    std::string content(size, '\0');
    ifs.seekg(0, std::ios::beg);
    ifs.read(&content[0], size);
    return content;
  }
  LOG << "Failed to read file " << fileName;
  return std::string();
}

}  // namespace Utils

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

#ifndef HTTP_HPP_
#define HTTP_HPP_

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl.hpp>

#include <future>


namespace Http
{
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

class Client
{
    boost::asio::io_service & _ioService;
    tcp::resolver _resolver;
    ssl::stream<tcp::socket> _stream;
    boost::beast::flat_buffer _buffer;
    http::request<http::string_body> _request;
    http::response<http::string_body> _response;
    std::promise<std::string> _promise;

    void onShutdown(boost::system::error_code);
    void onRead(boost::system::error_code, size_t);
    void onWrite(boost::system::error_code, size_t);
    void onHandshake(boost::system::error_code);
    void onConnect(boost::system::error_code,
            tcp::resolver::iterator);
    void onResolve(boost::system::error_code, 
            tcp::resolver::iterator);

    public:
    Client(boost::asio::io_service &, ssl::context&);
    Client(const Client &) = delete;
    Client(Client &&) = default;
    ~Client() = default;

    void setRequestCookies(std::string cookies);
    std::string getResponseCookies() const;

    std::future<std::string> get(
            const std::string & host,
            const std::string & port,
            const std::string & target);

};


struct Url
{
    Url();
    Url(const std::string &);
    bool empty() const;

    std::string _host;
    std::string _target;
};


std::string decode(std::string::const_iterator begin, std::string::const_iterator end);


}

#endif /* HTTP_HPP */

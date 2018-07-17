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

#include "Http.hpp"
#include "Utils.hpp"

namespace Http 
{

namespace 
{

std::exception_ptr make_exception(boost::system::error_code err)
{
    return std::make_exception_ptr(boost::system::system_error(err));
}

char hexToInt(char c)
{
    // look ASCII table to get this
    return c <= '9' ? c - '0' : c - 'A' + 10;
}

}


std::string decode(std::string::const_iterator begin, std::string::const_iterator end)
{
    std::string output;
    output.reserve(std::distance(begin, end));
    for (auto it = begin; it != end; ++it)
    {
        if (*it == '%' && std::distance(it, end) > 2)
        {
            output += (hexToInt(*(++it)) << 4) | (hexToInt(*(++it)) & 0x0F);
        }
        else if (*it != '+')
        {
            output += *it;
        }
    }
    return output;
}


//////////// HTTP CLIENT //////////////////////////
Client::Client(boost::asio::io_context & ioService, ssl::context & ctx)
    : _ioService(ioService)
    , _resolver(_ioService)
    , _stream(_ioService, ctx)
    , _buffer()
    , _request()
    , _response()
    , _promise()
{}

void Client::onShutdown(boost::system::error_code err)
{
    if (err == boost::asio::error::eof)
    {
        err.assign(0, err.category());
    }
    if (err)
    {
        LOG << "onShutdown error : " << err.message();
    }
    else
    {
        LOG << "onShutdown success ";
    }
}


void Client::onRead(boost::system::error_code err, std::size_t nbBytes)
{
    if (err)
    {
        LOG << "onRead error : " << err.message();
        _promise.set_exception(make_exception(err));
    }
    else
    {
        LOG << " read success : " << nbBytes;/*<< _response*/;

        //    this->processResponse(_response.body());
        _promise.set_value(std::move(_response.body()));
        _stream.async_shutdown(
                [this](boost::system::error_code errShutdown)
                {
                this->onShutdown(errShutdown);
                });
    }
}


void Client::onWrite(boost::system::error_code err, std::size_t nbBytes)
{
    if (err)
    {
        LOG << "onWrite error : " << err.message();
        _promise.set_exception(make_exception(err));
    }
    else
    {
        LOG << "write success";
        http::async_read(_stream, _buffer, _response,
                [this](auto errRead, auto nbBytesRead)
                {
                this->onRead(errRead, nbBytesRead);
                });
    }
}


void Client::onHandshake(boost::system::error_code err)
{
    if (err)
    {
        LOG << "onHandshake err : " << err.message();
        _promise.set_exception(make_exception(err));
    }
    else
    {
        LOG << "handshake success ";
        http::async_write(_stream, _request, 
                [this] (boost::system::error_code errWrite, std::size_t nbBytes)
                {
                this->onWrite(errWrite, nbBytes);
                });
    }
}


void Client::onConnect(boost::system::error_code err, tcp::resolver::iterator itResolver)
{
    if (err)
    {
        LOG << "onConnect err : " << err.message();
        _promise.set_exception(make_exception(err));           
    }
    else
    {
        LOG << " connect success ";
        _stream.async_handshake(ssl::stream_base::client, 
                [this](boost::system::error_code errHandshake)
                {
                this->onHandshake(errHandshake);
                });
    }
}

void Client::onResolve(boost::system::error_code err, tcp::resolver::iterator endpoint)
{
    if (err)
    {
        LOG << "onResolve err : " << err.message();
        _promise.set_exception(make_exception(err));           
    }
    else
    {
        LOG << "onResolve success ";
        boost::asio::async_connect(_stream.next_layer(), endpoint,
                [this] (boost::system::error_code errConnect, tcp::resolver::iterator itResolver)
                {
                this->onConnect(errConnect, itResolver);
                });
    }
}


void Client::setRequestCookies(std::string cookies)
{
    LOG << "request cookie set to : " << cookies;
    _request.set(http::field::cookie, std::move(cookies));
}

std::string Client::getResponseCookies() const
{
    auto rangeCookies = _response.equal_range(http::field::set_cookie);
    std::string cookies;
    cookies.reserve(512);
    for (auto itCookies = rangeCookies.first; itCookies != rangeCookies.second; ++itCookies)
    {
        const boost::string_view cookie = itCookies->value().substr(0, itCookies->value().find(";") + 1);
        cookies += std::string(cookie.data(), cookie.size());   
    }
    if (!cookies.empty())
    {
        cookies.erase(cookies.size()-1);
    }
    LOG << "reponse set_cookie : " << cookies;
    return cookies;
}


std::future<std::string> Client::get(const std::string & host, const std::string & port, const std::string & target)
{
    _request.version(11);
    _request.method(http::verb::get);
    _request.target(target);
    _request.set(http::field::host, host);
    _request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    LOG << "Launch resolve on " << host << target;

    _resolver.async_resolve({host, port}, 
            [this](boost::system::error_code ec, tcp::resolver::iterator resolverIt)
            {
            this->onResolve(ec, resolverIt);
            });

    return _promise.get_future();
}



////////////// HTTP URL ///////////////////
Url::Url()
    : _host()
      , _target()
{}

Url::Url(const std::string& url)
{
    size_t begin = url.find("//");
    if (begin == std::string::npos)
    {
        LOG << "Could not find // begin in url : " << url;
        begin = 0;
    } 
    else
    {
        begin += 2;
    }

    size_t end = url.find("/", begin);
    if (end == std::string::npos)
    {
        LOG << "Could not find / end in url : " << url;
        end = url.size() - 1;
    }

    _host = url.substr(begin, end - begin);
    _target = url.substr(end);
}

bool Url::empty() const
{
    return _host.empty() && _target.empty();
}


} // namespace Http


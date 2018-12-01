cmake_minimum_required(VERSION 3.4)
project(WebRadio VERSION 1.0.0 LANGUAGES CXX)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
link_directories(/usr/local/lib)
include_directories(/usr/local/include)

add_executable(WebRadio WebRadio.cpp)


target_sources(WebRadio
    PRIVATE
        HtmlParser.cpp
        HtmlParser.hpp
        Http.cpp
        Http.hpp
        Audio.cpp
        Audio.hpp
        JavascriptEngine.cpp
        JavascriptEngine.hpp
        Utils.cpp
        Utils.hpp
)

# not using boost package because cmake version is too old for boost 1.66
target_link_libraries(WebRadio
    boost_program_options
    boost_system
    boost_thread
    Threads::Threads
    boost_context
    boost_fiber
    OpenSSL::SSL
    OpenSSL::Crypto
    avcodec
    avformat
    avutil
    swresample
    SDL2)




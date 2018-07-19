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
#include "JavascriptEngine.hpp"
#include "Http.hpp"
#include "Audio.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <thread>
#include <future>
#include <typeinfo>
#include <cstdlib>

enum class Quality : std::uint8_t
{
    Small = 0,
    Medium = 1,
    HD = 2
};

static const std::array<std::string, 3> qualityStrings 
{ "small", "medium", "hd" };

Quality findLowestQuality(const std::string & html, size_t beginStreams, size_t endStreams, size_t & qualityPos)
{
    for (int i = 0; i < qualityStrings.size(); ++i)
    {
        qualityPos = html.find("quality=" + qualityStrings[i], beginStreams);
        if (qualityPos != std::string::npos)
        {
            return Quality(i);
        }
    }
    return Quality::HD; 
}

std::map<std::string,std::string> parse(const std::string & html)
{
    std::map<std::string,std::string> streamInfos;
    // find url_encoded_fmt_stream_map value
    const static std::string streamMapTag("url_encoded_fmt_stream_map");

    const size_t beginStreamMap = html.find(streamMapTag);
    if (beginStreamMap == std::string::npos)
    {
        LOG << "parse error : " << streamMapTag << " not found";
    }
    else
    {
        const size_t endStreamMap = html.find('"', beginStreamMap + streamMapTag.size() + 3);

        if (endStreamMap == std::string::npos)
        {
            LOG << "parse error : " << streamMapTag << " end not found ";
        }
        
        size_t qualityPos = 0;
        const Quality lowestQuality = findLowestQuality(html, beginStreamMap, endStreamMap, qualityPos);

        
        if (qualityPos != std::string::npos)
        {
            const size_t beginStream = html.rfind(",", qualityPos);
            const size_t beginUrl = html.find("url=", beginStream) + 4;
            const size_t endUrl = html.find_first_of({'\n', '\\', ','}, beginUrl);

            streamInfos.insert(std::make_pair("url",
                        Http::decode(html.cbegin() + beginUrl, html.cbegin() + endUrl)));

            const size_t beginSig = html.find("0026s=", beginStream);
            if (beginSig != std::string::npos)
            {
                const size_t endSig = html.find_first_of({'\n', '\\', ','}, beginSig);
                streamInfos.insert(std::make_pair("s", 
                            Http::decode(html.cbegin() + beginSig + 6, html.cbegin() + endSig)));
            }
        }
        else
        {
            LOG << "no url found";
        }
    }
    return streamInfos;
}

Http::Url extractVideoUrl(Http::Client & jsClient, const std::string & response)
{
    LOG << "dump html on dumpHtml.txt ";
    Utils::saveFile("dumpHtml.txt", response, std::ofstream::out | std::ofstream::trunc);

    const std::map<std::string, std::string> & streamMap = parse(response);

    auto itUrl = streamMap.find("url");
    std::string urlStr = std::move(itUrl->second);
    if (itUrl != streamMap.cend())
    {
        LOG << "url found : " << urlStr;
    }
    else
    {
        LOG << "no url found";
        return Http::Url();
    }

    auto itSig = streamMap.find("s");
    if (itSig != streamMap.cend())
    {
        LOG << "encrypted signature found, must decipher it";
        LOG << "encrypted signature : " << itSig->second;

        const size_t beginJs = response.find("\"js\":\"");
        if (beginJs == std::string::npos)
        {
            LOG << "could not find js path begin";
            return Http::Url();
        }
        const size_t endJs = response.find_first_of({'}', ','}, beginJs);
        if (endJs == std::string::npos)
        {
            LOG << "could not find js path end";
            return Http::Url();
        }

        std::string jsPath;//(response.cbegin() + beginJs + 6, response.cbegin() + endJs - 1);
        jsPath.reserve(endJs - beginJs);
        std::copy_if(response.cbegin() + beginJs + 6, 
                response.cbegin() + endJs - 1,
                std::back_inserter(jsPath),
                [](char c) { return c != '\\'; });

        LOG << "js path found : " << jsPath;

        std::future<std::string> jsCodeFuture = jsClient.get("s.ytimg.com", "443", 
                jsPath +"?disable_polymer=true");

                
        std::chrono::seconds span(10);
        if (jsCodeFuture.wait_for(span) != std::future_status::ready)
        {
            LOG << "signature decoding timeout ";
            return Http::Url();
        }
        const std::string & jsCode = jsCodeFuture.get();
        LOG << "dump js code in js_code.txt";
        Utils::saveFile("js_code.txt", jsCode, std::ofstream::out | std::ofstream::trunc);

        const std::string & decodedSig = JSEngine::decipherSignature(jsCode, itSig->second);
        LOG << "decoded signature : " << decodedSig;
        urlStr += "&signature=" + decodedSig;

    }
    return Http::Url(urlStr);
}

int main(int argc, char * argv[])
{
    namespace po = boost::program_options;

    bool isDownload = false;
    bool isPlay = false;
    bool isRepeat = false;

    std::string publicUrlStr;
    try
    {

        po::options_description desc("Arguments");
        desc.add_options()
            ("help", "list command arguments")
            ("url", po::value<std::string>(&publicUrlStr)->required(), "Youtube video URL")
            ("download,D", "Download the video")
            ("play,P", "Play audio")
            ("repeat,R", "Repeat mode");

        po::positional_options_description p;
        po::variables_map argsMap;
        po::store(po::command_line_parser(argc, argv).
                options(desc).positional(p).run(), argsMap);

        isDownload = argsMap.count("download");
        isPlay = argsMap.count("play");
        isRepeat = argsMap.count("repeat");

        if (argsMap.count("help") || (!isDownload && !isPlay))
        {
            std::cout << "Usage: options_description [options] " << std::endl;
            std::cout << desc;
            return EXIT_SUCCESS;
        }

        po::notify(argsMap);
    }
    catch (const std::exception ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    boost::asio::io_context ioService(1); // run in a single thread
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
    ctx.set_default_verify_paths();

    Http::Client clientHtml(ioService, ctx);
    Http::Client clientJs(ioService, ctx);
    Http::Client clientVideo(ioService, ctx);

    Http::Url youtubeUrl(publicUrlStr);

    // //testing VEVO 
    //std::future<std::string> htmlFuture = clientHtml.get("www.youtube.com" , "443", 
    //        "/watch?has_verified=1&bpctr=9999999999&hl=en&disable_polymer=true&gl=US&v=f68VJQc7qys");
    
    std::future<std::string> htmlFuture = clientHtml.get(youtubeUrl._host, "443", youtubeUrl._target);        
    
    std::thread ioThread([&ioService]() {ioService.run();});

    std::function<void(void)> playAudioFct;
    std::function<void(void)> saveFileFct;

    const std::string & html = htmlFuture.get();

    const std::string & cookies = clientHtml.getResponseCookies();
    clientJs.setRequestCookies(cookies);

    Http::Url videoUrl = extractVideoUrl(clientJs, html);
    std::string videoData;

    if (!videoUrl.empty())
    {
        std::future<std::string> videoFuture = clientVideo.get(videoUrl._host, "443", videoUrl._target); 

        videoData = videoFuture.get();

        if (isPlay)
        {

            playAudioFct = [&videoData, isRepeat]()
            { Audio::playAudio(videoData, isRepeat); };
        }

        if (isDownload)
        {
            saveFileFct = [&videoData, &publicUrlStr]()
            { 
                Utils::saveFile("videoData"/*publicUrlStr*/,videoData, 
                    std::ofstream::binary | 
                    std::ofstream::out | 
                    std::ofstream::trunc);
            };
        } 
    } 

    if (playAudioFct && saveFileFct)
    {
        std::thread audioThread(playAudioFct);
        std::thread saveThread(saveFileFct);
        audioThread.join();
        saveThread.join();
    }
    else if (saveFileFct)
    {
        std::thread saveThread(saveFileFct);
        saveThread.join();
    }
    else if (playAudioFct)
    {
        std::thread audioThread(playAudioFct);
        audioThread.join();
    }


    ioThread.join();


    return EXIT_SUCCESS;
}

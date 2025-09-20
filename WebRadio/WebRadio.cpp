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

#include <boost/program_options.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <typeinfo>

#include "Audio.hpp"
#include "HtmlParser.hpp"
#include "Http.hpp"
#include "Utils.hpp"

int main(int argc, char* argv[]) {
  namespace po = boost::program_options;

  bool isDownload = false;
  bool isPlay = false;
  bool isRepeat = false;

  std::string publicUrlStr;
  try {
    po::options_description desc("Arguments");
    desc.add_options()("help", "list command arguments")(
        "url", po::value<std::string>(&publicUrlStr)->required(),
        "Youtube video URL")("download,D", "Download the video")(
        "play,P", "Play audio")("repeat,R", "Repeat mode");

    po::positional_options_description p;
    po::variables_map argsMap;
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(p).run(),
        argsMap);

    isDownload = argsMap.count("download");
    isPlay = argsMap.count("play");
    isRepeat = argsMap.count("repeat");

    if (argsMap.count("help") || (!isDownload && !isPlay)) {
      std::cout << "Usage: options_description [options] " << std::endl;
      std::cout << desc;
      return EXIT_SUCCESS;
    }

    po::notify(argsMap);
  } catch (const std::exception ex) {
    std::cerr << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  boost::asio::io_context ioService(1);  // run in a single thread
  boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
  ctx.set_default_verify_paths();

  Http::Client clientHtml(ioService, ctx);
  Http::Client clientJs(ioService, ctx);
  Http::Client clientVideo(ioService, ctx);

  Http::Url youtubeUrl(publicUrlStr);

  // //testing VEVO
  // std::future<std::string> htmlFuture = clientHtml.get("www.youtube.com" ,
  // "443",
  //        "/watch?has_verified=1&bpctr=9999999999&hl=en&disable_polymer=true&gl=US&v=f68VJQc7qys");

  std::future<std::string> htmlFuture =
      clientHtml.get(youtubeUrl._host, "443", youtubeUrl._target);

  std::thread ioThread([&ioService]() { ioService.run(); });

  std::function<void(void)> playAudioFct;
  std::function<void(void)> saveFileFct;

  const std::string& html = htmlFuture.get();

  const std::string& cookies = clientHtml.getResponseCookies();
  clientJs.setRequestCookies(cookies);

  Http::Url videoUrl = HtmlParser::extractVideoUrl(clientJs, html);
  std::string videoData;

  if (!videoUrl.empty()) {
    std::future<std::string> videoFuture =
        clientVideo.get(videoUrl._host, "443", videoUrl._target);

    videoData = videoFuture.get();

    if (isPlay) {
      playAudioFct = [&videoData, isRepeat]() {
        Audio::playAudio(videoData, isRepeat);
      };
    }

    if (isDownload) {
      saveFileFct = [&videoData, &publicUrlStr]() {
        Utils::saveFile(
            "videoData" /*publicUrlStr*/, videoData,
            std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
      };
    }
  }

  if (playAudioFct && saveFileFct) {
    std::thread audioThread(playAudioFct);
    std::thread saveThread(saveFileFct);
    audioThread.join();
    saveThread.join();
  } else if (saveFileFct) {
    std::thread saveThread(saveFileFct);
    saveThread.join();
  } else if (playAudioFct) {
    std::thread audioThread(playAudioFct);
    audioThread.join();
  }

  ioThread.join();

  return EXIT_SUCCESS;
}

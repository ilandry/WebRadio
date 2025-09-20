#include "HtmlParser.hpp"

#include <array>

#include "JavascriptEngine.hpp"
#include "Utils.hpp"

namespace HtmlParser {

namespace {
enum class Quality : std::uint8_t { Small = 0, Medium = 1, HD = 2 };

static const std::array<std::string, 3> qualityStrings{"small", "medium", "hd"};

Quality findLowestQuality(const std::string& html, size_t beginStreams,
                          size_t endStreams, size_t& qualityPos) {
  for (int i = 0; i < qualityStrings.size(); ++i) {
    qualityPos = html.find("quality=" + qualityStrings[i], beginStreams);
    if (qualityPos != std::string::npos) {
      return Quality(i);
    }
  }
  return Quality::HD;
}

}  // namespace

std::unordered_map<std::string, std::string> parse(const std::string& html) {
  std::unordered_map<std::string, std::string> streamInfos;
  // find url_encoded_fmt_stream_map value
  const static std::string streamMapTag("url_encoded_fmt_stream_map");

  const size_t beginStreamMap = html.find(streamMapTag);
  if (beginStreamMap == std::string::npos) {
    LOG << "parse error : " << streamMapTag << " not found";
  } else {
    const size_t endStreamMap =
        html.find('"', beginStreamMap + streamMapTag.size() + 3);

    if (endStreamMap == std::string::npos) {
      LOG << "parse error : " << streamMapTag << " end not found ";
    }

    size_t qualityPos = 0;
    const Quality lowestQuality =
        findLowestQuality(html, beginStreamMap, endStreamMap, qualityPos);

    if (qualityPos != std::string::npos) {
      const size_t beginStream = html.rfind(",", qualityPos);
      const size_t beginUrl = html.find("url=", beginStream) + 4;
      const size_t endUrl = html.find_first_of({'\n', '\\', ','}, beginUrl);

      streamInfos.insert(std::make_pair(
          "url",
          Http::decode(html.cbegin() + beginUrl, html.cbegin() + endUrl)));

      const size_t beginSig = html.find("0026s=", beginStream);
      if (beginSig != std::string::npos) {
        const size_t endSig = html.find_first_of({'\n', '\\', ','}, beginSig);
        streamInfos.insert(
            std::make_pair("s", Http::decode(html.cbegin() + beginSig + 6,
                                             html.cbegin() + endSig)));
      }
    } else {
      LOG << "no url found";
    }
  }
  return streamInfos;
}

Http::Url extractVideoUrl(Http::Client& jsClient, const std::string& response) {
  LOG << "dump html on dumpHtml.txt ";
  Utils::saveFile("dumpHtml.txt", response,
                  std::ofstream::out | std::ofstream::trunc);

  const auto& streamMap = parse(response);

  auto itUrl = streamMap.find("url");
  std::string urlStr = std::move(itUrl->second);
  if (itUrl != streamMap.cend()) {
    LOG << "url found : " << urlStr;
  } else {
    LOG << "no url found";
    return Http::Url();
  }

  auto itSig = streamMap.find("s");
  if (itSig != streamMap.cend()) {
    LOG << "encrypted signature found, must decipher it";
    LOG << "encrypted signature : " << itSig->second;

    const size_t beginJs = response.find("\"js\":\"");
    if (beginJs == std::string::npos) {
      LOG << "could not find js path begin";
      return Http::Url();
    }
    const size_t endJs = response.find_first_of({'}', ','}, beginJs);
    if (endJs == std::string::npos) {
      LOG << "could not find js path end";
      return Http::Url();
    }

    std::string jsPath;  //(response.cbegin() + beginJs + 6, response.cbegin() +
                         //endJs - 1);
    jsPath.reserve(endJs - beginJs);
    std::copy_if(response.cbegin() + beginJs + 6, response.cbegin() + endJs - 1,
                 std::back_inserter(jsPath), [](char c) { return c != '\\'; });

    LOG << "js path found : " << jsPath;

    std::future<std::string> jsCodeFuture =
        jsClient.get("s.ytimg.com", "443", jsPath + "?disable_polymer=true");

    std::chrono::seconds span(10);
    if (jsCodeFuture.wait_for(span) != std::future_status::ready) {
      LOG << "signature decoding timeout ";
      return Http::Url();
    }
    const std::string& jsCode = jsCodeFuture.get();
    LOG << "dump js code in js_code.txt";
    Utils::saveFile("js_code.txt", jsCode,
                    std::ofstream::out | std::ofstream::trunc);

    const std::string& decodedSig =
        JSEngine::decipherSignature(jsCode, itSig->second);
    LOG << "decoded signature : " << decodedSig;
    urlStr += "&signature=" + decodedSig;
  }
  return Http::Url(urlStr);
}

}  // namespace HtmlParser

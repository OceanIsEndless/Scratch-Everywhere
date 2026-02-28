#include <nlohmann/json.hpp>
#include "blockUtils.hpp"
#include "downloader.hpp"
#include <value.hpp>
#include <fstream>
#include <regex>

const std::string languageNamesFilePath = "gfx/ingame/languageNames.json";

Value lastText = Value("");
Value lastLang = Value("");
Value lastResult = Value("");

nlohmann::json& loadTranslateExtensionLanguages(const std::string& path = languageNamesFilePath) {
    static nlohmann::json languageNames;
    static std::string languageNamesPath;
    if (languageNamesPath == path) return languageNames;
    languageNamesPath = path;

    std::ifstream file(languageNamesPath);
    if (file.is_open()) {
        file >> languageNames;
        file.close();
    } else {
        Log::logWarning("Failed to load language names from " + path);
    }

    return languageNames;
}

SCRATCH_REPORTER_BLOCK(translate, getTranslate) {
    const Value textValue = Scratch::getInputValue(block, "WORDS", sprite);
    const std::string textString = textValue.asString();
    if (std::regex_match(textString, std::regex("/^\\d+$/"))) {
        return Value(textString);
    }
    const Value langValue = Scratch::getInputValue(block, "LANGUAGE", sprite);
    if (lastText.isIdenticalTo(textValue) && lastLang.isIdenticalTo(langValue)) {
        return Value(lastResult);
    }
#if defined(ENABLE_DOWNLOAD)
    // Get language code
    std::string langString = langValue.asString();
    std::transform(langString.begin(), langString.end(), langString.begin(), ::tolower);
    nlohmann::json names = loadTranslateExtensionLanguages();

    if (names["menuMap"].contains(langString)) {
        // langString is already a valid language code
    } else if (names["nameMap"].contains(langString)) {
        // langString is a valid name for a language code
        langString = names["nameMap"]["codes"][names["nameMap"][langString]];
    } else if (std::find(names["previouslySupported"].begin(), names["previouslySupported"].end(), langString) != names["previouslySupported"].end()) {
        // langString was a previously supported language code
    } else {
        // Defaults to English
        langString = "en";
    }

    // Interface with translate service
    if (!DownloadManager::init()) return Value("");
    std::string api = "https://translate-service.scratch.mit.edu/translate?language=" + langString + "&text=" + urlEncode(textString);
    std::string temp = OS::getScratchFolderLocation() + "cache/translate_temp_" + std::to_string(std::hash<std::string>{}(api)) + ".json";
    try {
        if (OS::fileExists(temp) && !DownloadManager::isDownloading(api)) {
            std::ifstream file(temp);
            if (file.is_open()) {
                nlohmann::json output;
                file >> output;
                file.close();
                Value result = Value(output["result"].get<std::string>());
                lastText = textValue;
                lastLang = langValue;
                lastResult = result;
                return result;
            } else {
                Log::logWarning("Failed to load translation result from " + temp);
                return Value("");
            }
        }

        if (DownloadManager::isDownloading(api)) {
            return;
        } else {
            Log::log("Translate: starting download for: " +textString + " -> " + temp);
            DownloadManager::addDownload(api, temp);
            BlockExecutor::addToRepeatQueue(sprite, &block);
        }
    } catch (const std::exception &e) {
        Log::logWarning(std::string("Filesystem::exists threw: ") + e.what());
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return Value("");
    }
#else
  return Value("");
#endif
}

SCRATCH_REPORTER_BLOCK(translate, getViewerLanguage) {
    return Value("English"); // Once SE! is translated into other languages, this should be replaced with the user's language
}
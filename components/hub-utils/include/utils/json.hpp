#ifndef HUB_UTILS_JSON_HPP
#define HUB_UTILS_JSON_HPP

#include <cassert>
#include <string>
#include <string_view>
#include <fstream>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"

namespace hub::utils::json
{
    inline std::string dump(rapidjson::Document&& document) noexcept
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    inline std::string dump(const rapidjson::Document& document) noexcept
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    inline rapidjson::Document parse(std::string_view str) noexcept
    {
        rapidjson::Document result;
        result.Parse(str.data(), str.length());
        return result;
    }

    inline rapidjson::Document parse_file(std::string_view path) noexcept
    {
        rapidjson::Document file;
        std::ifstream ifs(path.data());

        if (!ifs)
        {
            return file;
        }

        {
            rapidjson::IStreamWrapper json_stream(ifs);
            file.ParseStream(json_stream);
        }

        return file;
    }

    inline bool has_parse_error(const rapidjson::Document& document) noexcept
    {
        return document.HasParseError();
    }

    inline bool is_object(const rapidjson::Document& document) noexcept
    {
        return document.IsObject();
    }

    inline bool has_member(const rapidjson::Document& document, std::string_view key) noexcept
    {
        assert(is_object(document));
        return document.HasMember(key.data());
    }

    inline auto has_member_predicate(std::string_view key) noexcept
    {
        return [=](const rapidjson::Document& document) { return has_member(document, key); };
    }

    inline bool is_array(const rapidjson::Document& document) noexcept
    {
        return document.IsArray();
    }
}

#endif
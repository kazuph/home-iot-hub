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

namespace rjs = rapidjson;

namespace hub::utils::json
{
    inline std::string dump(rjs::Document&& document) noexcept
    {
        rjs::StringBuffer buffer;
        rjs::Writer<rjs::StringBuffer> writer(buffer);
        document.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    inline std::string dump(const rjs::Document& document) noexcept
    {
        rjs::StringBuffer buffer;
        rjs::Writer<rjs::StringBuffer> writer(buffer);
        document.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }

    inline rjs::Document parse(std::string_view str) noexcept
    {
        rjs::Document result;
        result.Parse(str.data(), str.length());
        return result;
    }

    inline rjs::Document parse_file(std::string_view path) noexcept
    {
        rjs::Document file;
        std::ifstream ifs(path.data());

        if (!ifs)
        {
            return file;
        }

        {
            rjs::IStreamWrapper json_stream(ifs);
            file.ParseStream(json_stream);
        }

        return file;
    }

    inline bool has_parse_error(const rjs::Document& document) noexcept
    {
        return document.HasParseError();
    }

    inline bool is_object(const rjs::Document& document) noexcept
    {
        return document.IsObject();
    }

    inline bool has_member(const rjs::Document& document, std::string_view key) noexcept
    {
        assert(is_object(document));
        return document.HasMember(key.data());
    }

    inline auto has_member_predicate(std::string_view key) noexcept
    {
        return [=](const rjs::Document& document) { return has_member(document, key); };
    }

    inline bool is_array(const rjs::Document& document) noexcept
    {
        return document.IsArray();
    }
}

#endif
#ifndef HUB_UTILS_JSON_HPP
#define HUB_UTILS_JSON_HPP

#include <string>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"

namespace hub::utils::json
{
    using namespace rapidjson;

    inline std::string dump(Document&& document) noexcept
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return std::string(buffer.GetString(), buffer.GetSize());
    }
}

#endif
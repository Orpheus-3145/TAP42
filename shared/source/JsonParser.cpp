// A plain recursive-descent parser. Nothing here handles \uXXXX escapes -
// wasn't worth it for a config file we control ourselves.
#include "JsonParser.hpp"

#include <cctype>
#include <cstdlib>
#include <cassert>
#include <format>

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s(text) {}

    bool parse(JsonValue& out, std::string& error) {
        skip_ws();
        if (!parse_value(out, error)) return false;
        skip_ws();
        if (pos != s.size()) {
            error = "trailing data after JSON value";
            return false;
        }
        return true;
    }

private:
    const std::string& s;
    size_t pos = 0;

    void skip_ws() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    bool parse_value(JsonValue& out, std::string& error) {
        skip_ws();
        if (pos >= s.size()) {
            error = "unexpected end of input";
            return false;
        }
        char c = s[pos];
        if (c == '{') return parse_object(out, error);
        if (c == '[') return parse_array(out, error);
        if (c == '"') return parse_string_value(out, error);
        if (c == 't' || c == 'f') return parse_bool(out, error);
        if (c == 'n') return parse_null(out, error);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(out, error);
        error = std::string("unexpected character '") + c + "' at offset " + std::to_string(pos);
        return false;
    }

    bool parse_object(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::Object;
        ++pos; // consume '{'
        skip_ws();
        if (pos < s.size() && s[pos] == '}') {
            ++pos;
            return true;
        }
        while (true) {
            skip_ws();
            if (pos >= s.size() || s[pos] != '"') {
                error = "expected string key at offset " + std::to_string(pos);
                return false;
            }
            JsonValue key_val;
            if (!parse_string_value(key_val, error)) return false;
            skip_ws();
            if (pos >= s.size() || s[pos] != ':') {
                error = "expected ':' at offset " + std::to_string(pos);
                return false;
            }
            ++pos;
            JsonValue value;
            if (!parse_value(value, error)) return false;
            out.object_value[key_val.string_value] = value;
            skip_ws();
            if (pos < s.size() && s[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < s.size() && s[pos] == '}') {
                ++pos;
                break;
            }
            error = "expected ',' or '}' at offset " + std::to_string(pos);
            return false;
        }
        return true;
    }

    bool parse_array(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::Array;
        ++pos; // consume '['
        skip_ws();
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            return true;
        }
        while (true) {
            JsonValue value;
            if (!parse_value(value, error)) return false;
            out.array_value.push_back(value);
            skip_ws();
            if (pos < s.size() && s[pos] == ',') {
                ++pos;
                continue;
            }
            if (pos < s.size() && s[pos] == ']') {
                ++pos;
                break;
            }
            error = "expected ',' or ']' at offset " + std::to_string(pos);
            return false;
        }
        return true;
    }

    bool parse_string_value(JsonValue& out, std::string& error) {
        out.type = JsonValue::Type::String;
        ++pos; // consume '"'
        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            char c = s[pos];
            if (c == '\\') {
                ++pos;
                if (pos >= s.size()) {
                    error = "unterminated escape sequence";
                    return false;
                }
                switch (s[pos]) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'n': result.push_back('\n'); break;
                    case 't': result.push_back('\t'); break;
                    case 'r': result.push_back('\r'); break;
                    default:
                        error = "unsupported escape sequence '\\" + std::string(1, s[pos]) + "'";
                        return false;
                }
                ++pos;
            } else {
                result.push_back(c);
                ++pos;
            }
        }
        if (pos >= s.size()) {
            error = "unterminated string";
            return false;
        }
        ++pos; // consume closing '"'
        out.string_value = result;
        return true;
    }

    bool parse_bool(JsonValue& out, std::string& error) {
        if (s.compare(pos, 4, "true") == 0) {
            out.type = JsonValue::Type::Bool;
            out.bool_value = true;
            pos += 4;
            return true;
        }
        if (s.compare(pos, 5, "false") == 0) {
            out.type = JsonValue::Type::Bool;
            out.bool_value = false;
            pos += 5;
            return true;
        }
        error = "invalid literal at offset " + std::to_string(pos);
        return false;
    }

    bool parse_null(JsonValue& out, std::string& error) {
        if (s.compare(pos, 4, "null") == 0) {
            out.type = JsonValue::Type::Null;
            pos += 4;
            return true;
        }
        error = "invalid literal at offset " + std::to_string(pos);
        return false;
    }

    bool parse_number(JsonValue& out, std::string& error) {
        size_t start = pos;
        if (pos < s.size() && s[pos] == '-') ++pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos < s.size() && s[pos] == '.') {
            ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        std::string num_str = s.substr(start, pos - start);
        if (num_str.empty() || num_str == "-") {
            error = "invalid number at offset " + std::to_string(start);
            return false;
        }
        out.type = JsonValue::Type::Number;
        out.number_value = std::strtod(num_str.c_str(), nullptr);
        return true;
    }
};

} // namespace

const JsonValue& JsonValue::operator[](const std::string& key) const {
    static const JsonValue null_value{};
    if (type != Type::Object) return null_value;
    auto it = object_value.find(key);
    if (it == object_value.end()) return null_value;
    return it->second;
}

const std::string& JsonValue::as_string() const {
    static const std::string empty;
    return type == Type::String ? string_value : empty;
}

int JsonValue::as_int() const { return type == Type::Number ? static_cast<int>(number_value) : 0; }

bool parse_json(const std::string& text, JsonValue& out, std::string& error) {
    Parser parser(text);
    return parser.parse(out, error);
}

static std::string scalarToString(JsonValue const& json);
static std::string stringifyJsonObject(JsonValue const& json, std::string const& indent = "");
static std::string stringifyJsonArray(JsonValue const& json, std::string const& indent = "");

std::string JsonValue::toString() const
{
    std::string out;
    switch (type) {
        case Type::Object: out = stringifyJsonObject(*this); break;
        case Type::Array:  out = stringifyJsonArray(*this);  break;
        case Type::Null:   return "";
        default:           return scalarToString(*this);
    }
    if (!out.empty() && out.back() == '\n')
        out.pop_back(); // no final new line
    return out;
}

std::string scalarToString(JsonValue const& json) {
    switch (json.type) {
        case JsonValue::Type::Bool:   return json.bool_value ? "true" : "false";
        case JsonValue::Type::Number: return std::format("{}", json.number_value);
        case JsonValue::Type::String: return json.string_value;
        default:                      return "null";
    }
}

std::string stringifyJsonObject(JsonValue const& json, std::string const& indent)
{
    assert(json.is_object() and "json is not an object");

    std::string out;
    for (auto const& [key, value] : json.object_value) {
        switch (value.type) {
            case JsonValue::Type::Object: out += std::format("{}{}:\n{}", indent, key, stringifyJsonObject(value, indent + "\t")); break;
            case JsonValue::Type::Array:  out += std::format("{}{}:\n{}", indent, key, stringifyJsonArray(value, indent + "\t")); break;
            default:                      out += std::format("{}{}: {}\n", indent, key, scalarToString(value)); break;
        }
    }
    return out;
}

std::string stringifyJsonArray(JsonValue const& json, std::string const& indent)
{
    assert(json.is_array() and "json is not an array");

    std::string out;
    for (JsonValue const& entry : json.array_value) {
        switch (entry.type) {
            case JsonValue::Type::Object: out += std::format("{}-\n{}", indent, stringifyJsonObject(entry, indent + "\t")); break;
            case JsonValue::Type::Array:  out += std::format("{}-\n{}", indent, stringifyJsonArray(entry, indent + "\t")); break;
            default:                      out += std::format("{}- {}\n", indent, scalarToString(entry)); break;
        }
    }
    return out;
}

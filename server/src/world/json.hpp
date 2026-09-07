// A JSON parser we wrote ourselves instead of pulling in a library - just
// enough to load the world file, nothing that aims to be spec-complete.
#pragma once

#include <map>
#include <string>
#include <vector>

// Minimal, hand-rolled JSON parser for the world data — not a complete
// RFC 8259 parser (no \uXXXX unicode escape support), but covers what's
// needed for nested objects/arrays/strings/numbers/bools/null. Avoids an
// external dependency (e.g. nlohmann/json) for a project of this scale.
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool bool_value = false;
    double number_value = 0;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::map<std::string, JsonValue> object_value;

    bool is_object() const { return type == Type::Object; }
    bool is_array() const { return type == Type::Array; }
    bool is_string() const { return type == Type::String; }

    // Safe access: returns a Null JsonValue if the key/index doesn't exist.
    const JsonValue& operator[](const std::string& key) const;

    const std::string& as_string() const; // "" if not a string
    int as_int() const;                   // 0 if not a number
};

// Returns false on a parse error, with `error` set.
bool parse_json(const std::string& text, JsonValue& out, std::string& error);

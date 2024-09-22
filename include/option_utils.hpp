// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <charconv>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace option_utils {
enum class supported_types { NONE, BOOLEAN, INTEGER, REAL, STRING };
inline std::string type_to_string(const supported_types t) {
    switch (t) {
        case supported_types::NONE:
            return "NONE";
        case supported_types::BOOLEAN:
            return "BOOLEAN";
        case supported_types::INTEGER:
            return "INTEGER";
        case supported_types::REAL:
            return "REAL";
        case supported_types::STRING:
            return "STRING";
    }
    return "ERROR";
}
struct option_as_string {
  private:
    bool throw_on_error = false;
    std::string name;
    std::string value;
    supported_types type;

  public:
    option_as_string(const std::string& nam, const std::string& val, const supported_types tp, const bool throw_except) {
        name           = nam;
        value          = val;
        type           = tp;
        throw_on_error = throw_except;
    }
    ~option_as_string() = default;
    template <typename T>
    std::string incompatible_type_error_message(const std::string& name, const supported_types type) const {
        return "incompatible types for option \"" + name + "\": cannot cast " + type_to_string(type) +
               " to (possibly mangled) type " + typeid(T).name();
    }
    void cast_error(const std::string& error_message) const {
        if (throw_on_error) {
            throw std::runtime_error(error_message);
        } else {
            std::cerr << error_message << std::endl;
            exit(1);
        }
    }
    template <typename T> T as() const {
        if constexpr (std::is_same<T, bool>::value) {
            if (type == supported_types::BOOLEAN) {
                if (std::toupper(value.c_str()[0]) == 'T') {
                    return true;
                }
                return false;
            }
        } else if constexpr (std::is_integral<T>()) {
            if (type == supported_types::INTEGER) {
                T result{};
                std::from_chars(value.data(), value.data() + value.size(), result);
                return result;
            }
        }
        if constexpr (std::is_floating_point<T>()) {
            if (type == supported_types::REAL) {
                T result{};
                std::from_chars(value.data(), value.data() + value.size(), result);
                return result;
            }
        }
        if constexpr (std::is_same<T, std::string>::value) {
            if (type == supported_types::STRING) {
                return (T)value;
            }
        }
        // last resort is to try stringstream
        T result{};
        std::stringstream(value) >> result;
        cast_error(incompatible_type_error_message<T>(name, type));
        return result;
    }
};
struct parsed_options {
  private:
    bool throw_on_error = false;
    std::map<std::string, std::string> values;
    std::map<std::string, supported_types> types;

  public:
    parsed_options(const std::map<std::string, std::string>& vals,
                   const std::map<std::string, supported_types>& tps,
                   const bool throw_except) {
        values         = vals;
        types          = tps;
        throw_on_error = throw_except;
    };
    ~parsed_options() = default;
    option_as_string operator[](const std::string& name) noexcept {
        if (values.count(name) > 0) {
            return {name, values[name], types[name], throw_on_error};
        }
        // asked for a key which does not exist
        lookup_error("option requested but not set: " + name);
        return {name, "", supported_types::NONE, throw_on_error};
    }
    size_t count(const std::string& key) const noexcept { return values.count(key); };
    void lookup_error(const std::string& error_message) const {
        if (throw_on_error) {
            throw std::runtime_error(error_message);
        } else {
            std::cerr << error_message << std::endl;
            exit(1);
        }
    }
    bool help_requested() {
        if (values.count("help") > 0) {
            if (types["help"] == supported_types::BOOLEAN) {
                return std::toupper(values["help"].c_str()[0]) == 'T';
            }
        }
        return false;
    }
};
class program_options {
  private:
    std::string program_name;
    std::string program_function;
    bool throw_on_error = false;
    std::map<std::string, std::string> values;
    std::map<std::string, supported_types> types;
    std::map<std::string, bool> is_required;
    std::map<std::string, std::string> help_msg;

  public:
    explicit program_options(const std::string& prog_name     = "",
                             const std::string& prog_function = "",
                             const bool throw_except          = false) {
        program_name     = prog_name;
        program_function = prog_function;
        throw_on_error   = throw_except;
    };
    ~program_options() = default;
    parsed_options parse(const int argc, char* argv[]) {
        int i = 1;
        while (i < argc) {
            std::string opt     = argv[i];
            std::string nextopt = "";
            if (i + 1 < argc) {
                nextopt = argv[i + 1];
            }
            if (opt.starts_with("--")) {
                // remove the dashes and see if it's recognized
                std::string opt_name = opt.substr(2);
                if (values.count(opt_name)) {
                    if (types[opt_name] == supported_types::BOOLEAN) {
                        // this is a flag, set the value to true
                        values[opt_name] = "T";
                        i++;
                    } else {
                        if (nextopt.empty() or nextopt.starts_with("--")) {
                            // this option needs a value, but there isn't one
                            parse_error("option requires a value: " + opt);
                            return {values, types, throw_on_error};
                        } else {
                            // this option looks like it has a value
                            values[opt_name] = nextopt;
                            i += 2;
                        }
                    }
                } else {
                    if (opt_name.starts_with("no")) {
                        // see if this is the --nosomething form of the --something flag
                        std::string test_flag = opt_name.substr(2);
                        if (values.count(test_flag) and types[test_flag] == supported_types::BOOLEAN) {
                            values[test_flag] = "F";
                            i++;
                        }
                    } else {
                        parse_error("unrecognized option: --" + opt);
                        return {values, types, throw_on_error};
                    }
                }
            } else {
                parse_error("unrecognized option: --" + opt);
                return {values, types, throw_on_error};
            }
        }
        for (auto& [key, is_req] : is_required) {
            if (is_req and values[key].empty()) {
                parse_error("required option has not been set: --" + key);
                break;
            }
        }
        return {values, types, throw_on_error};
    };
    void parse_error(const std::string& error_message) const {
        if (throw_on_error) {
            throw std::runtime_error(error_message);
        } else {
            std::cerr << error_message << std::endl;
            exit(1);
        }
    }
    void add_flag(const std::string& long_name, const std::string& help_text) {
        add_option(long_name, help_text, supported_types::BOOLEAN, false, "F");
    }
    void add_flag(const std::string& long_name, const std::string& help_text, const bool required) {
        add_option(long_name, help_text, supported_types::BOOLEAN, required);
    }
    void add_flag(const std::string& long_name, const std::string& help_text, const bool required, const bool default_value) {
        if (default_value) {
            add_option(long_name, help_text, supported_types::BOOLEAN, required, "T");
        } else {
            add_option(long_name, help_text, supported_types::BOOLEAN, required, "F");
        }
    }
    void add_option(const std::string& long_name, const std::string& help_text, supported_types type) {
        types[long_name]    = type;
        values[long_name]   = "";
        help_msg[long_name] = help_text;
        is_required[long_name] = false;
    }
    void add_option(const std::string& long_name,
                    const std::string& help_text,
                    supported_types type,
                    const bool required) {
        types[long_name]    = type;
        values[long_name]   = "";
        help_msg[long_name] = help_text;
        is_required[long_name] = required;
    }
    void add_option(const std::string& long_name,
                    const std::string& help_text,
                    supported_types type,
                    const bool required,
                    const std::string& default_value) {
        types[long_name]    = type;
        values[long_name]   = default_value;
        help_msg[long_name] = help_text;
        // if an empty default is given, then it's still required on the command line, otherwise not
        is_required[long_name] = default_value.empty();
    }
    std::string help() {
        std::stringstream outstr;
        if (not program_name.empty()) {
            outstr << program_name;
            if (not program_function.empty()) {
                outstr << ": " << program_function;
            }
            outstr << std::endl;
        }
        outstr << "Command line options:" << std::endl;
        for (auto& [key, value] : values) {
            if (types[key] == supported_types::BOOLEAN) {
                outstr << "     --" << key << " (flag, opposite is --no" << key << "): " << help_msg[key];
            } else {
                outstr << "     --" << key << " <" << type_to_string(types[key]) << ">: " << help_msg[key];
            }
            if (is_required[key]) {
                outstr << " [REQUIRED]";
            }
            outstr << std::endl;
        }
        return outstr.str();
    };
};
}  // namespace option_utils

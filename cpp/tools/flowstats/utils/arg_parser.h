#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <sys/stat.h>

namespace flowstats {

/**
 * Simple argument parser for flowstats
 * Supports string, unsigned integer, and boolean (flag) options
 */
class ArgParser {
public:
    explicit ArgParser(const std::string& description)
        : m_description(description)
        , m_show_help(false)
        , m_has_error(false)
    {}

    // Add string option
    void add_option(const std::string& short_name,
                   const std::string& long_name,
                   std::string& target,
                   const std::string& description,
                   bool required = false,
                   const std::string& default_value = "") {
        Option opt;
        opt.short_name = short_name;
        opt.long_name = long_name;
        opt.description = description;
        opt.required = required;
        opt.is_flag = false;
        opt.string_target = &target;
        opt.default_string = default_value;

        m_options.push_back(opt);

        if (!default_value.empty()) {
            target = default_value;
        }
    }

    // Add unsigned integer option (works with size_t, uint32_t, uint64_t)
    template<typename T>
    typename std::enable_if<std::is_unsigned<T>::value && std::is_integral<T>::value, void>::type
    add_option(const std::string& short_name,
              const std::string& long_name,
              T& target,
              const std::string& description,
              T default_value = 0) {
        Option opt;
        opt.short_name = short_name;
        opt.long_name = long_name;
        opt.description = description;
        opt.required = false;
        opt.is_flag = false;
        opt.is_uint = true;
        opt.uint64_target = reinterpret_cast<uint64_t*>(&target);
        opt.default_uint64 = static_cast<uint64_t>(default_value);

        m_options.push_back(opt);

        target = default_value;
    }

    // Add boolean flag
    void add_flag(const std::string& long_name,
                 bool& target,
                 const std::string& description) {
        Option opt;
        opt.short_name = "";
        opt.long_name = long_name;
        opt.description = description;
        opt.required = false;
        opt.is_flag = true;
        opt.bool_target = &target;

        m_options.push_back(opt);
        target = false;
    }

    // Parse arguments
    bool parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                m_show_help = true;
                print_help();
                return false;
            }

            // Find matching option
            Option* opt = find_option(arg);
            if (!opt) {
                m_error = "Unknown option: " + arg;
                m_has_error = true;
                return false;
            }

            if (opt->is_flag) {
                *opt->bool_target = true;
            } else {
                // Next argument is the value
                if (i + 1 >= argc) {
                    m_error = "Missing value for option: " + arg;
                    m_has_error = true;
                    return false;
                }

                std::string value = argv[++i];

                if (opt->is_uint) {
                    try {
                        *opt->uint64_target = std::stoull(value);
                    } catch (...) {
                        m_error = "Invalid value for " + arg + ": " + value;
                        m_has_error = true;
                        return false;
                    }
                } else {
                    *opt->string_target = value;
                }
            }

            opt->was_set = true;
        }

        // Check required options
        for (const auto& opt : m_options) {
            if (opt.required && !opt.was_set) {
                m_error = "Required option --" + opt.long_name + " not provided";
                m_has_error = true;
                return false;
            }
        }

        return true;
    }

    // Print help message
    void print_help() const {
        std::cout << m_description << "\n\n";
        std::cout << "Options:\n";

        for (const auto& opt : m_options) {
            std::cout << "  ";

            if (!opt.short_name.empty()) {
                std::cout << "-" << opt.short_name << ", ";
            }

            std::cout << "--" << opt.long_name;

            if (!opt.is_flag) {
                std::cout << " <value>";
            }

            std::cout << "\n      " << opt.description;

            if (opt.required) {
                std::cout << " [REQUIRED]";
            } else if (!opt.is_flag) {
                if (opt.is_uint) {
                    std::cout << " (default: " << opt.default_uint64 << ")";
                } else if (!opt.default_string.empty()) {
                    std::cout << " (default: " << opt.default_string << ")";
                }
            }

            std::cout << "\n\n";
        }
    }

    bool should_show_help() const { return m_show_help; }
    bool has_error() const { return m_has_error; }
    const std::string& error() const { return m_error; }

private:
    struct Option {
        std::string short_name;
        std::string long_name;
        std::string description;
        bool required;
        bool is_flag;
        bool is_uint = false;
        bool was_set = false;

        // Targets for different types
        std::string* string_target = nullptr;
        uint64_t* uint64_target = nullptr;
        bool* bool_target = nullptr;

        // Default values
        std::string default_string;
        uint64_t default_uint64 = 0;
    };

    Option* find_option(const std::string& arg) {
        for (auto& opt : m_options) {
            if (arg == "-" + opt.short_name || arg == "--" + opt.long_name) {
                return &opt;
            }
        }
        return nullptr;
    }

    std::string m_description;
    std::vector<Option> m_options;
    bool m_show_help;
    bool m_has_error;
    std::string m_error;
};

// Helper function to check if file exists
inline bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

} // namespace flowstats

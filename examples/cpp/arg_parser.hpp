#ifndef EXAMPLES_ARG_PARSER_HPP
#define EXAMPLES_ARG_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace examples {

/**
 * Simple command-line argument parser
 */
class ArgParser {
public:
    ArgParser(const std::string& description)
        : description_(description), show_help_(false) {}

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
        opt.type = OptionType::STRING;
        opt.string_target = &target;
        opt.default_string = default_value;
        options_.push_back(opt);

        if (!default_value.empty()) {
            target = default_value;
        }
    }

    // Add unsigned integer option (works for uint64_t and size_t)
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
        opt.type = OptionType::UINT64;
        opt.uint64_target = reinterpret_cast<uint64_t*>(&target);
        opt.default_uint64 = static_cast<uint64_t>(default_value);
        options_.push_back(opt);

        target = default_value;
    }

    // Add double option
    void add_option(const std::string& short_name,
                   const std::string& long_name,
                   double& target,
                   const std::string& description,
                   double default_value = 0.0) {
        Option opt;
        opt.short_name = short_name;
        opt.long_name = long_name;
        opt.description = description;
        opt.required = false;
        opt.is_flag = false;
        opt.type = OptionType::DOUBLE;
        opt.double_target = &target;
        opt.default_double = default_value;
        options_.push_back(opt);

        target = default_value;
    }

    // Add flag (boolean) option
    void add_flag(const std::string& long_name,
                 bool& target,
                 const std::string& description) {
        Option opt;
        opt.short_name = "";
        opt.long_name = long_name;
        opt.description = description;
        opt.required = false;
        opt.is_flag = true;
        opt.type = OptionType::BOOL;
        opt.bool_target = &target;
        options_.push_back(opt);

        target = false;
    }

    // Parse arguments
    bool parse(int argc, char** argv) {
        program_name_ = argv[0];

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            // Check for help
            if (arg == "-h" || arg == "--help") {
                show_help_ = true;
                return false;
            }

            // Find matching option
            Option* opt = find_option(arg);
            if (!opt) {
                error_ = "Unknown option: " + arg;
                return false;
            }

            // Handle flag
            if (opt->is_flag) {
                *opt->bool_target = true;
                continue;
            }

            // Get value
            if (i + 1 >= argc) {
                error_ = "Option " + arg + " requires a value";
                return false;
            }

            std::string value = argv[++i];

            // Set value based on type
            try {
                switch (opt->type) {
                    case OptionType::STRING:
                        *opt->string_target = value;
                        break;
                    case OptionType::UINT64:
                        *opt->uint64_target = std::stoull(value);
                        break;
                    case OptionType::DOUBLE:
                        *opt->double_target = std::stod(value);
                        break;
                    default:
                        break;
                }
                opt->was_set = true;
            } catch (const std::exception& e) {
                error_ = "Invalid value '" + value + "' for option " + arg;
                return false;
            }
        }

        // Check required options
        for (const auto& opt : options_) {
            if (opt.required && !opt.was_set) {
                error_ = "Required option --" + opt.long_name + " not provided";
                return false;
            }
        }

        return true;
    }

    // Print help
    void print_help() const {
        std::cout << description_ << "\n";
        std::cout << "Usage: " << program_name_ << " [OPTIONS]\n\n";
        std::cout << "Options:\n";

        for (const auto& opt : options_) {
            std::cout << "  ";

            // Short name
            if (!opt.short_name.empty()) {
                std::cout << opt.short_name;
                if (!opt.long_name.empty()) {
                    std::cout << ", ";
                }
            }

            // Long name
            if (!opt.long_name.empty()) {
                std::cout << "--" << opt.long_name;
            }

            // Value placeholder
            if (!opt.is_flag) {
                std::cout << " <value>";
            }

            std::cout << "\n";
            std::cout << "      " << opt.description;

            // Default value
            if (!opt.required && !opt.is_flag) {
                switch (opt.type) {
                    case OptionType::STRING:
                        if (!opt.default_string.empty()) {
                            std::cout << " (default: " << opt.default_string << ")";
                        }
                        break;
                    case OptionType::UINT64:
                        std::cout << " (default: " << opt.default_uint64 << ")";
                        break;
                    case OptionType::DOUBLE:
                        std::cout << " (default: " << opt.default_double << ")";
                        break;
                    default:
                        break;
                }
            }

            if (opt.required) {
                std::cout << " [REQUIRED]";
            }

            std::cout << "\n\n";
        }
    }

    // Get error message
    std::string error() const { return error_; }

    // Should show help?
    bool should_show_help() const { return show_help_; }

private:
    enum class OptionType {
        STRING,
        UINT64,
        DOUBLE,
        BOOL
    };

    struct Option {
        std::string short_name;
        std::string long_name;
        std::string description;
        bool required = false;
        bool is_flag = false;
        bool was_set = false;
        OptionType type;

        // Targets for different types
        std::string* string_target = nullptr;
        uint64_t* uint64_target = nullptr;
        double* double_target = nullptr;
        bool* bool_target = nullptr;

        // Default values
        std::string default_string;
        uint64_t default_uint64 = 0;
        double default_double = 0.0;
    };

    Option* find_option(const std::string& name) {
        for (auto& opt : options_) {
            if (name == opt.short_name || name == "--" + opt.long_name) {
                return &opt;
            }
        }
        return nullptr;
    }

    std::string description_;
    std::string program_name_;
    std::string error_;
    bool show_help_;
    std::vector<Option> options_;
};

} // namespace examples

#endif // EXAMPLES_ARG_PARSER_HPP

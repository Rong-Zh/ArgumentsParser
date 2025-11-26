#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <fstream>
#include <memory>
#include <typeinfo>
#include <algorithm>
#include <functional>
#include <iomanip>

namespace utils
{
    bool startswith(const std::string &str, const std::string &prefix)
    {
        if (prefix.length() > str.length())
        {
            return false;
        };
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    bool endswith(const std::string &str, const std::string &suffix)
    {
        if (suffix.length() > str.length())
        {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    bool is_longopt(const std::string &arg)
    {
        return startswith(arg, "--");
    }

    bool is_shortopt(const std::string &arg)
    {
        return startswith(arg, "-") && !startswith(arg, "--");
    }
}

enum class BoundType
{
    Closed,
    Open,
    LeftOpen,
    RightOpen
};

template <typename T>
std::string into_string(const T &value)
{
    return std::to_string(value);
}

template <>
std::string into_string(const std::string &value)
{
    return value;
}

template <>
std::string into_string<bool>(const bool &value)
{
    return value ? "true" : "false";
}

class File
{
private:
    std::string path;
public: 
    File() : path("") {}
    File(const std::string &p) : path(p) {}
    bool exists() const
    {
        std::ifstream f(path.c_str());
        return f.good();
    }
    operator std::string() const { return path; }
};

template <>
std::string into_string<File>(const File &value)
{
    return std::string(value);
}

template <typename T>
class Range
{
    T min_value, max_value;
    BoundType type;

public:
    Range() : min_value(), max_value(), type(BoundType::Closed) {}
    Range(T min, T max, BoundType t = BoundType::Closed)
        : min_value(min), max_value(max), type(t) {}

    bool check(const T &v) const
    {
        switch (type)
        {
        case BoundType::Closed:
            return v >= min_value && v <= max_value;
        case BoundType::Open:
            return v > min_value && v < max_value;
        case BoundType::LeftOpen:
            return v > min_value && v <= max_value;
        case BoundType::RightOpen:
            return v >= min_value && v < max_value;
        }
        return false;
    }

    std::string desc() const
    {
        std::string left = (type == BoundType::Closed || type == BoundType::RightOpen) ? "[" : "(";
        std::string right = (type == BoundType::Closed || type == BoundType::LeftOpen) ? "]" : ")";
        return " Range: " + left + std::to_string(min_value) + ", " + std::to_string(max_value) + right;
    }
};

template <typename T>
struct Choices
{
    std::vector<T> values;
    bool check(const T &v) const
    {
        return std::find(values.begin(), values.end(), v) != values.end();
    }

    std::string desc() const
    {
        std::string s = " Choices: {";
        for (size_t i = 0; i < values.size(); ++i)
        {
            s += into_string(values[i]);
            if (i + 1 < values.size())
                s += ", ";
        }
        s += "}";
        return s;
    }
};

enum class ArgType
{
    Required,
    Optional,
    Positional,
    Flag,
};

class ArgumentBase
{
public:
    virtual ~ArgumentBase() {}
    char short_opt;
    std::string long_opt;
    std::string help;
    ArgType arg_type;
    bool required = false;
    bool is_set = false;
    bool deprecated = false;

    virtual std::string to_string() const = 0;
    virtual void from_string(const std::string &value) = 0;
    virtual const std::type_info &get_type() const = 0;
    virtual void *get_value() = 0;
};

template <typename T>
T parse_value(const std::string &str)
{
    T value;
    std::istringstream iss(str);
    iss >> value;
    if (iss.fail())
    {
        throw std::runtime_error("Failed to parse string " + str + " as type " + typeid(T).name());
    }
    return value;
}

template <>
bool parse_value<bool>(const std::string &str)
{
    if (str == "1" || str == "true" || str == "True" || str == "TRUE")
        return true;
    if (str == "0" || str == "false" || str == "False" || str == "FALSE")
        return false;
    throw std::runtime_error("Failed to parse string '" + str + "' as type bool");
}

template <>
File parse_value<File>(const std::string &str)
{
    return File(str);
}

// Distinguish between explicit 0 and construct T(), link c++17 optional
template <typename T>
class Expected
{
private:
    bool is_explicit;
    T value;

public:
    Expected() : is_explicit(false), value() {}
    Expected(const T &v) : is_explicit(true), value(v) {}
    Expected(const char *v) : is_explicit(true), value(v) {}
    bool has_value() const { return is_explicit; }
    T get() const { return value; }
};

template <typename T, typename U = void>
class Argument : public ArgumentBase
{
public:
    T value;
    U constraint;

    std::string to_string() const override
    {
        return into_string<T>(value);
    }

    void from_string(const std::string &str) override
    {
        value = parse_value<T>(str);
        if (!constraint.check(value))
        {
            throw std::runtime_error("Parameter value does not satisfy constraints: " + to_string());
        }
    }

    const std::type_info &get_type() const override
    {
        return typeid(T);
    }

    void *get_value() override
    {
        return &value;
    }
};

inline std::string type_name(const std::type_info &t)
{
    if (t == typeid(int))
        return "int";
    if (t == typeid(float))
        return "float";
    if (t == typeid(double))
        return "double";
    if (t == typeid(bool))
        return "bool";
    if (t == typeid(std::string))
        return "string";
    if (t == typeid(File))
        return "file";
    return t.name();
};

template <typename T>
class Argument<T, void> : public ArgumentBase
{
public:
    T value;
    bool is_explicit;

    std::string to_string() const override
    {
        return into_string<T>(value);
    }

    void from_string(const std::string &str) override
    {
        value = parse_value<T>(str);
    }

    const std::type_info &get_type() const override
    {
        return typeid(T);
    }

    void *get_value() override
    {
        return &value;
    }
};

class ArgumentParser
{
public:
    ArgumentParser(const std::string &programname, const std::string &description) : programname_(programname), description_(description) {}
    ArgumentParser(const std::string &description = "") : description_(description) {}

    template <typename T>
    void add_argument(
        const char short_opt,
        const std::string &long_opt,
        ArgType type,
        const std::string &help,
        const Expected<T> &expected = Expected<T>())
    {
        // Allow optional arguments without default values
        // Users should check with exists() before calling get()
        /*
        if (type == ArgType::Optional && !expected.has_value())
        {
            std::string msg = "Optional arguments '";
            if (short_opt != '\0')
            {
                msg += "-" + std::string(1, short_opt) + ", ";
            }
            msg += "--" + long_opt + "' must have a non-default value.";
            throw std::runtime_error(msg);
        }
        */
        auto arg = Arg::argument(expected.get());
        add_argument_impl(std::move(arg), short_opt, long_opt, type, help, expected);
    }

    template <typename T, typename U>
    void add_argument(
        const char short_opt,
        const std::string &long_opt,
        ArgType type,
        const std::string &help,
        const Expected<T> &expected = Expected<T>(),
        const U &constraint = U())
    {

        if (long_opt.empty())
        {
            throw std::runtime_error("Long option cannot be empty.");
        }
        // Allow optional arguments without default values
        // Users should check with exists() before calling get()
        /*
        if (type == ArgType::Optional && !expected.has_value())
        {
            std::string msg = "Optional arguments '";
            if (short_opt != '\0')
            {
                msg += "-" + std::string(1, short_opt) + ", ";
            }
            msg += "--" + long_opt + "' must have a non-default value.";
            throw std::runtime_error(msg);
        }
        */
        if (!constraint.check(expected.get()))
        {
            throw std::runtime_error(
                "Argument: '" + long_opt +
                "', value: " + into_string(expected.get()) +
                " does not satisfy constraint: " + constraint.desc());
        }
        auto arg = Arg::argument(expected.get(), constraint);
        add_argument_impl(std::move(arg), short_opt, long_opt, type, help + constraint.desc(), expected);
    }

    template <typename T>
    void add_positional(const std::string &long_opt, const std::string &help)
    {
        add_argument<T>('\0', long_opt, ArgType::Positional, help, Expected<T>());
    }

    template <typename T>
    void add_required(const char short_opt, const std::string &long_opt, const std::string &help)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Required, help, Expected<T>());
    }

    template <typename T>
    void add_required(const std::string &long_opt, const std::string &help)
    {
        add_argument<T>('\0', long_opt, ArgType::Required, help, Expected<T>());
    }

    template <typename T>
    void add_optional(const char short_opt, const std::string &long_opt, const std::string &help, const Expected<T> &expected)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Optional, help, expected);
    }

    template <typename T>
    void add_optional(const char short_opt, const std::string &long_opt, const std::string &help)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Optional, help, Expected<T>());
    }

    template <typename T>
    void add_optional(const std::string &long_opt, const std::string &help, const Expected<T> &expected)
    {
        add_argument<T>('\0', long_opt, ArgType::Optional, help, expected);
    }

    template <typename T>
    void add_optional(const std::string &long_opt, const std::string &help)
    {
        add_argument<T>('\0', long_opt, ArgType::Optional, help, Expected<T>());
    }

    template <typename T, typename U>
    void add_optional(const char short_opt, const std::string &long_opt, const std::string &help, const Expected<T> &expected, const U &constraint)
    {
        add_argument<T, U>(short_opt, long_opt, ArgType::Optional, help, expected, constraint);
    }

    template <typename T, typename U>
    void add_optional(const std::string &long_opt, const std::string &help, const Expected<T> &expected, const U &constraint)
    {
        add_argument<T, U>('\0', long_opt, ArgType::Optional, help, expected, constraint);
    }

    void add_flag(const char short_opt, const std::string &long_opt, const std::string &help)
    {
        add_argument<bool>(short_opt, long_opt, ArgType::Flag, help, Expected<bool>());
    }

    void add_flag(const std::string &long_opt, const std::string &help)
    {
        add_argument<bool>('\0', long_opt, ArgType::Flag, help, Expected<bool>());
    }

    template <typename T>
    T get(const std::string &name) const
    {
        ArgumentBase *arg = nullptr;
        std::string key;
        if (name.size() == 1)
        {
            // find short argument
            char short_opt = name.front();
            for (const auto &pair : arguments_)
            {
                if (pair.second->short_opt == short_opt)
                {
                    arg = pair.second.get();
                    key = "-" + name;
                    break;
                }
            }
            if (!arg)
                throw std::runtime_error("Argument not found: -" + name);
        }
        else
        {
            // find long argument
            auto it = arguments_.find(name);
            if (it == arguments_.end())
                throw std::runtime_error("Argument not found: --" + name);
            arg = it->second.get();
            key = "--" + name;
        }

        if (arg->get_type() != typeid(T))
        {
            throw std::runtime_error("Argument type mismatch: " + key +
                                     ", expected " + type_name(typeid(T)) +
                                     ", got " + type_name(arg->get_type()));
        }
        void *ptr = arg->get_value();
        if (!ptr)
            throw std::runtime_error("Argument value is null: " + key);
        return *static_cast<T *>(ptr);
    }

    template <typename T>
    T get(const char short_opt) const
    {
        return get<T>(std::string(1, short_opt));
    }

    void parse_args(int argc, char *argv[])
    {
        if (argc < 2)
        {
            print_help();
            std::exit(EXIT_SUCCESS);
        }
        std::vector<std::string> positional_values;
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
            // --arg=value --arg value
            if (utils::is_longopt(arg))
            {
                std::string opt_name = arg.substr(2);
                std::string value;
                size_t eq_pos = opt_name.find('=');
                if (eq_pos != std::string::npos)
                {
                    value = opt_name.substr(eq_pos + 1);
                    opt_name = opt_name.substr(0, eq_pos);
                }

                auto it = arguments_.find(opt_name);
                if (it == arguments_.end())
                    throw std::runtime_error("Unknown argument: --" + opt_name);

                ArgumentBase *argument = it->second.get();
                if (argument->deprecated)
                {
                    throw std::runtime_error(std::string("Argument '-") +
                                             (argument->short_opt ? std::string(1, argument->short_opt) : argument->long_opt) +
                                             "' is deprecated and cannot be used.");
                }

                if (argument->arg_type == ArgType::Flag)
                {
                    argument->from_string("true");
                }
                else
                {
                    if (value.empty())
                    {
                        if (i + 1 >= argc)
                            throw std::runtime_error("Argument --" + opt_name + " requires a value");
                        value = argv[++i];
                    }
                    argument->from_string(value);
                }
                argument->is_set = true;
            }
            // -a  -abc
            else if (utils::is_shortopt(arg))
            {
                if (arg.size() > 2)
                {
                    char first_opt = arg[1];
                    ArgumentBase *first_argument = nullptr;
                    for (const auto &pair : arguments_)
                    {
                        if (pair.second->short_opt == first_opt)
                        {
                            first_argument = pair.second.get();
                            break;
                        }
                    }
                    
                    if (!first_argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + first_opt);
                    
                    // if the first argument is not a flag,  value is after argument
                    if (first_argument->arg_type != ArgType::Flag)
                    {
                        std::string value = arg.substr(2);
                        first_argument->from_string(value);
                        first_argument->is_set = true;
                    }
                    else
                    {
                        for (size_t j = 1; j < arg.size(); ++j)
                        {
                            char short_opt = arg[j];
                            ArgumentBase *argument = nullptr;
                            for (const auto &pair : arguments_)
                            {
                                if (pair.second->short_opt == short_opt)
                                {
                                    argument = pair.second.get();
                                    break;
                                }
                            }
                            if (!argument)
                                throw std::runtime_error(std::string("Unknown short flag: -") + short_opt);
                            if (argument->arg_type != ArgType::Flag)
                                throw std::runtime_error(std::string("Only flag arguments can be merged: -") + short_opt);
                            argument->from_string("true");
                            argument->is_set = true;
                        }
                    }
                }
                else
                {
                    char short_opt = arg[1];
                    ArgumentBase *argument = nullptr;
                    for (const auto &pair : arguments_)
                    {
                        if (pair.second->short_opt == short_opt)
                        {
                            argument = pair.second.get();
                            break;
                        }
                    }
                    if (!argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + short_opt);

                    if (argument->arg_type == ArgType::Flag)
                    {
                        argument->from_string("true");
                        argument->is_set = true;
                    }
                    else
                    {
                        if (i + 1 >= argc)
                            throw std::runtime_error(std::string("Argument -") + short_opt + " requires a value");
                        argument->from_string(argv[++i]);
                        argument->is_set = true;
                    }
                }
            }
            else
            {
                positional_values.push_back(arg);
            }
        }
        // deal with positional arguments
        for (size_t i = 0; i < positionals_.size(); ++i)
        {
            if (i < positional_values.size())
            {
                const std::string &name = positionals_[i];
                arguments_[name]->from_string(positional_values[i]);
                arguments_[name]->is_set = true;
            }
        }

        // check required arguments
        for (const auto &pair : arguments_)
        {
            const auto &arg = pair.second;
            if (arg->required && !arg->is_set)
                throw std::runtime_error("Required argument missing: " + pair.first);
        }
    }

    void print_help() const
    {
        std::cout << programname_ << ": " << description_ << "\n"
                  << "Version:" << version_ << "\n"
                  << "Usage: " << programname_ << " [options]";
        for (const auto &name : positionals_)
        {
            std::cout << " <" << name << ">";
        }
        std::cout << "\n\n";
        print_help("Required arguments", true);
        print_help("Optional arguments", false);
        if (!note_.empty())
        {
            std::cout << note_ << "\n\n";
        }
    }

    bool exists(const std::string &name) const
    {
        return arguments_.find(name) != arguments_.end();
    }

    bool exists(const char short_opt) const
    {
        if (short_opt == '\0')
        {
            return false;
        }
        for (const auto &pair : arguments_)
        {
            if (pair.second->short_opt == short_opt)
            {
                return true;
            }
        }
        return false;
    }

    void set_program_name(const std::string &name)
    {
        programname_ = name;
    }

    std::string get_program_name() const
    {
        return programname_;
    }

    void set_description(const std::string &description)
    {
        description_ = description;
    }

    std::string get_description() const
    {
        return description_;
    }

    void set_version(const std::string &version)
    {
        version_ = version;
    }

    std::string get_version() const
    {
        return version_;
    }

    bool deprecate(const std::string &long_opt)
    {
        auto it = arguments_.find(long_opt);
        if (it != arguments_.end())
        {
            it->second->deprecated = true;
            return true;
        }
        return false;
    }

    bool deprecate(char short_opt)
    {
        for (auto &pair : arguments_)
        {
            if (pair.second->short_opt == short_opt)
            {
                pair.second->deprecated = true;
                return true;
            }
        }
        return false;
    }

    bool is_deprecated() const
    {
        for (const auto &pair : arguments_)
        {
            return pair.second->deprecated;
        }
        return false;
    }

    void footnote(const std::string &note)
    {
        note_ = note;
    }

private:
    void print_help(const std::string &title, bool required_flag) const
    {
        size_t opt_w = 0, type_w = 0;
        bool has_args = false;
        for (const auto &name : order_)
        {
            const ArgumentBase *arg = arguments_.at(name).get();
            if (arg->required == required_flag && arg->arg_type != ArgType::Positional)
            {
                has_args = true;
                opt_w = std::max(opt_w, arg->long_opt.length());
                type_w = std::max(type_w, type_name(arg->get_type()).length());
            }
        }
        if (!has_args)
            return;
        std::cout << title << ":\n";
        for (const auto &name : order_)
        {
            const ArgumentBase *arg = arguments_.at(name).get();
            if (arg->required == required_flag && arg->arg_type != ArgType::Positional)
            {
                std::cout << "  ";
                if (arg->short_opt != '\0')
                    std::cout << "-" << arg->short_opt << ", ";
                else
                    std::cout << "    ";

                std::cout << "--" << std::setw(opt_w) << std::left << arg->long_opt << "    ";
                std::cout << std::setw(type_w) << std::left << (arg->arg_type == ArgType::Flag ? "flag" : type_name(arg->get_type())) << "    ";
                if (arg->deprecated)
                    std::cout << "[Deprecated] ";
                std::cout << arg->help << ". ";
                if (arg->is_set)
                    std::cout << "(default: " << arg->to_string() << ")";
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }

    class Arg
    {
    public:
        template <typename T>
        static std::unique_ptr<ArgumentBase> argument(const T &expected)
        {
            std::unique_ptr<Argument<T>> arg(new Argument<T>());
            arg->value = expected;
            return arg;
        }

        template <typename T, typename U>
        static std::unique_ptr<ArgumentBase> argument(const T &expected, const U &constraint)
        {
            std::unique_ptr<Argument<T, U>> arg(new Argument<T, U>());
            arg->value = expected;
            arg->constraint = constraint;
            return arg;
        }
    };

    template <typename T>
    void add_argument_impl(
        std::unique_ptr<ArgumentBase> arg,
        const char short_opt,
        const std::string &long_opt,
        ArgType type,
        const std::string &help,
        const Expected<T> &expected)
    {
        if (exists(short_opt))
        {
            throw std::runtime_error("Short option '" + std::string(1, short_opt) + "' already exists.");
        }
        arg->short_opt = short_opt;
        if (exists(long_opt))
        {
            throw std::runtime_error("Long option '" + long_opt + "' already exists.");
        }
        arg->long_opt = long_opt;
        arg->help = help;
        arg->arg_type = type;
        arg->required = (type == ArgType::Required);
        arg->is_set = expected.has_value();
        if (type == ArgType::Positional)
        {
            positionals_.push_back(long_opt);
        }
        arguments_[long_opt] = std::move(arg);
        order_.push_back(long_opt);
    }

    std::string programname_;
    std::string description_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<ArgumentBase>> arguments_;
    std::vector<std::string> order_;
    std::vector<std::string> positionals_;
};

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <list>
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
#include <type_traits>
#include <utility>

namespace utils
{
    inline bool startswith(const std::string &str, const std::string &prefix)
    {
        if (prefix.length() > str.length())
            return false;
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    inline bool endswith(const std::string &str, const std::string &suffix)
    {
        if (suffix.length() > str.length())
            return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    inline bool is_longopt(const std::string &arg)
    {
        return startswith(arg, "--");
    }

    inline bool is_shortopt(const std::string &arg)
    {
        if (!startswith(arg, "-") || startswith(arg, "--"))
            return false;
        // "-1", "-3.14" etc. are negative numbers, not short options
        if (arg.size() >= 2 && (std::isdigit(static_cast<unsigned char>(arg[1])) || arg[1] == '.'))
            return false;
        return true;
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
inline std::string into_string(const std::string &value)
{
    return value;
}

template <>
inline std::string into_string<bool>(const bool &value)
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
inline std::string into_string<File>(const File &value)
{
    return std::string(value);
}

template <typename T>
std::string into_string(const std::vector<T> &value)
{
    if (value.empty())
        return "";
    std::string result = into_string(value[0]);
    for (size_t i = 1; i < value.size(); ++i)
    {
        result += "," + into_string(value[i]);
    }
    return result;
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
    bool has_default = false; // true if a default value was provided at registration
    bool was_parsed = false;  // true if the user supplied this argument on the command line
    bool deprecated = false;
    bool is_helparg = false;
    bool is_repeatable = false;
    bool is_variadic = false; // true if this positional absorbs all remaining positional values

    virtual std::string to_string() const = 0;
    virtual void from_string(const std::string &value) = 0;
    virtual void append_repeatable(const std::string &value) = 0;
    virtual const std::type_info &get_type() const = 0;
    virtual std::string display_type() const = 0;
    virtual void *get_value() = 0;

    std::string label_type() const
    {
        return arg_type == ArgType::Flag ? "bool" : display_type();
    }
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
inline bool parse_value<bool>(const std::string &str)
{
    if (str == "1" || str == "true" || str == "True" || str == "TRUE")
        return true;
    if (str == "0" || str == "false" || str == "False" || str == "FALSE")
        return false;
    throw std::runtime_error("Failed to parse string '" + str + "' as type bool");
}

template <>
inline File parse_value<File>(const std::string &str)
{
    return File(str);
}

template <>
inline std::vector<std::string> parse_value<std::vector<std::string>>(const std::string &str)
{
    return {str};
}

// Whitelist: sequential containers treated as appendable for repeatable arguments.
// Excludes std::string which has push_back but is not a container in this context.
template <typename T>
struct is_appendable : std::false_type
{
};

template <typename T, typename A>
struct is_appendable<std::vector<T, A>> : std::true_type
{
};

template <typename T, typename A>
struct is_appendable<std::deque<T, A>> : std::true_type
{
};

template <typename T, typename A>
struct is_appendable<std::list<T, A>> : std::true_type
{
};

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

template <typename T>
struct TypeName
{
    static std::string name() { return typeid(T).name(); }
};
template <>
struct TypeName<int>
{
    static std::string name() { return "int"; }
};
template <>
struct TypeName<float>
{
    static std::string name() { return "float"; }
};
template <>
struct TypeName<double>
{
    static std::string name() { return "double"; }
};
template <>
struct TypeName<bool>
{
    static std::string name() { return "bool"; }
};
template <>
struct TypeName<std::string>
{
    static std::string name() { return "string"; }
};
template <>
struct TypeName<File>
{
    static std::string name() { return "file"; }
};
template <typename T>
struct TypeName<std::vector<T>>
{
    static std::string name() { return TypeName<T>::name() + "[]"; }
};

// CRTP base: holds value and implements the three repeated virtual methods
template <typename Derived, typename T>
class ArgumentValueBase : public ArgumentBase
{
public:
    T value;

    std::string to_string() const override
    {
        return into_string(value);
    }

    const std::type_info &get_type() const override
    {
        return typeid(T);
    }

    std::string display_type() const override
    {
        return TypeName<T>::name();
    }

    void *get_value() override
    {
        return &value;
    }
};

template <typename T, typename U = void>
class Argument : public ArgumentValueBase<Argument<T, U>, T>
{
public:
    U constraint;

    void from_string(const std::string &str) override
    {
        this->value = parse_value<T>(str);
        if (!constraint.check(this->value))
        {
            throw std::runtime_error("Parameter value does not satisfy constraints: " + this->to_string());
        }
    }

    void append_repeatable(const std::string &str) override
    {
        append_repeatable_impl<T>(str);
    }

private:
    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string &str)
    {
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string &)
    {
        throw std::runtime_error("Argument '--" + this->long_opt + "' is marked repeatable but its type does not support push_back");
    }
};

template <typename T>
class Argument<T, void> : public ArgumentValueBase<Argument<T, void>, T>
{
public:
    void from_string(const std::string &str) override
    {
        from_string_impl<T>(str);
    }

    void append_repeatable(const std::string &str) override
    {
        append_repeatable_impl<T>(str);
    }

private:
    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type from_string_impl(const std::string &str)
    {
        this->value = parse_value<Q>(str);
    }

    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type from_string_impl(const std::string &str)
    {
        this->value.clear();
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string &str)
    {
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string &)
    {
        throw std::runtime_error("Argument '--" + this->long_opt + "' is marked repeatable but its type does not support push_back");
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
        const Expected<T> &expected = Expected<T>(),
        bool is_helparg = false,
        bool is_repeatable = false)
    {
        add_argument_impl(Arg::argument(expected.get()), short_opt, long_opt, type, help, expected, is_helparg, is_repeatable);
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
        if (expected.has_value() && !constraint.check(expected.get()))
        {
            throw std::runtime_error(
                "Argument: '" + long_opt +
                "', value: " + into_string(expected.get()) +
                " does not satisfy constraint: " + constraint.desc());
        }
        add_argument_impl(Arg::argument(expected.get(), constraint), short_opt, long_opt, type, help + constraint.desc(), expected);
    }

    template <typename T>
    void add_positional(const std::string &long_opt, const std::string &help)
    {
        add_argument<T>('\0', long_opt, ArgType::Positional, help, Expected<T>());
    }

    // Variadic positional: absorbs all remaining positional values into a vector<T>.
    // Must be registered last; only one variadic positional is allowed.
    template <typename T>
    void add_positional_variadic(const std::string &long_opt, const std::string &help)
    {
        for (const auto &name : positionals_)
        {
            if (arguments_.count(name) && arguments_.at(name)->is_variadic)
                throw std::runtime_error("Only one variadic positional is allowed; '" + name + "' is already variadic.");
        }
        add_argument<std::vector<T>>('\0', long_opt, ArgType::Positional, help, Expected<std::vector<T>>(), false, true);
        arguments_[long_opt]->is_variadic = true;
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
    void add_repeatable(const char short_opt, const std::string &long_opt, const std::string &help)
    {
        add_argument<std::vector<T>>(short_opt, long_opt, ArgType::Optional, help, Expected<std::vector<T>>(), false, true);
    }

    template <typename T>
    void add_repeatable(const std::string &long_opt, const std::string &help)
    {
        add_argument<std::vector<T>>('\0', long_opt, ArgType::Optional, help, Expected<std::vector<T>>(), false, true);
    }

    void add_help(char short_opt = 'h', const std::string &long_opt = "help", const std::string &help = "Show help message")
    {
        add_argument<bool>(short_opt, long_opt, ArgType::Flag, help, Expected<bool>(), true);
    }

    bool was_set(const std::string &name) const
    {
        auto it = arguments_.find(name);
        return it != arguments_.end() && it->second->was_parsed;
    }

    bool was_set(const char short_opt) const
    {
        auto it = short_to_long_.find(short_opt);
        if (it == short_to_long_.end())
            return false;
        return was_set(it->second);
    }

    template <typename T>
    T get(const std::string &name) const
    {
        ArgumentBase *arg = nullptr;
        std::string key;
        if (name.size() == 1)
        {
            // find via short-option index
            char short_opt = name.front();
            auto sit = short_to_long_.find(short_opt);
            if (sit == short_to_long_.end())
                throw std::runtime_error("Argument not found: -" + name);
            auto it = arguments_.find(sit->second);
            arg = it->second.get();
            key = "-" + name;
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
                                     ", expected " + TypeName<T>::name() +
                                     ", got " + arg->display_type());
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
        if (programname_.empty())
            programname_ = argv[0];
        // Check for help flag if enabled
        ArgumentBase *args = get_helparg();
        if (args)
        {
            std::string short_opt;
            if (args->short_opt != '\0')
                short_opt = std::string("-") + std::string(1, args->short_opt);
            std::string long_opt = std::string("--") + args->long_opt;

            for (int i = 1; i < argc; ++i)
            {
                std::string a = argv[i];
                if ((!short_opt.empty() && a == short_opt) || a == long_opt)
                {
                    print_help();
                    std::exit(EXIT_SUCCESS);
                }
            }
        }

        if (argc < 2)
        {
            if (has_required() || has_helparg())
            {
                print_help();
                std::exit(EXIT_SUCCESS);
            }
            // No required args and no help flag: allow running without arguments
        }
        std::vector<std::string> positional_values;
        bool end_of_opts = false;
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
            // "--" signals end of options: all remaining tokens are positional values.
            if (!end_of_opts && arg == "--")
            {
                end_of_opts = true;
                continue;
            }
            if (end_of_opts)
            {
                positional_values.push_back(arg);
                continue;
            }
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
                    if (argument->is_repeatable)
                        argument->append_repeatable(value);
                    else
                        argument->from_string(value);
                }
                argument->was_parsed = true;
            }
            // -a  -abc
            else if (utils::is_shortopt(arg))
            {
                if (arg.size() > 2)
                {
                    char first_opt = arg[1];
                    ArgumentBase *first_argument = nullptr;
                    {
                        auto sit = short_to_long_.find(first_opt);
                        if (sit != short_to_long_.end())
                            first_argument = arguments_.at(sit->second).get();
                    }

                    if (!first_argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + first_opt);

                    // if the first argument is not a flag,  value is after argument
                    if (first_argument->arg_type != ArgType::Flag)
                    {
                        std::string value = arg.substr(2);
                        if (first_argument->is_repeatable)
                            first_argument->append_repeatable(value);
                        else
                            first_argument->from_string(value);
                        first_argument->was_parsed = true;
                    }
                    else
                    {
                        for (size_t j = 1; j < arg.size(); ++j)
                        {
                            char sopt = arg[j];
                            ArgumentBase *argument = nullptr;
                            {
                                auto sit = short_to_long_.find(sopt);
                                if (sit != short_to_long_.end())
                                    argument = arguments_.at(sit->second).get();
                            }
                            if (!argument)
                                throw std::runtime_error(std::string("Unknown short flag: -") + sopt);
                            if (argument->arg_type != ArgType::Flag)
                                throw std::runtime_error(std::string("Only flag arguments can be merged: -") + sopt);
                            argument->from_string("true");
                            argument->was_parsed = true;
                        }
                    }
                }
                else
                {
                    char sopt = arg[1];
                    ArgumentBase *argument = nullptr;
                    {
                        auto sit = short_to_long_.find(sopt);
                        if (sit != short_to_long_.end())
                            argument = arguments_.at(sit->second).get();
                    }
                    if (!argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + sopt);

                    if (argument->arg_type == ArgType::Flag)
                    {
                        argument->from_string("true");
                        argument->was_parsed = true;
                    }
                    else
                    {
                        if (i + 1 >= argc)
                            throw std::runtime_error(std::string("Argument -") + sopt + " requires a value");
                        std::string value = argv[++i];
                        if (argument->is_repeatable)
                            argument->append_repeatable(value);
                        else
                            argument->from_string(value);
                        argument->was_parsed = true;
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
                ArgumentBase *arg = arguments_[name].get();
                if (arg->is_variadic)
                {
                    // Feed all remaining positional values into this argument.
                    for (size_t j = i; j < positional_values.size(); ++j)
                        arg->append_repeatable(positional_values[j]);
                    arg->was_parsed = true;
                    break;
                }
                arg->from_string(positional_values[i]);
                arg->was_parsed = true;
            }
        }

        // check required arguments
        for (const auto &pair : arguments_)
        {
            const auto &arg = pair.second;
            if (arg->required && !arg->was_parsed)
                throw std::runtime_error("Required argument missing: " + pair.first);
        }
    }

    void print_help()
    {
        std::cout << programname_ << ": " << description_ << "\n"
                  << "Version:" << version_ << "\n"
                  << "Usage: " << programname_ << " [options]";
        for (const auto &name : positionals_)
        {
            const bool variadic = arguments_.count(name) && arguments_.at(name)->is_variadic;
            std::cout << " <" << name << (variadic ? "..." : "") << ">";
        }
        std::cout << "\n\n";
        print_help_sections();
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
            return false;
        return short_to_long_.find(short_opt) != short_to_long_.end();
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
        auto it = short_to_long_.find(short_opt);
        if (it != short_to_long_.end())
        {
            arguments_[it->second]->deprecated = true;
            return true;
        }
        return false;
    }

    bool is_deprecated() const
    {
        for (const auto &pair : arguments_)
        {
            if (pair.second->deprecated)
                return true;
        }
        return false;
    }

    void footnote(const std::string &note)
    {
        note_ = note;
    }

private:
    bool has_required() const
    {
        for (const auto &pair : arguments_)
        {
            if (pair.second->required)
            {
                return true;
            }
        }
        return false;
    }

    bool has_helparg() const
    {
        return get_helparg() != nullptr;
    }

    ArgumentBase *get_helparg() const
    {
        for (const auto &pair : arguments_)
        {
            if (pair.second->is_helparg)
                return pair.second.get();
        }
        return nullptr;
    }

    // Single-pass help printer: emits Required section then Optional section
    void print_help_sections()
    {
        // Build display order once (move help arg to end)
        auto display_order = order_;
        ArgumentBase *help_arg = get_helparg();
        if (help_arg)
        {
            auto it = std::find(display_order.begin(), display_order.end(), help_arg->long_opt);
            if (it != display_order.end())
                std::rotate(it, it + 1, display_order.end());
        }

        // Compute column widths across all non-positional arguments (one pass)
        size_t opt_w = 0, type_w = 0;
        for (const auto &name : display_order)
        {
            const ArgumentBase *arg = arguments_.at(name).get();
            if (arg->arg_type == ArgType::Positional)
                continue;
            opt_w = std::max(opt_w, arg->long_opt.length());
            std::string ts = arg->label_type();
            type_w = std::max(type_w, ts.length());
        }

        // Emit each section using the pre-built display_order
        for (int pass = 0; pass < 2; ++pass)
        {
            bool show_required = (pass == 0);
            const char *section_title = show_required ? "Required arguments" : "Optional arguments";

            // Check if this section has any entries
            bool has_any = false;
            for (const auto &name : display_order)
            {
                const ArgumentBase *arg = arguments_.at(name).get();
                if (arg->arg_type != ArgType::Positional && arg->required == show_required)
                {
                    has_any = true;
                    break;
                }
            }
            if (!has_any)
                continue;

            std::cout << section_title << ":\n";
            for (const auto &name : display_order)
            {
                const ArgumentBase *arg = arguments_.at(name).get();
                if (arg->arg_type == ArgType::Positional || arg->required != show_required)
                    continue;

                std::cout << "  ";
                if (arg->short_opt != '\0')
                    std::cout << "-" << arg->short_opt << ", ";
                else
                    std::cout << "    ";

                std::cout << "--" << std::setw(opt_w) << std::left << arg->long_opt << "    ";
                std::cout << std::setw(type_w) << std::left
                          << arg->label_type()
                          << "    ";

                if (arg->deprecated)
                    std::cout << "[Deprecated] ";

                std::cout << arg->help;

                if (arg->has_default && arg->arg_type != ArgType::Flag)
                    std::cout << " (default: " << arg->to_string() << ")";

                std::cout << "\n";
            }
            std::cout << "\n";
        }
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
        const Expected<T> &expected,
        bool is_helparg = false,
        bool is_repeatable = false)
    {
        if (long_opt.empty())
            throw std::runtime_error("Long option cannot be empty.");
        if (short_opt != '\0' && exists(short_opt))
            throw std::runtime_error("Short option '" + std::string(1, short_opt) + "' already exists.");
        if (exists(long_opt))
            throw std::runtime_error("Long option '" + long_opt + "' already exists.");

        arg->short_opt = short_opt;
        arg->long_opt = long_opt;
        arg->help = help;
        arg->arg_type = type;
        arg->required = (type == ArgType::Required);
        arg->has_default = expected.has_value();
        arg->was_parsed = false;
        arg->is_helparg = is_helparg;
        arg->is_repeatable = is_repeatable;
        if (type == ArgType::Positional)
            positionals_.push_back(long_opt);
        if (short_opt != '\0')
            short_to_long_[short_opt] = long_opt;
        arguments_[long_opt] = std::move(arg);
        order_.push_back(long_opt);
    }

    std::string programname_;
    std::string description_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<ArgumentBase>> arguments_;
    std::unordered_map<char, std::string> short_to_long_; // short opt -> long opt index
    std::vector<std::string> order_;
    std::vector<std::string> positionals_;
};

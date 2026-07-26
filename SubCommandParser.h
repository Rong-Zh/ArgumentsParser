#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef ARGUMENTS_PARSER_BASE_TYPES_DEFINED
#define ARGUMENTS_PARSER_BASE_TYPES_DEFINED

enum class BoundType : uint8_t
{
    Closed,
    Open,
    LeftOpen,
    RightOpen
};

enum class ArgType : uint8_t
{
    Option,
    Positional,
    Flag,
};

class File
{
  private:
    std::string path;

  public:
    File() = default;
    File(const std::string& p) : path(p) {}
    bool exists() const
    {
        std::ifstream f(path.c_str());
        return f.good();
    }
    operator std::string() const { return path; }
};

#endif

// Internal implementation details — not part of the public API
namespace subcommand_detail
{

class StreamStateGuard
{
  public:
    explicit StreamStateGuard(std::ios& stream)
        : stream_(stream), flags_(stream.flags()), precision_(stream.precision()), fill_(stream.fill())
    {
    }

    ~StreamStateGuard()
    {
        stream_.flags(flags_);
        stream_.precision(precision_);
        stream_.fill(fill_);
    }

  private:
    StreamStateGuard(const StreamStateGuard&);
    StreamStateGuard& operator=(const StreamStateGuard&);

    std::ios& stream_;
    std::ios::fmtflags flags_;
    std::streamsize precision_;
    char fill_;
};

inline int stream_width(std::size_t width)
{
    const std::size_t maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(width, maximum));
}

inline bool is_positional_type(ArgType type)
{
    return type == ArgType::Positional;
}

inline bool startswith(const std::string& str, const std::string& prefix)
{
    if (prefix.length() > str.length())
    {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}

inline bool endswith(const std::string& str, const std::string& suffix)
{
    if (suffix.length() > str.length())
    {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

inline bool is_negative_number(const std::string& arg)
{
    return arg.size() >= 2 && (std::isdigit(static_cast<unsigned char>(arg[1])) || arg[1] == '.');
}

inline bool is_longopt(const std::string& arg)
{
    return startswith(arg, "--");
}

inline bool is_shortopt(const std::string& arg)
{
    if (arg.size() < 2 || !startswith(arg, "-") || startswith(arg, "--"))
    {
        return false;
    }
    // "-1", "-3.14" etc. are negative numbers, not short options
    if (is_negative_number(arg))
    {
        return false;
    }
    return true;
}

inline bool accepts_option_like_value(const std::string& type)
{
    return type == "string" || type == "file" || type == "string[]" || type == "file[]";
}

inline std::string constraint_expectation(std::string description)
{
    const size_t first = description.find_first_not_of(' ');
    description = first == std::string::npos ? std::string() : description.substr(first);
    const std::string range_prefix = "Range: ";
    const std::string choices_prefix = "Choices: ";
    if (description.compare(0, range_prefix.size(), range_prefix) == 0)
        return "in range " + description.substr(range_prefix.size());
    if (description.compare(0, choices_prefix.size(), choices_prefix) == 0)
        return "one of " + description.substr(choices_prefix.size());
    return "satisfying " + description;
}

template <typename T>
std::string into_string(const T& value)
{
    return std::to_string(value);
}

template <>
inline std::string into_string(const std::string& value)
{
    return value;
}

template <>
inline std::string into_string<bool>(const bool& value)
{
    return value ? "true" : "false";
}

template <>
inline std::string into_string<File>(const File& value)
{
    return std::string(value);
}

template <typename T>
std::string into_string(const std::vector<T>& value)
{
    if (value.empty())
    {
        return "";
    }
    std::string result = into_string(value[0]);
    for (size_t i = 1; i < value.size(); ++i)
    {
        result += "," + into_string(value[i]);
    }
    return result;
}

template <typename T>
T parse_value(const std::string& str)
{
    const size_t first_non_space = str.find_first_not_of(" \t\n\r\f\v");
    if (std::is_unsigned<T>::value && first_non_space != std::string::npos && str[first_non_space] == '-')
        throw std::runtime_error("Failed to parse negative string " + str + " as unsigned type " + typeid(T).name());
    T value;
    std::istringstream iss(str);
    iss >> value;
    if (iss.fail())
    {
        throw std::runtime_error("Failed to parse string " + str + " as type " + typeid(T).name());
    }
    iss >> std::ws;
    if (!iss.eof())
    {
        throw std::runtime_error("Failed to parse string " + str + " as type " + typeid(T).name());
    }
    return value;
}

template <>
inline std::string parse_value<std::string>(const std::string& str)
{
    return str;
}

template <>
inline bool parse_value<bool>(const std::string& str)
{
    if (str == "1")
    {
        return true;
    }
    if (str == "0")
    {
        return false;
    }

    std::string normalized = str;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    if (normalized == "true" || normalized == "yes" || normalized == "on")
    {
        return true;
    }
    if (normalized == "false" || normalized == "no" || normalized == "off")
    {
        return false;
    }
    throw std::runtime_error("Failed to parse string '" + str +
                             "' as bool; expected true, false, yes, no, on, off, 1, or 0");
}

template <>
inline File parse_value<File>(const std::string& str)
{
    return File(str);
}

template <>
inline std::vector<std::string> parse_value<std::vector<std::string>>(const std::string& str)
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

class Option
{
  public:
    Option() = default;
    virtual ~Option() = default;
    Option(const Option&) = default;
    Option& operator=(const Option&) = default;
    Option(Option&&) = default;
    Option& operator=(Option&&) = default;
    char short_opt = '\0';
    std::string long_opt;
    std::string help;
    ArgType arg_type = ArgType::Option;
    bool required = false;
    bool has_default = false;  // true if a default value was provided at registration
    bool was_parsed = false;   // true if the user supplied this argument on the command line
    bool deprecated = false;
    bool is_helparg = false;
    bool is_repeatable = false;
    bool is_variadic = false;  // true if this positional absorbs all remaining positional values

    virtual std::string to_string() const = 0;
    virtual void from_string(const std::string& value) = 0;
    virtual void append_repeatable(const std::string& value) = 0;
    virtual const std::type_info& get_type() const = 0;
    virtual std::string display_type() const = 0;
    virtual void* get_value() = 0;
    virtual void reset() = 0;

    std::string label_type() const { return arg_type == ArgType::Flag ? "bool" : display_type(); }
};

template <typename Derived, typename T>
class OptionValueBase : public Option
{
  public:
    T value;
    T initial_value;

    std::string to_string() const override { return into_string(value); }

    const std::type_info& get_type() const override { return typeid(T); }

    std::string display_type() const override { return TypeName<T>::name(); }

    void* get_value() override { return &value; }

    void reset() override
    {
        value = initial_value;
        this->was_parsed = false;
    }

    std::runtime_error value_error(const std::string& input) const
    {
        return std::runtime_error("Invalid value '" + input + "' for argument '--" + this->long_opt +
                                  "': expected " + display_type() + ".");
    }
};

template <typename T, typename U = void>
class Argument : public OptionValueBase<Argument<T, U>, T>
{
  public:
    U constraint;

    void from_string(const std::string& str) override
    {
        try
        {
            this->value = parse_value<T>(str);
        }
        catch (const std::exception&)
        {
            throw this->value_error(str);
        }
        if (!constraint.check(this->value))
            throw std::runtime_error("Invalid value '" + str + "' for argument '--" + this->long_opt +
                                     "': expected " + this->display_type() + " " +
                                     constraint_expectation(constraint.desc()) + ".");
    }

    void append_repeatable(const std::string& str) override
    {
        try
        {
            append_repeatable_impl<T>(str);
        }
        catch (const std::exception&)
        {
            throw this->value_error(str);
        }
    }

  private:
    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string& str)
    {
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string&)
    {
        throw std::runtime_error("Argument '--" + this->long_opt +
                                 "' is marked repeatable but its type does not support push_back");
    }
};

template <typename T>
class Argument<T, void> : public OptionValueBase<Argument<T, void>, T>
{
  public:
    void from_string(const std::string& str) override
    {
        try
        {
            from_string_impl<T>(str);
        }
        catch (const std::exception&)
        {
            throw this->value_error(str);
        }
    }

    void append_repeatable(const std::string& str) override
    {
        try
        {
            append_repeatable_impl<T>(str);
        }
        catch (const std::exception&)
        {
            throw this->value_error(str);
        }
    }

  private:
    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type from_string_impl(const std::string& str)
    {
        this->value = parse_value<Q>(str);
    }

    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type from_string_impl(const std::string& str)
    {
        this->value.clear();
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string& str)
    {
        this->value.push_back(parse_value<typename Q::value_type>(str));
    }

    template <typename Q>
    typename std::enable_if<!is_appendable<Q>::value, void>::type append_repeatable_impl(const std::string&)
    {
        throw std::runtime_error("Argument '--" + this->long_opt +
                                 "' is marked repeatable but its type does not support push_back");
    }
};

}  // namespace subcommand_detail

#ifndef ARGUMENTS_PARSER_API_TYPES_DEFINED
#define ARGUMENTS_PARSER_API_TYPES_DEFINED
template <typename T>
class Range
{
    T min_value, max_value;
    BoundType type;

  public:
    Range() : min_value(), max_value(), type(BoundType::Closed) {}
    Range(T min, T max, BoundType t = BoundType::Closed) : min_value(min), max_value(max), type(t) {}

    bool check(const T& v) const
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
    bool check(const T& v) const { return std::find(values.begin(), values.end(), v) != values.end(); }

    std::string desc() const
    {
        std::string s = " Choices: {";
        for (size_t i = 0; i < values.size(); ++i)
        {
            s += subcommand_detail::into_string(values[i]);
            if (i + 1 < values.size())
            {
                s += ", ";
            }
        }
        s += "}";
        return s;
    }
};

// Distinguish between explicit 0 and construct T(), analogous to std::optional
template <typename T>
class Expected
{
  private:
    bool is_explicit;
    T value;

  public:
    Expected() : is_explicit(false), value() {}
    Expected(const T& v) : is_explicit(true), value(v) {}
    Expected(const char* v) : is_explicit(true), value(v) {}
    bool has_value() const { return is_explicit; }
    T get() const { return value; }
};
#endif

class ArgumentOptions
{
    friend class SubCommandParser;

    using Option = subcommand_detail::Option;
    template <typename T, typename U = void>
    using Argument = subcommand_detail::Argument<T, U>;
    template <typename T>
    using TypeName = subcommand_detail::TypeName<T>;

  public:
    ArgumentOptions() = default;
    ArgumentOptions(const std::string& programname, const std::string& description)
        : programname_(programname), description_(description)
    {
    }

    template <typename T>
    void add_positional(const std::string& long_opt, const std::string& help, bool required = false)
    {
        add_argument<T>('\0', long_opt, ArgType::Positional, help, Expected<T>(), required);
    }

    // Variadic positional: absorbs all remaining positional values into a vector<T>.
    // Must be registered last; only one variadic positional is allowed.
    template <typename T>
    void add_positional_variadic(const std::string& long_opt, const std::string& help, bool required = false)
    {
        for (const auto& name : positionals_)
        {
            if (arguments_.count(name) && arguments_.at(name)->is_variadic)
                throw std::runtime_error("Only one variadic positional is allowed; '" + name +
                                         "' is already variadic.");
        }
        add_argument<std::vector<T>>('\0', long_opt, ArgType::Positional, help, Expected<std::vector<T>>(), required,
                                     false, true);
        arguments_[long_opt]->is_variadic = true;
    }

    template <typename T>
    void add_required(const char short_opt, const std::string& long_opt, const std::string& help)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Option, help, Expected<T>(), true);
    }

    template <typename T>
    void add_required(const std::string& long_opt, const std::string& help)
    {
        add_argument<T>('\0', long_opt, ArgType::Option, help, Expected<T>(), true);
    }

    template <typename T>
    void add_optional(const char short_opt, const std::string& long_opt, const std::string& help,
                      const Expected<T>& expected)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Option, help, expected);
    }

    template <typename T>
    void add_optional(const char short_opt, const std::string& long_opt, const std::string& help)
    {
        add_argument<T>(short_opt, long_opt, ArgType::Option, help, Expected<T>());
    }

    template <typename T>
    void add_optional(const std::string& long_opt, const std::string& help, const Expected<T>& expected)
    {
        add_argument<T>('\0', long_opt, ArgType::Option, help, expected);
    }

    template <typename T>
    void add_optional(const std::string& long_opt, const std::string& help)
    {
        add_argument<T>('\0', long_opt, ArgType::Option, help, Expected<T>());
    }

    template <typename T, typename U>
    void add_optional(const char short_opt, const std::string& long_opt, const std::string& help,
                      const Expected<T>& expected, const U& constraint)
    {
        add_argument<T, U>(short_opt, long_opt, ArgType::Option, help, expected, constraint);
    }

    template <typename T, typename U>
    void add_optional(const std::string& long_opt, const std::string& help, const Expected<T>& expected,
                      const U& constraint)
    {
        add_argument<T, U>('\0', long_opt, ArgType::Option, help, expected, constraint);
    }

    void add_flag(const char short_opt, const std::string& long_opt, const std::string& help)
    {
        add_argument<bool>(short_opt, long_opt, ArgType::Flag, help, Expected<bool>());
    }

    void add_flag(const std::string& long_opt, const std::string& help)
    {
        add_argument<bool>('\0', long_opt, ArgType::Flag, help, Expected<bool>());
    }

    template <typename T>
    void add_repeatable(const char short_opt, const std::string& long_opt, const std::string& help,
                        bool required = false)
    {
        add_argument<std::vector<T>>(short_opt, long_opt, ArgType::Option, help, Expected<std::vector<T>>(), required,
                                     false, true);
    }

    template <typename T>
    void add_repeatable(const std::string& long_opt, const std::string& help, bool required = false)
    {
        add_argument<std::vector<T>>('\0', long_opt, ArgType::Option, help, Expected<std::vector<T>>(), required,
                                     false, true);
    }

    void add_help(char short_opt = 'h', const std::string& long_opt = "help",
                  const std::string& help = "Show help message")
    {
        add_argument<bool>(short_opt, long_opt, ArgType::Flag, help, Expected<bool>(), false, true);
    }

    bool was_set(const std::string& name) const
    {
        auto it = arguments_.find(name);
        return it != arguments_.end() && it->second->was_parsed;
    }

    bool was_set(const char short_opt) const
    {
        auto it = short_to_long_.find(short_opt);
        if (it == short_to_long_.end())
        {
            return false;
        }
        return was_set(it->second);
    }

    template <typename T>
    T get(const std::string& name) const
    {
        Option* arg = nullptr;
        std::string key;
        if (name.size() == 1)
        {
            char sopt = name.front();
            auto sit = short_to_long_.find(sopt);
            if (sit == short_to_long_.end())
                throw std::runtime_error("Argument not found: -" + name);
            auto it = arguments_.find(sit->second);
            arg = it->second.get();
            key = "-" + name;
        }
        else
        {
            auto it = arguments_.find(name);
            if (it == arguments_.end())
                throw std::runtime_error("Argument not found: --" + name);
            arg = it->second.get();
            key = "--" + name;
        }

        if (arg->get_type() != typeid(T))
        {
            throw std::runtime_error("Argument type mismatch: " + key + ", expected " + TypeName<T>::name() + ", got " +
                                     arg->display_type());
        }
        void* ptr = arg->get_value();
        if (!ptr)
            throw std::runtime_error("Argument value is null: " + key);
        return *static_cast<T*>(ptr);
    }

    template <typename T>
    T get(const char short_opt) const
    {
        return get<T>(std::string(1, short_opt));
    }

    bool exists(const std::string& name) const { return arguments_.find(name) != arguments_.end(); }

    bool exists(const char short_opt) const
    {
        if (short_opt == '\0')
        {
            return false;
        }
        return short_to_long_.find(short_opt) != short_to_long_.end();
    }

    void set_program_name(const std::string& name) { programname_ = name; }
    std::string get_program_name() const { return programname_; }
    void set_description(const std::string& description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string& version) { version_ = version; }
    std::string get_version() const { return version_; }

    bool deprecate(const std::string& long_opt)
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
        bool found = (it != short_to_long_.end());
        if (found)
        {
            arguments_[it->second]->deprecated = true;
        }
        return found;
    }

    void footnote(const std::string& note) { note_ = note; }

  protected:
    bool has_required() const
    {
        for (const auto& pair : arguments_)
        {
            if (pair.second->required)
                return true;
        }
        return false;
    }

    bool is_registered_option_token(const std::string& token) const
    {
        if (subcommand_detail::is_longopt(token))
        {
            std::string name = token.substr(2);
            const size_t equals = name.find('=');
            if (equals != std::string::npos)
                name = name.substr(0, equals);
            return arguments_.find(name) != arguments_.end();
        }
        return subcommand_detail::is_shortopt(token) && short_to_long_.find(token[1]) != short_to_long_.end();
    }

    bool has_helparg() const { return get_helparg() != nullptr; }

    Option* get_helparg() const
    {
        for (const auto& pair : arguments_)
        {
            if (pair.second->is_helparg)
                return pair.second.get();
        }
        return nullptr;
    }

    class Arg
    {
      public:
        template <typename T>
        static std::unique_ptr<Option> argument(const T& expected)
        {
            std::unique_ptr<Argument<T>> arg(new Argument<T>());
            arg->value = expected;
            arg->initial_value = expected;
            return arg;
        }

        template <typename T, typename U>
        static std::unique_ptr<Option> argument(const T& expected, const U& constraint)
        {
            std::unique_ptr<Argument<T, U>> arg(new Argument<T, U>());
            arg->value = expected;
            arg->initial_value = expected;
            arg->constraint = constraint;
            return arg;
        }
    };

    template <typename T>
    void add_argument_impl(std::unique_ptr<Option> arg, const char short_opt, const std::string& long_opt, ArgType type,
                           const std::string& help, const Expected<T>& expected, bool required = false,
                           bool is_helparg = false, bool is_repeatable = false)
    {
        if (long_opt.empty())
            throw std::runtime_error("Long option cannot be empty.");
        if (long_opt[0] == '-' || long_opt.find('=') != std::string::npos ||
            std::find_if(long_opt.begin(), long_opt.end(), [](char c) {
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            }) != long_opt.end())
            throw std::runtime_error("Invalid long option name: '" + long_opt + "'.");
        if (short_opt != '\0' &&
            (short_opt == '-' || short_opt == '=' || !std::isprint(static_cast<unsigned char>(short_opt)) ||
             std::isspace(static_cast<unsigned char>(short_opt))))
            throw std::runtime_error("Invalid short option name.");
        if (short_opt != '\0' && exists(short_opt))
            throw std::runtime_error("Short option '" + std::string(1, short_opt) + "' already exists.");
        if (exists(long_opt))
            throw std::runtime_error("Long option '" + long_opt + "' already exists.");
        if (type == ArgType::Flag && required)
            throw std::runtime_error("Flag argument '--" + long_opt + "' cannot be required.");
        if (type == ArgType::Positional && required)
        {
            for (const auto& name : positionals_)
            {
                if (!arguments_.at(name)->required)
                    throw std::runtime_error("Required positional argument '" + long_opt +
                                             "' cannot follow optional positional argument '" + name + "'.");
            }
        }
        if (subcommand_detail::is_positional_type(type) && !positionals_.empty() &&
            arguments_.at(positionals_.back())->is_variadic)
            throw std::runtime_error("Cannot register positional argument '" + long_opt +
                                     "' after variadic positional argument '" + positionals_.back() + "'.");

        arg->short_opt = short_opt;
        arg->long_opt = long_opt;
        arg->help = help;
        arg->arg_type = type;
        arg->required = required;
        arg->has_default = expected.has_value();
        arg->was_parsed = false;
        arg->is_helparg = is_helparg;
        arg->is_repeatable = is_repeatable;
        if (subcommand_detail::is_positional_type(type))
            positionals_.push_back(long_opt);
        if (short_opt != '\0')
            short_to_long_[short_opt] = long_opt;
        arguments_[long_opt] = std::move(arg);
        order_.push_back(long_opt);
    }

    template <typename T>
    void add_argument(const char short_opt, const std::string& long_opt, ArgType type, const std::string& help,
                      const Expected<T>& expected = Expected<T>(), bool required = false, bool is_helparg = false,
                      bool is_repeatable = false)
    {
        add_argument_impl(Arg::argument(expected.get()), short_opt, long_opt, type, help, expected, required,
                          is_helparg, is_repeatable);
    }

    template <typename T, typename U>
    void add_argument(const char short_opt, const std::string& long_opt, ArgType type, const std::string& help,
                      const Expected<T>& expected = Expected<T>(), const U& constraint = U(), bool required = false)
    {
        if (expected.has_value() && !constraint.check(expected.get()))
        {
            throw std::runtime_error("Invalid default value '" + subcommand_detail::into_string(expected.get()) +
                                     "' for argument '--" + long_opt + "': expected " +
                                     subcommand_detail::TypeName<T>::name() + " " +
                                     subcommand_detail::constraint_expectation(constraint.desc()) + ".");
        }
        add_argument_impl(Arg::argument(expected.get(), constraint), short_opt, long_opt, type,
                          help + constraint.desc(), expected, required);
    }

    std::string programname_;
    std::string description_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Option>> arguments_;
    std::unordered_map<char, std::string> short_to_long_;  // short opt -> long opt index
    std::vector<std::string> order_;
    std::vector<std::string> positionals_;
};

class SubCommandParser
{
    using Callable = std::function<int(ArgumentOptions& parser)>;
    enum class ParseResult
    {
        Success,
        HelpRequested,
        VersionRequested,
        SubcommandHelpRequested
    };

  public:
    SubCommandParser(const std::string& programname, const std::string& description)
        : programname_(programname), description_(description)
    {
    }

    ArgumentOptions& add_subcommand(const std::string& name, const std::string& description, Callable callable)
    {
        if (name.empty())
            throw std::runtime_error("Command name cannot be empty.");
        if (name[0] == '-' || name.find('=') != std::string::npos ||
            std::find_if(name.begin(), name.end(), [](char c) {
                const unsigned char value = static_cast<unsigned char>(c);
                return !std::isprint(value) || std::isspace(value);
            }) != name.end())
            throw std::runtime_error("Invalid command name: '" + name + "'.");
        if (name == "help")
            throw std::runtime_error("Command name 'help' is reserved.");
        if (commands_.find(name) != commands_.end())
            throw std::runtime_error("Command '" + name + "' already exists.");
        if (!callable)  // Check for empty std::function
            throw std::runtime_error("Callable function cannot be empty.");
        std::unique_ptr<Command> cmd(
            new Command(std::move(callable), std::unique_ptr<ArgumentOptions>(new ArgumentOptions(name, description))));

        ArgumentOptions& parser_ref = *cmd->parser;
        commands_[name] = std::move(cmd);
        command_order_.push_back(name);
        return parser_ref;
    }

    /// Parses command-line arguments.
    ///
    /// Returns true when a subcommand is ready to execute. Returns false after
    /// help or version output has been handled. Invalid arguments throw.
    [[nodiscard]] bool parse_args(int argc, char* const argv[])
    {
        current_command_.clear();
        requested_subcommand_.clear();
        skip_execution_ = true;

        switch (parse_args_impl(argc, argv))
        {
            case ParseResult::Success:
                skip_execution_ = false;
                return true;

            case ParseResult::HelpRequested:
                print_help();
                return false;

            case ParseResult::VersionRequested:
                print_version();
                return false;

            case ParseResult::SubcommandHelpRequested:
                print_subcommand_help(requested_subcommand_);
                return false;
        }

        throw std::logic_error("Unhandled parse result");
    }

  private:
    static void validate_arguments(int argc, char* const argv[])
    {
        if (argc <= 0 || argv == nullptr || argv[0] == nullptr)
            throw std::runtime_error("Invalid argc/argv passed to parse_args.");

        for (int index = 0; index < argc; ++index)
        {
            if (argv[index] == nullptr)
                throw std::runtime_error("Invalid null argument in argv.");
        }
    }

    static subcommand_detail::Option* find_short_argument(ArgumentOptions& parser, char short_option)
    {
        if (parser.short_to_long_.count(short_option) == 0)
            return nullptr;

        return parser.arguments_.at(parser.short_to_long_.at(short_option)).get();
    }

    static void assign_value(subcommand_detail::Option& argument, const std::string& value)
    {
        if (argument.is_repeatable)
            argument.append_repeatable(value);
        else
            argument.from_string(value);
    }

    ParseResult parse_args_impl(int argc, char* const argv[])
    {
        validate_arguments(argc, argv);
        if (programname_.empty())
            programname_ = argv[0];
        if (argc < 2)
            return ParseResult::HelpRequested;
        // Everything after the subcommand belongs exclusively to the subcommand parser.
        std::string subcmd_name = argv[1];
        if (version_enabled_ &&
            ((version_short_opt_ != '\0' && subcmd_name == std::string("-") + version_short_opt_) ||
             subcmd_name == std::string("--") + version_long_opt_))
        {
            return ParseResult::VersionRequested;
        }
        if (subcmd_name == "-h" || subcmd_name == "--help")
        {
            return ParseResult::HelpRequested;
        }
        if (subcmd_name == "help")
        {
            if (argc >= 3)
            {
                std::string subcmd = argv[2];
                if (commands_.find(subcmd) == commands_.end())
                {
                    throw std::runtime_error("Unknown subcommand: " + subcmd + available_commands_suffix());
                }
                requested_subcommand_ = subcmd;
                return ParseResult::SubcommandHelpRequested;
            }
            return ParseResult::HelpRequested;
        }

        if (commands_.find(subcmd_name) == commands_.end())
        {
            throw std::runtime_error("Unknown subcommand: " + subcmd_name + available_commands_suffix());
        }
        // Check if all arguments are optional
        auto& cmd = commands_[subcmd_name];
        ArgumentOptions& parser = *cmd->parser;
        if (argc < 3 && !parser.has_required() && parser.has_helparg())
        {
            // No arguments provided, show help
            requested_subcommand_ = subcmd_name;
            return ParseResult::SubcommandHelpRequested;
        }
        const ParseResult subcommand_result = parse_subcommand_args(subcmd_name, argc - 1, argv + 1);
        if (subcommand_result != ParseResult::Success)
        {
            requested_subcommand_ = subcmd_name;
            return subcommand_result;
        }
        current_command_ = subcmd_name;
        return ParseResult::Success;
    }

  public:
    int execute()
    {
        if (skip_execution_)
            return 0;
        if (current_command_.empty())
            throw std::runtime_error("Error: No command selected (did you call parse_args?)");
        auto& cmd = commands_[current_command_];
        return cmd->callable(*cmd->parser);
    }

    void print_help(std::ostream& out = std::cout) const
    {
        subcommand_detail::StreamStateGuard stream_state(out);
        out << programname_ << ": " << description_ << "\n";
        if (!version_.empty())
        {
            out << "Version: " << version_ << "\n";
        }
        out << "Usage: " << programname_ << " <command> [options]\n\n";
        out << "Options:\n";
        if (version_enabled_)
        {
            if (version_short_opt_ != '\0')
                out << "  -" << version_short_opt_ << ", ";
            else
                out << "      ";
            out << "--" << version_long_opt_ << "    " << version_help_ << "\n";
        }
        out << "  -h, --help    Show help message\n\n";
        out << "Commands:\n";

        size_t max_cmd_width = 0;
        for (const auto& name : command_order_)
        {
            max_cmd_width = std::max(max_cmd_width, name.length());
        }

        for (const auto& name : command_order_)
        {
            const auto& cmd = commands_.at(name);
            out << "  " << std::setw(subcommand_detail::stream_width(max_cmd_width)) << std::left << name << "    "
                      << cmd->parser->get_description() << "\n";
        }
        out << "\n";
        if (!note_.empty())
        {
            out << note_ << "\n";
        }
        out << "See '" + programname_ + " help <command>' to read about a specific subcommand.\n";
    }

    void print_version(std::ostream& out = std::cout) const { out << programname_ << " " << version_ << "\n"; }

    void print_subcommand_help(const std::string& subcmd_name, std::ostream& out = std::cout)
    {
        subcommand_detail::StreamStateGuard stream_state(out);
        auto it = commands_.find(subcmd_name);
        if (it == commands_.end())
        {
            return;
        }

        const auto& cmd = it->second;
        ArgumentOptions& parser = *cmd->parser;
        out << programname_ << " " << subcmd_name << ": " << parser.get_description() << "\n";
        if (!parser.get_version().empty())
        {
            out << "Version: " << parser.get_version() << "\n";
        }
        out << "Usage: " << programname_ << " " << subcmd_name << " [options]";
        for (const auto& name : parser.positionals_)
        {
            const bool variadic = parser.arguments_.count(name) && parser.arguments_.at(name)->is_variadic;
            const bool required = parser.arguments_.at(name)->required;
            out << (required ? " <" : " [") << name << (variadic ? "..." : "")
                      << (required ? ">" : "]");
        }
        out << "\n\n";

        print_argument_sections(parser, out);

        if (!note_.empty())
        {
            out << note_ << "\n\n";
        }
    }

    bool has_command(const std::string& name) const { return commands_.find(name) != commands_.end(); }
    std::string get_program_name() const { return programname_; }
    void set_program_name(const std::string& name) { programname_ = name; }
    std::string get_current_command() const { return current_command_; }
    void set_description(const std::string& description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string& version) { version_ = version; }
    std::string get_version() const { return version_; }
    void add_version(char short_opt = 'V', const std::string& long_opt = "version",
                     const std::string& help = "Show version information")
    {
        if (long_opt.empty())
            throw std::runtime_error("Long option cannot be empty.");
        if (long_opt[0] == '-' || long_opt.find('=') != std::string::npos ||
            std::find_if(long_opt.begin(), long_opt.end(), [](char c) {
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            }) != long_opt.end())
            throw std::runtime_error("Invalid long option name: '" + long_opt + "'.");
        if (short_opt != '\0' &&
            (short_opt == '-' || short_opt == '=' || !std::isprint(static_cast<unsigned char>(short_opt)) ||
             std::isspace(static_cast<unsigned char>(short_opt))))
            throw std::runtime_error("Invalid short option name.");
        if (short_opt == 'h' || long_opt == "help")
            throw std::runtime_error("Version option conflicts with the built-in help option.");
        version_enabled_ = true;
        version_short_opt_ = short_opt;
        version_long_opt_ = long_opt;
        version_help_ = help;
    }
    void footnote(const std::string& note) { note_ = note; }

  private:
    using Option = subcommand_detail::Option;

    std::string available_commands_suffix() const
    {
        if (command_order_.empty())
            return ".";
        std::string result = ". Available commands: ";
        for (size_t i = 0; i < command_order_.size(); ++i)
        {
            if (i != 0)
                result += ", ";
            result += command_order_[i];
        }
        return result + ".";
    }

    static void reject_duplicate(const Option* argument)
    {
        if (argument->was_parsed && !argument->is_repeatable)
            throw std::runtime_error("Argument '--" + argument->long_opt + "' was provided more than once.");
    }

    void print_argument_sections(ArgumentOptions& parser, std::ostream& out)
    {
        const auto& arguments = parser.arguments_;

        // Build display order once
        auto display_order = parser.order_;
        Option* help_arg = parser.get_helparg();
        if (help_arg != nullptr)
        {
            auto it = std::find(display_order.begin(), display_order.end(), help_arg->long_opt);
            if (it != display_order.end())
                std::rotate(it, it + 1, display_order.end());
        }

        // Compute column widths in one pass (across all non-positional arguments)
        size_t opt_w = 0, type_w = 0;
        for (const auto& name : display_order)
        {
            const Option* arg = arguments.at(name).get();
            if (subcommand_detail::is_positional_type(arg->arg_type))
                continue;
            std::string opt_str;
            if (arg->short_opt != '\0')
                opt_str = std::string("-") + arg->short_opt + ", --" + arg->long_opt;
            else
                opt_str = std::string("--") + arg->long_opt;
            opt_w = std::max(opt_w, opt_str.length());

            std::string ts = arg->label_type();
            type_w = std::max(type_w, ts.length());
        }

        for (int pass = 0; pass < 2; ++pass)
        {
            bool req = (pass == 0);
            const char* section_title = req ? "Required arguments" : "Optional arguments";

            bool has_any = false;
            for (const auto& name : display_order)
            {
                const Option* arg = arguments.at(name).get();
                if (!subcommand_detail::is_positional_type(arg->arg_type) && arg->required == req)
                {
                    has_any = true;
                    break;
                }
            }
            if (!has_any)
            {
                continue;
            }

            out << section_title << ":\n";
            for (const auto& name : display_order)
            {
                const Option* arg = arguments.at(name).get();
                if (subcommand_detail::is_positional_type(arg->arg_type) || arg->required != req)
                    continue;

                out << "  ";
                std::string opt_str;
                if (arg->short_opt != '\0')
                    opt_str = std::string("-") + arg->short_opt + ", --" + arg->long_opt;
                else
                    opt_str = std::string("--") + arg->long_opt;
                out << std::setw(subcommand_detail::stream_width(opt_w)) << std::left << opt_str << "    ";

                std::string type_str = arg->label_type();
                out << std::setw(subcommand_detail::stream_width(type_w)) << std::left << type_str << "    ";

                if (arg->deprecated)
                    out << "[Deprecated] ";

                out << arg->help;
                if (arg->has_default && arg->arg_type != ArgType::Flag)
                    out << " (default: " << arg->to_string() << ")";

                out << "\n";
            }
            out << "\n";
        }

        if (!parser.positionals_.empty())
        {
            size_t name_width = 0;
            size_t type_width = 0;
            for (const auto& name : parser.positionals_)
            {
                const Option* argument = arguments.at(name).get();
                name_width = std::max(name_width, name.length() + (argument->is_variadic ? 3U : 0U));
                type_width = std::max(type_width, argument->display_type().length());
            }

            out << "Positional arguments:\n";
            for (const auto& name : parser.positionals_)
            {
                const Option* argument = arguments.at(name).get();
                const std::string display_name = name + (argument->is_variadic ? "..." : "");
                out << "  " << std::setw(subcommand_detail::stream_width(name_width)) << std::left << display_name << "    "
                          << std::setw(subcommand_detail::stream_width(type_width)) << std::left << argument->display_type() << "    " << argument->help
                          << "\n";
            }
            out << "\n";
        }
    }

    ParseResult parse_subcommand_args(const std::string& subcmd_name, int argc, char* const argv[])
    {
        auto& cmd = commands_[subcmd_name];
        ArgumentOptions& parser = *cmd->parser;

        for (auto& pair : parser.arguments_)
            pair.second->reset();

        Option* help_arg = parser.get_helparg();

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
            if (subcommand_detail::is_longopt(arg))
            {
                std::string opt_name = arg.substr(2);
                std::string value;
                size_t eq_pos = opt_name.find('=');
                const bool has_inline_value = eq_pos != std::string::npos;
                if (has_inline_value)
                {
                    value = opt_name.substr(eq_pos + 1);
                    opt_name = opt_name.substr(0, eq_pos);
                }

                auto it = parser.arguments_.find(opt_name);
                if (it == parser.arguments_.end())
                    throw std::runtime_error("Unknown argument: --" + opt_name);

                Option* argument = it->second.get();
                if (argument->deprecated)
                    throw std::runtime_error("Argument '--" + opt_name + "' is deprecated and cannot be used.");

                if (argument == help_arg)
                {
                    if (has_inline_value)
                        throw std::runtime_error("Flag argument --" + opt_name + " does not accept a value");
                    return ParseResult::SubcommandHelpRequested;
                }

                reject_duplicate(argument);

                if (argument->arg_type == ArgType::Flag)
                {
                    if (has_inline_value)
                        throw std::runtime_error("Flag argument --" + opt_name + " does not accept a value");
                    argument->from_string("true");
                }
                else
                {
                    if (!has_inline_value)
                    {
                        if (i + 1 >= argc)
                            throw std::runtime_error("Argument --" + opt_name + " requires a value");
                        if (!subcommand_detail::accepts_option_like_value(argument->display_type()) &&
                            parser.is_registered_option_token(argv[i + 1]))
                            throw std::runtime_error("Argument '--" + opt_name + "' requires a value before '" +
                                                     std::string(argv[i + 1]) + "'.");
                        value = argv[++i];
                    }
                    assign_value(*argument, value);
                }
                argument->was_parsed = true;
            }
            else if (subcommand_detail::is_shortopt(arg))
            {
                if (arg.size() > 2)
                {
                    char first_sopt = arg[1];
                    Option* first_argument = find_short_argument(parser, first_sopt);
                    if (!first_argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + first_sopt);

                    // if the first argument is not a flag, value follows the short letter
                    if (first_argument->arg_type != ArgType::Flag)
                    {
                        reject_duplicate(first_argument);
                        std::string value = arg.substr(2);
                        assign_value(*first_argument, value);
                        first_argument->was_parsed = true;
                    }
                    else
                    {
                        for (size_t j = 1; j < arg.size(); ++j)
                        {
                            char sopt = arg[j];
                            Option* argument = find_short_argument(parser, sopt);
                            if (!argument)
                                throw std::runtime_error(std::string("Unknown short flag: -") + sopt);
                            if (argument->arg_type != ArgType::Flag)
                                throw std::runtime_error(std::string("Only flag arguments can be merged: -") + sopt);
                            reject_duplicate(argument);
                            if (argument == help_arg)
                            {
                                return ParseResult::SubcommandHelpRequested;
                            }
                            argument->from_string("true");
                            argument->was_parsed = true;
                        }
                    }
                }
                else
                {
                    char sopt = arg[1];
                    Option* argument = find_short_argument(parser, sopt);
                    if (!argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + sopt);

                    reject_duplicate(argument);

                    if (argument->arg_type == ArgType::Flag)
                    {
                        if (argument == help_arg)
                        {
                            return ParseResult::SubcommandHelpRequested;
                        }
                        argument->from_string("true");
                        argument->was_parsed = true;
                    }
                    else
                    {
                        if (i + 1 >= argc)
                            throw std::runtime_error(std::string("Argument -") + sopt + " requires a value");
                        if (!subcommand_detail::accepts_option_like_value(argument->display_type()) &&
                            parser.is_registered_option_token(argv[i + 1]))
                            throw std::runtime_error(std::string("Argument '-") + sopt +
                                                     "' requires a value before '" + argv[i + 1] + "'.");
                        std::string value = argv[++i];
                        assign_value(*argument, value);
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
        const auto& positionals = parser.positionals_;
        for (size_t i = 0; i < positionals.size(); ++i)
        {
            if (i < positional_values.size())
            {
                const std::string& name = positionals[i];
                Option* arg = parser.arguments_[name].get();
                if (arg->is_variadic)
                {
                    // Feed all remaining positional values into this argument.
                    for (size_t j = i; j < positional_values.size(); ++j)
                    {
                        arg->append_repeatable(positional_values[j]);
                    }
                    arg->was_parsed = true;
                    break;
                }
                arg->from_string(positional_values[i]);
                arg->was_parsed = true;
            }
        }

        if (positional_values.size() > positionals.size() &&
            (positionals.empty() || !parser.arguments_.at(positionals.back())->is_variadic))
        {
            throw std::runtime_error("Unexpected positional argument: " + positional_values[positionals.size()]);
        }

        // check required arguments
        for (const auto& pair : parser.arguments_)
        {
            if (pair.second->required && !pair.second->was_parsed)
                throw std::runtime_error("Required argument missing: " + pair.first);
        }
        return ParseResult::Success;
    }

    struct Command
    {
        Callable callable = nullptr;
        std::unique_ptr<ArgumentOptions> parser = nullptr;
        Command(Callable c, std::unique_ptr<ArgumentOptions> a) : callable(std::move(c)), parser(std::move(a)) {}
    };

    std::string programname_;
    std::string description_;
    std::string current_command_;
    std::string requested_subcommand_;
    std::string version_;
    bool version_enabled_ = false;
    bool skip_execution_ = false;
    char version_short_opt_ = 'V';
    std::string version_long_opt_ = "version";
    std::string version_help_ = "Show version information";
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
    std::vector<std::string> command_order_;
};

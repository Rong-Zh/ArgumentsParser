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

class Option
{
public:
    virtual ~Option() {}
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

// CRTP base: holds value and implements the three repeated virtual methods
template <typename Derived, typename T>
class OptionValueBase : public Option
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
class Argument : public OptionValueBase<Argument<T, U>, T>
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
class Argument<T, void> : public OptionValueBase<Argument<T, void>, T>
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

class ArgumentOptions
{
    friend class SubCommandParser;

public:
    ArgumentOptions() {}
    ArgumentOptions(const std::string &programname, const std::string &description)
        : programname_(programname), description_(description) {}

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
    void add_optional(const std::string &long_opt, const std::string &help, const Expected<T> &expected)
    {
        add_argument<T>('\0', long_opt, ArgType::Optional, help, expected);
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
        Option *arg = nullptr;
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

    void set_program_name(const std::string &name) { programname_ = name; }
    std::string get_program_name() const { return programname_; }
    void set_description(const std::string &description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string &version) { version_ = version; }
    std::string get_version() const { return version_; }

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

    void footnote(const std::string &note) { note_ = note; }

protected:
    bool has_required() const
    {
        for (const auto &pair : arguments_)
        {
            if (pair.second->required)
                return true;
        }
        return false;
    }

    bool has_helparg() const
    {
        return get_helparg() != nullptr;
    }

    Option *get_helparg() const
    {
        for (const auto &pair : arguments_)
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
        static std::unique_ptr<Option> argument(const T &expected)
        {
            std::unique_ptr<Argument<T>> arg(new Argument<T>());
            arg->value = expected;
            return arg;
        }

        template <typename T, typename U>
        static std::unique_ptr<Option> argument(const T &expected, const U &constraint)
        {
            std::unique_ptr<Argument<T, U>> arg(new Argument<T, U>());
            arg->value = expected;
            arg->constraint = constraint;
            return arg;
        }
    };

    template <typename T>
    void add_argument_impl(
        std::unique_ptr<Option> arg,
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
        if (!constraint.check(expected.get()))
        {
            throw std::runtime_error(
                "Argument: '" + long_opt +
                "', value: " + into_string(expected.get()) +
                " does not satisfy constraint: " + constraint.desc());
        }
        add_argument_impl(Arg::argument(expected.get(), constraint), short_opt, long_opt, type, help + constraint.desc(), expected);
    }

    std::string programname_;
    std::string description_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Option>> arguments_;
    std::unordered_map<char, std::string> short_to_long_; // short opt -> long opt index
    std::vector<std::string> order_;
    std::vector<std::string> positionals_;
};

class SubCommandParser
{
    using Callable = std::function<int(ArgumentOptions &parser)>;

public:
    SubCommandParser(const std::string &programname, const std::string &description)
        : programname_(programname), description_(description) {}

    ArgumentOptions &add_subcommand(const std::string &name, const std::string &description, Callable callable)
    {
        if (name.empty())
            throw std::runtime_error("Command name cannot be empty.");
        if (commands_.find(name) != commands_.end())
            throw std::runtime_error("Command '" + name + "' already exists.");
        if (!callable) // Check for empty std::function
            throw std::runtime_error("Callable function cannot be empty.");
        std::unique_ptr<Command> cmd(new Command(
            std::move(callable),
            std::unique_ptr<ArgumentOptions>(new ArgumentOptions(name, description))));

        ArgumentOptions &parser_ref = *cmd->parser;
        commands_[name] = std::move(cmd);
        command_order_.push_back(name);
        return parser_ref;
    }

    void parse_args(int argc, char *argv[])
    {
        if (programname_.empty())
            programname_ = argv[0];
        if (argc < 2)
        {
            print_help();
            std::exit(EXIT_SUCCESS);
        }
        for (int i = 1; i < argc; ++i)
        {
            std::string a = argv[i];
            if (a == "--help" || a == "-h")
            {
                print_help();
                std::exit(EXIT_SUCCESS);
            }
        }
        std::string subcmd_name = argv[1];
        if (subcmd_name == "help")
        {
            if (argc >= 3)
            {
                std::string subcmd = argv[2];
                if (commands_.find(subcmd) == commands_.end())
                {
                    throw std::runtime_error("Unknown subcommand: " + subcmd + "\n");
                }
                print_subcommand_help(subcmd);
            }
            else
            {
                print_help();
            }
            std::exit(EXIT_SUCCESS);
        }

        if (commands_.find(subcmd_name) == commands_.end())
        {
            throw std::runtime_error("Unknown subcommand: " + subcmd_name + "\n");
        }
        // Check if all arguments are optional
        auto &cmd = commands_[subcmd_name];
        ArgumentOptions &parser = *cmd->parser;
        if (argc < 3 && (parser.has_required() || parser.has_helparg()))
        {
            // No arguments provided, show help
            print_subcommand_help(subcmd_name);
            std::exit(EXIT_SUCCESS);
        }
        parse_args(subcmd_name, argc - 1, argv + 1);
        current_command_ = subcmd_name;
    }

    int execute()
    {
        if (current_command_.empty())
            throw std::runtime_error("Error: No command selected (did you call parse_args?)");
        auto &cmd = commands_[current_command_];
        return cmd->callable(*cmd->parser);
    }

    void print_help() const
    {
        std::cout << programname_ << ": " << description_ << "\n";
        if (!version_.empty())
        {
            std::cout << "Version: " << version_ << "\n";
        }
        std::cout << "Usage: " << programname_ << " <command> [options]\n\n";
        std::cout << "Commands:\n";

        size_t max_cmd_width = 0;
        for (const auto &name : command_order_)
        {
            max_cmd_width = std::max(max_cmd_width, name.length());
        }

        for (const auto &name : command_order_)
        {
            const auto &cmd = commands_.at(name);
            std::cout << "  " << std::setw(max_cmd_width) << std::left << name
                      << "    " << cmd->parser->get_description() << "\n";
        }
        std::cout << "\n";
        if (!note_.empty())
        {
            std::cout << note_ << "\n";
        }
        std::cout << "See '" + programname_ + " help <command>' to read about a specific subcommand.\n";
    }

    void print_subcommand_help(const std::string &subcmd_name)
    {
        auto it = commands_.find(subcmd_name);
        if (it == commands_.end())
            return;

        const auto &cmd = it->second;
        ArgumentOptions &parser = *cmd->parser;
        std::cout << programname_ << " " << subcmd_name << ": " << parser.get_description() << "\n";
        if (!parser.get_version().empty())
        {
            std::cout << "Version: " << parser.get_version() << "\n";
        }
        std::cout << "Usage: " << programname_ << " " << subcmd_name << " [options]";
        for (const auto &name : parser.positionals_)
        {
            std::cout << " <" << name << ">";
        }
        std::cout << "\n\n";

        print_argument_sections(parser);

        if (!note_.empty())
        {
            std::cout << note_ << "\n\n";
        }
    }

    bool has_command(const std::string &name) const { return commands_.find(name) != commands_.end(); }
    std::string get_program_name() const { return programname_; }
    void set_program_name(const std::string &name) { programname_ = name; }
    std::string get_current_command() const { return current_command_; }
    void set_description(const std::string &description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string &version) { version_ = version; }
    std::string get_version() const { return version_; }
    void footnote(const std::string &note) { note_ = note; }

private:
    void print_argument_sections(ArgumentOptions &parser)
    {
        const auto &arguments = parser.arguments_;

        // Build display order once
        auto display_order = parser.order_;
        Option *help_arg = parser.get_helparg();
        if (help_arg)
        {
            auto it = std::find(display_order.begin(), display_order.end(), help_arg->long_opt);
            if (it != display_order.end())
                std::rotate(it, it + 1, display_order.end());
        }

        // Compute column widths in one pass (across all non-positional arguments)
        size_t opt_w = 0, type_w = 0;
        for (const auto &name : display_order)
        {
            const Option *arg = arguments.at(name).get();
            if (arg->arg_type == ArgType::Positional)
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
            const char *section_title = req ? "Required arguments" : "Optional arguments";

            bool has_any = false;
            for (const auto &name : display_order)
            {
                const Option *arg = arguments.at(name).get();
                if (arg->arg_type != ArgType::Positional && arg->required == req)
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
                const Option *arg = arguments.at(name).get();
                if (arg->arg_type == ArgType::Positional || arg->required != req)
                    continue;

                std::cout << "  ";
                std::string opt_str;
                if (arg->short_opt != '\0')
                    opt_str = std::string("-") + arg->short_opt + ", --" + arg->long_opt;
                else
                    opt_str = std::string("--") + arg->long_opt;
                std::cout << std::setw(opt_w) << std::left << opt_str << "    ";

                std::string type_str = arg->label_type();
                std::cout << std::setw(type_w) << std::left << type_str << "    ";

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

    void parse_args(std::string subcmd_name, int argc, char *argv[])
    {
        auto &cmd = commands_[subcmd_name];
        ArgumentOptions &parser = *cmd->parser;

        // Check for help flag if enabled
        Option *help_arg = parser.get_helparg();
        if (help_arg)
        {
            std::string help_short;
            if (help_arg->short_opt != '\0')
                help_short = std::string("-") + std::string(1, help_arg->short_opt);
            std::string help_long = std::string("--") + help_arg->long_opt;

            for (int i = 1; i < argc; ++i)
            {
                std::string a = argv[i];
                if ((!help_short.empty() && a == help_short) || a == help_long)
                {
                    print_subcommand_help(subcmd_name);
                    std::exit(EXIT_SUCCESS);
                }
            }
        }

        std::vector<std::string> positional_values;
        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];
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

                auto it = parser.arguments_.find(opt_name);
                if (it == parser.arguments_.end())
                    throw std::runtime_error("Unknown argument: --" + opt_name);

                Option *argument = it->second.get();
                if (argument->deprecated)
                    throw std::runtime_error("Argument '--" + opt_name + "' is deprecated and cannot be used.");

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
            else if (utils::is_shortopt(arg))
            {
                if (arg.size() > 2)
                {
                    char first_sopt = arg[1];
                    Option *first_argument = nullptr;
                    {
                        auto sit = parser.short_to_long_.find(first_sopt);
                        if (sit != parser.short_to_long_.end())
                            first_argument = parser.arguments_.at(sit->second).get();
                    }
                    if (!first_argument)
                        throw std::runtime_error(std::string("Unknown argument: -") + first_sopt);

                    // if the first argument is not a flag, value follows the short letter
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
                            Option *argument = nullptr;
                            {
                                auto sit = parser.short_to_long_.find(sopt);
                                if (sit != parser.short_to_long_.end())
                                    argument = parser.arguments_.at(sit->second).get();
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
                    Option *argument = nullptr;
                    {
                        auto sit = parser.short_to_long_.find(sopt);
                        if (sit != parser.short_to_long_.end())
                            argument = parser.arguments_.at(sit->second).get();
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
        const auto &positionals = parser.positionals_;
        for (size_t i = 0; i < positionals.size(); ++i)
        {
            if (i < positional_values.size())
            {
                const std::string &name = positionals[i];
                parser.arguments_[name]->from_string(positional_values[i]);
                parser.arguments_[name]->was_parsed = true;
            }
        }

        // check required arguments
        for (const auto &pair : parser.arguments_)
        {
            if (pair.second->required && !pair.second->was_parsed)
                throw std::runtime_error("Required argument missing: " + pair.first);
        }
    }

    struct Command
    {
        Callable callable;
        std::unique_ptr<ArgumentOptions> parser;
        Command(Callable c, std::unique_ptr<ArgumentOptions> a)
            : callable(std::move(c)), parser(std::move(a)) {}
    };

    std::string programname_;
    std::string description_;
    std::string current_command_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
    std::vector<std::string> command_order_;
};
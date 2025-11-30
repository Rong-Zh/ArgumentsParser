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
}

class Option
{
public:
    virtual ~Option() {}
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

template <typename T, typename U = void>
class Argument : public Option
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

template <typename T>
class Argument<T, void> : public Option
{
public:
    T value;

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

class ArgumentOptions
{
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
    T get(const std::string &name) const
    {
        Option *arg = nullptr;
        std::string key;
        if (name.size() == 1)
        {
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

    void set_program_name(const std::string &name) { programname_ = name; }
    std::string get_program_name() const { return programname_; }
    void set_description(const std::string &description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string &version) { version_ = version; }
    std::string get_version() const { return version_; }

    const std::vector<std::string> &get_order() const { return order_; }
    const std::vector<std::string> &get_positionals() const { return positionals_; }

    const std::unordered_map<std::string, std::unique_ptr<Option>> &get_arguments() const
    {
        return arguments_;
    }

    std::unordered_map<std::string, std::unique_ptr<Option>> &get_arguments()
    {
        return arguments_;
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

    void footnote(const std::string &note) { note_ = note; }

protected:
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

    template <typename T>
    void add_argument(
        const char short_opt,
        const std::string &long_opt,
        ArgType type,
        const std::string &help,
        const Expected<T> &expected = Expected<T>())
    {
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

    std::string programname_;
    std::string description_;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Option>> arguments_;
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
        ready_to_execute_ = false; // reset flag

        if (argc < 2)
        {
            print_help();
            std::exit(EXIT_SUCCESS);
        }

        std::string subcmd_name = argv[1];
        if (commands_.find(subcmd_name) == commands_.end())
        {
            print_help();
            throw std::runtime_error("Unknown subcommand: " + subcmd_name);
        }
        current_command = subcmd_name;

        if (argc >= 3)
        {
            parse_args(current_command, argc - 1, argv + 1);
            ready_to_execute_ = true;
        }
        else
        {
            print_subcommand_help(current_command);
        }
    }

    template <typename R>
    bool execute(R &result)
    {
        if (!ready_to_execute_ || commands_.empty() || current_command.empty())
            return false;

        auto &cmd = commands_[current_command];
        if (cmd->callable)
        {
            result = cmd->callable(*cmd->parser);
            return true;
        }
        return false;
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
            std::cout << note_ << "\n\n";
        }
    }

    void print_subcommand_help(const std::string &subcmd_name) const
    {
        auto it = commands_.find(subcmd_name);
        if (it == commands_.end())
            return;

        const auto &cmd = it->second;
        const ArgumentOptions &parser = *cmd->parser;

        std::cout << programname_ << " " << subcmd_name << ": " << parser.get_description() << "\n";
        if (!parser.get_version().empty())
        {
            std::cout << "Version: " << parser.get_version() << "\n";
        }

        std::cout << "Usage: " << programname_ << " " << subcmd_name << " [options]";

        for (const auto &name : parser.get_positionals())
        {
            std::cout << " <" << name << ">";
        }

        std::cout << "\n\n";

        print_argument_details("Required arguments", parser, true);
        print_argument_details("Optional arguments", parser, false);

        if (!note_.empty())
        {
            std::cout << note_ << "\n\n";
        }
    }

    bool has_command(const std::string &name) const { return commands_.find(name) != commands_.end(); }
    std::string get_program_name() const { return programname_; }
    void set_program_name(const std::string &name) { programname_ = name; }
    std::string get_current_command() const { return current_command; }
    void set_description(const std::string &description) { description_ = description; }
    std::string get_description() const { return description_; }
    void set_version(const std::string &version) { version_ = version; }
    std::string get_version() const { return version_; }
    void footnote(const std::string &note) { note_ = note; }

private:
    void print_argument_details(const std::string &title, const ArgumentOptions &parser, bool show_required) const
    {
        const auto &arguments = parser.get_arguments();
        const auto &option_order = parser.get_order();

        bool has_args = false;
        for (const auto &name : option_order)
        {
            const Option *arg = arguments.at(name).get();
            if (arg->required == show_required && arg->arg_type != ArgType::Positional)
            {
                has_args = true;
                break;
            }
        }

        if (!has_args)
            return;

        size_t opt_w = 0, type_w = 0;
        for (const auto &name : option_order)
        {
            const Option *arg = arguments.at(name).get();
            if (arg->required == show_required && arg->arg_type != ArgType::Positional)
            {
                std::string opt_str;
                if (arg->short_opt != '\0')
                {
                    opt_str = std::string("-") + arg->short_opt + ", --" + arg->long_opt;
                }
                else
                {
                    opt_str = std::string("--") + arg->long_opt;
                }
                opt_w = std::max(opt_w, opt_str.length());
                type_w = std::max(type_w, type_name(arg->get_type()).length());
            }
        }

        std::cout << title << ":\n";
        for (const auto &name : option_order)
        {
            const Option *arg = arguments.at(name).get();
            if (arg->required == show_required && arg->arg_type != ArgType::Positional)
            {
                std::cout << "  ";

                // option name
                std::string opt_str;
                if (arg->short_opt != '\0')
                {
                    opt_str = std::string("-") + arg->short_opt + ", --" + arg->long_opt;
                }
                else
                {
                    opt_str = std::string("--") + arg->long_opt;
                }
                std::cout << std::setw(opt_w) << std::left << opt_str << "    ";

                std::string type_str = (arg->arg_type == ArgType::Flag) ? "flag" : type_name(arg->get_type());
                std::cout << std::setw(type_w) << std::left << type_str << "    ";

                if (arg->deprecated)
                    std::cout << "[Deprecated] ";
                std::cout << arg->help;

                if (arg->is_set && arg->arg_type != ArgType::Flag)
                    std::cout << " (default: " << arg->to_string() << ")";

                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }

    void parse_args(std::string subcmd_name, int argc, char *argv[])
    {
        auto &cmd = commands_[subcmd_name];
        ArgumentOptions &parser = *cmd->parser;
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

                const auto &arguments = parser.get_arguments();
                auto it = arguments.find(opt_name);
                if (it == arguments.end())
                    throw std::runtime_error("Unknown argument: --" + opt_name);

                Option *argument = it->second.get();
                if (argument->deprecated)
                {
                    throw std::runtime_error("Argument '--" + opt_name + "' is deprecated and cannot be used.");
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
            else if (utils::is_shortopt(arg))
            {
                const auto &arguments_ = parser.get_arguments();
                if (arg.size() > 2)
                {
                    char first_opt = arg[1];
                    Option *first_argument = nullptr;
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
                            Option *argument = nullptr;
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
                    Option *argument = nullptr;
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
        auto &arguments = parser.get_arguments();
        const auto &positionals = parser.get_positionals();
        for (size_t i = 0; i < positionals.size(); ++i)
        {
            if (i < positional_values.size())
            {
                const std::string &name = positionals[i];
                arguments[name]->from_string(positional_values[i]);
                arguments[name]->is_set = true;
            }
        }

        // check required arguments
        for (const auto &pair : arguments)
        {
            const auto &arg = pair.second;
            if (arg->required && !arg->is_set)
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
    bool ready_to_execute_ = false;
    std::string programname_;
    std::string description_;
    std::string current_command;
    std::string version_;
    std::string note_;
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
    std::vector<std::string> command_order_;
};
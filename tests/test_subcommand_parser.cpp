#include "SubCommandParser.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

// ---- Helper: build argc/argv from an initializer list -------------------
struct Args
{
    std::vector<std::string> store;
    std::vector<char *>      ptrs;

    explicit Args(std::vector<std::string> argv_strs)
        : store(std::move(argv_strs))
    {
        for (auto &s : store)
            ptrs.push_back(&s[0]);
    }

    int    argc() const { return static_cast<int>(ptrs.size()); }
    char **argv()       { return ptrs.data(); }
};

// ========================================================================
// SubCommandParser tests
// ========================================================================

static int s_sub_result = -9999;

static int sub_add_fn(ArgumentOptions &opts)
{
    int x = opts.get<int>("first");
    int y = opts.get<int>("second");
    s_sub_result = x + y;
    return 0;
}

static int sub_subtract_fn(ArgumentOptions &opts)
{
    int x = opts.get<int>("first");
    int y = opts.get<int>("second");
    s_sub_result = x - y;
    return 0;
}

static std::vector<std::string> s_captured_fields;

static int annotate_fn(ArgumentOptions &opts)
{
    s_captured_fields = opts.get<std::vector<std::string>>("field");
    return 0;
}

TEST(SubCommand, EndOfOptsMarker)
{
    // "--" should end option processing; remaining tokens go to positionals.
    static std::string captured_input;
    static std::string captured_output;
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run",
        [](ArgumentOptions &opts) -> int {
            captured_input  = opts.get<std::string>("input");
            captured_output = opts.get<std::string>("output");
            return 0;
        });
    p.add_positional<std::string>("input",  "input");
    p.add_positional<std::string>("output", "output");

    Args a({"tool", "run", "--", "--not-an-opt", "also-positional"});
    cmd.parse_args(a.argc(), a.argv());
    cmd.execute();
    EXPECT_EQ(captured_input,  "--not-an-opt");
    EXPECT_EQ(captured_output, "also-positional");
}

TEST(SubCommand, SingleDashIsPositional)
{
    std::string captured;
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [&captured](ArgumentOptions &args) {
        captured = args.get<std::string>("input");
        return 0;
    });
    p.add_positional<std::string>("input", "input");
    Args a({"tool", "run", "-"});
    cmd.parse_args(a.argc(), a.argv());
    EXPECT_EQ(cmd.execute(), 0);
    EXPECT_EQ(captured, "-");
}

TEST(SubCommand, VariadicPositional)
{
    static std::vector<std::string> captured_files;
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("process", "process files",
        [](ArgumentOptions &opts) -> int {
            captured_files = opts.get<std::vector<std::string>>("files");
            return 0;
        });
    p.add_positional_variadic<std::string>("files", "input files");

    Args a({"tool", "process", "a.txt", "b.txt", "c.txt"});
    cmd.parse_args(a.argc(), a.argv());
    cmd.execute();
    ASSERT_EQ(captured_files.size(), static_cast<size_t>(3));
    EXPECT_EQ(captured_files[0], "a.txt");
    EXPECT_EQ(captured_files[1], "b.txt");
    EXPECT_EQ(captured_files[2], "c.txt");
}

TEST(SubCommand, OptionalNoDefault)
{
    // add_optional without a default value must compile and not throw.
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run",
        [](ArgumentOptions &opts) -> int {
            opts.get<std::string>("host");
            return 0;
        });
    EXPECT_NO_THROW(p.add_optional<std::string>('H', "host", "host address"));
}

TEST(SubCommand, VersionArgumentCanBeRegistered)
{
    SubCommandParser cmd("tool", "tool");
    cmd.set_version("2.3.1");
    EXPECT_NO_THROW(cmd.add_version());
    EXPECT_EQ(cmd.get_version(), "2.3.1");
}

TEST(SubCommand, MainHelpListsHelpWithoutVersion)
{
    SubCommandParser cmd("tool", "tool");
    Args a({"tool", "--help"});
    testing::internal::CaptureStdout();
    cmd.parse_args(a.argc(), a.argv());
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("--help"), std::string::npos);
}

TEST(SubCommand, VersionRejectsInvalidNames)
{
    SubCommandParser cmd("tool", "tool");
    EXPECT_THROW(cmd.add_version('-', "version"), std::runtime_error);
    EXPECT_THROW(cmd.add_version('V', "--version"), std::runtime_error);
    EXPECT_THROW(cmd.add_version('V', "bad=name"), std::runtime_error);
    EXPECT_THROW(cmd.add_version('h', "version"), std::runtime_error);
    EXPECT_THROW(cmd.add_version('V', "help"), std::runtime_error);
}

TEST(SubCommand, HelpReturnsWithoutExecutingCommand)
{
    bool called = false;
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [&called](ArgumentOptions &) {
        called = true;
        return 0;
    });
    p.add_positional_variadic<std::string>("files", "input files");
    p.add_help();
    Args a({"tool", "run", "--help"});
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(cmd.parse_args(a.argc(), a.argv()));
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Usage:"), std::string::npos);
    EXPECT_NE(output.find("Positional arguments:"), std::string::npos);
    EXPECT_NE(output.find("files..."), std::string::npos);
    EXPECT_EQ(cmd.execute(), 0);
    EXPECT_FALSE(called);
}

TEST(SubCommand, BasicInvocation)
{
    s_sub_result = -9999;
    SubCommandParser cmd("calc", "calculator");
    ArgumentOptions &add_p = cmd.add_subcommand("add", "add two numbers", sub_add_fn);
    add_p.add_required<int>('a', "first",  "first number");
    add_p.add_required<int>('b', "second", "second number");
    Args a({"calc", "add", "-a", "7", "-b", "3"});
    cmd.parse_args(a.argc(), a.argv());
    cmd.execute();
    EXPECT_EQ(s_sub_result, 10);
}

TEST(SubCommand, UnknownSubcommandThrows)
{
    SubCommandParser cmd("calc", "calculator");
    cmd.add_subcommand("add", "add", sub_add_fn);
    Args a({"calc", "nonexistent"});
    try
    {
        cmd.parse_args(a.argc(), a.argv());
        FAIL() << "Expected unknown subcommand failure";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("Available commands: add"), std::string::npos);
    }
}

TEST(SubCommand, MultipleSubcommands)
{
    s_sub_result = -9999;
    SubCommandParser cmd("calc", "calculator");
    ArgumentOptions &add_p = cmd.add_subcommand("add", "add", sub_add_fn);
    add_p.add_required<int>('a', "first",  "first");
    add_p.add_required<int>('b', "second", "second");
    ArgumentOptions &sub_p = cmd.add_subcommand("subtract", "subtract", sub_subtract_fn);
    sub_p.add_required<int>('a', "first",  "first");
    sub_p.add_required<int>('b', "second", "second");

    // Test add
    {
        SubCommandParser cmd2("calc", "calculator");
        ArgumentOptions &p2 = cmd2.add_subcommand("add", "add", sub_add_fn);
        p2.add_required<int>('a', "first",  "first");
        p2.add_required<int>('b', "second", "second");
        Args a({"calc", "add", "-a", "10", "-b", "4"});
        cmd2.parse_args(a.argc(), a.argv());
        cmd2.execute();
        EXPECT_EQ(s_sub_result, 14);
    }

    // Test subtract
    {
        SubCommandParser cmd3("calc", "calculator");
        ArgumentOptions &p3 = cmd3.add_subcommand("subtract", "subtract", sub_subtract_fn);
        p3.add_required<int>('a', "first",  "first");
        p3.add_required<int>('b', "second", "second");
        Args a({"calc", "subtract", "-a", "10", "-b", "4"});
        cmd3.parse_args(a.argc(), a.argv());
        cmd3.execute();
        EXPECT_EQ(s_sub_result, 6);
    }
}

TEST(SubCommand, SubcommandRequiredArgMissing)
{
    SubCommandParser cmd("calc", "calculator");
    ArgumentOptions &add_p = cmd.add_subcommand("add", "add two numbers", sub_add_fn);
    add_p.add_required<int>('a', "first",  "first number");
    add_p.add_required<int>('b', "second", "second number");
    // Only provide one required argument → should throw on missing required
    Args a({"calc", "add", "-a", "5", "extra_token"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommand, RepeatableArgInSubcommand)
{
    s_captured_fields.clear();
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &ann = cmd.add_subcommand("annotate", "annotate", annotate_fn);
    ann.add_required<std::string>('i', "input",  "input");
    ann.add_required<std::string>('o', "output", "output");
    ann.add_repeatable<std::string>('f', "field", "field spec");

    Args a({"tool", "annotate", "-i", "in.vcf", "-o", "out.vcf",
            "-f", "db1:bed:col1", "-f", "db2:vcf:col2"});
    cmd.parse_args(a.argc(), a.argv());
    cmd.execute();
    ASSERT_EQ(s_captured_fields.size(), static_cast<size_t>(2));
    EXPECT_EQ(s_captured_fields[0], "db1:bed:col1");
    EXPECT_EQ(s_captured_fields[1], "db2:vcf:col2");
}

TEST(SubCommandEdgeCases, EmptyInlineValueIsPreserved)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<std::string>('n', "name", "name", "default");
    Args a({"tool", "run", "--name="});
    cmd.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "");
}

TEST(SubCommandEdgeCases, FlagWithInlineValueThrows)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_flag('v', "verbose", "verbose");
    Args a({"tool", "run", "--verbose=false"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandEdgeCases, ExtraPositionalThrows)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_positional<std::string>("input", "input");
    Args a({"tool", "run", "one", "two"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandEdgeCases, NumericTrailingCharactersThrow)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('n', "number", "number", 0);
    Args a({"tool", "run", "--number", "123abc"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandEdgeCases, ConversionErrorContainsArgumentContext)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('n', "number", "number", 0);
    Args a({"tool", "run", "--number", "abc"});
    try
    {
        cmd.parse_args(a.argc(), a.argv());
        FAIL() << "Expected parse failure";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_EQ(message, "Invalid value 'abc' for argument '--number': expected int.");
    }
}

TEST(SubCommandEdgeCases, MissingNumericValueBeforeRegisteredOption)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('p', "port", "port", 80);
    p.add_flag('v', "verbose", "verbose");
    Args a({"tool", "run", "--port", "--verbose"});
    try
    {
        cmd.parse_args(a.argc(), a.argv());
        FAIL() << "Expected missing value failure";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_EQ(std::string(error.what()), "Argument '--port' requires a value before '--verbose'.");
    }
}

TEST(SubCommandEdgeCases, BoolMixedCaseVariants)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<bool>('f', "flag", "flag", false);
    Args a({"tool", "run", "--flag", "yEs"});
    cmd.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("flag"));
}

TEST(SubCommandEdgeCases, HelpLikeValueIsNotSpecial)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<std::string>('o', "output", "output", "default");
    p.add_help();
    Args a({"tool", "run", "--output", "--help"});
    cmd.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("output"), "--help");
}

TEST(SubCommandRegistration, RejectsInvalidOptionNames)
{
    ArgumentOptions p;
    EXPECT_THROW(p.add_flag('-', "bad", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("bad=name", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("--bad", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("bad name", "bad"), std::runtime_error);
}

TEST(SubCommandRegistration, RejectsInvalidCommandNames)
{
    SubCommandParser cmd("tool", "tool");
    const auto callback = [](ArgumentOptions &) { return 0; };
    EXPECT_THROW(cmd.add_subcommand("", "empty", callback), std::runtime_error);
    EXPECT_THROW(cmd.add_subcommand("--run", "bad", callback), std::runtime_error);
    EXPECT_THROW(cmd.add_subcommand("bad=name", "bad", callback), std::runtime_error);
    EXPECT_THROW(cmd.add_subcommand("bad name", "bad", callback), std::runtime_error);
    EXPECT_THROW(cmd.add_subcommand("help", "reserved", callback), std::runtime_error);
}

TEST(SubCommandRegistration, CannotRegisterAfterVariadic)
{
    ArgumentOptions p;
    p.add_positional_variadic<std::string>("files", "files");
    EXPECT_THROW(p.add_positional<std::string>("output", "output"), std::runtime_error);
}

TEST(SubCommandRegistration, RequiredPositional)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_required_positional<std::string>("input", "input");
    Args valid({"tool", "run", "file.txt"});
    EXPECT_NO_THROW(cmd.parse_args(valid.argc(), valid.argv()));
    EXPECT_EQ(p.get<std::string>("input"), "file.txt");
}

TEST(SubCommandRegistration, RequiredCannotFollowOptional)
{
    ArgumentOptions p;
    p.add_positional<std::string>("output", "output");
    EXPECT_THROW(p.add_required_positional<std::string>("input", "input"), std::runtime_error);
}

TEST(SubCommandParse, RepeatedParseResetsValues)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('n', "number", "number", 7);
    p.add_flag('v', "verbose", "verbose");
    p.add_repeatable<std::string>('t', "tag", "tag");

    Args first({"tool", "run", "--number", "9", "-v", "-t", "one"});
    cmd.parse_args(first.argc(), first.argv());
    EXPECT_EQ(p.get<int>("number"), 9);
    EXPECT_TRUE(p.get<bool>("verbose"));

    Args second({"tool", "run", "-t", "two"});
    cmd.parse_args(second.argc(), second.argv());
    EXPECT_EQ(p.get<int>("number"), 7);
    EXPECT_FALSE(p.get<bool>("verbose"));
    EXPECT_EQ(p.get<std::vector<std::string>>("tag"), std::vector<std::string>({"two"}));
}

TEST(SubCommandParse, RejectsInvalidArgv)
{
    SubCommandParser cmd("tool", "tool");
    EXPECT_THROW(cmd.parse_args(0, nullptr), std::runtime_error);
}

TEST(SubCommandParse, UnsignedRejectsNegativeValue)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<unsigned int>('n', "number", "number", 0U);
    Args a({"tool", "run", "--number=-1"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandParse, DuplicateOptionThrows)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('p', "port", "port", 80);
    Args a({"tool", "run", "--port", "8000", "--port", "9000"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandParse, DuplicateAcrossShortAndLongThrows)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_optional<int>('p', "port", "port", 80);
    Args a({"tool", "run", "-p", "8000", "--port", "9000"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(SubCommandParse, DuplicateFlagThrows)
{
    SubCommandParser cmd("tool", "tool");
    ArgumentOptions &p = cmd.add_subcommand("run", "run", [](ArgumentOptions &) { return 0; });
    p.add_flag('v', "verbose", "verbose");
    Args a({"tool", "run", "-v", "--verbose"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

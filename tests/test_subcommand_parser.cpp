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
    Args a({"calc", "nonexistent"});
    EXPECT_THROW(cmd.parse_args(a.argc(), a.argv()), std::runtime_error);
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

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include "ArgumentParser.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

// ---- Helper: build argc/argv from an initializer list -------------------
// Store strings so pointers remain valid for the lifetime of Args.
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
// Long / short option parsing
// ========================================================================

TEST(LongOpt, SpaceSeparated)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string>('n', "name", "name", std::string("default"));
    Args a({"prog", "--name", "Alice"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "Alice");
}

TEST(LongOpt, EqualsSeparated)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog", "--port=9090"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 9090);
}

TEST(ShortOpt, SpaceSeparated)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string>('n', "name", "name", std::string("default"));
    Args a({"prog", "-n", "Bob"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "Bob");
}

TEST(ShortOpt, NoSpace)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 0);
    Args a({"prog", "-p9000"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 9000);
}

TEST(ShortOpt, NegativeIntValue)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('a', "alpha", "a number", 0);
    Args a({"prog", "-a", "-1"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("alpha"), -1);
}

TEST(ShortOpt, NegativeDoubleValue)
{
    ArgumentParser p("prog", "test");
    p.add_optional<double>('x', "val", "a float", 0.0);
    Args a({"prog", "-x", "-3.14"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_DOUBLE_EQ(p.get<double>("val"), -3.14);
}

// ========================================================================
// Flags
// ========================================================================

TEST(Flag, SetViaLongOpt)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "--verbose"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("verbose"));
}

TEST(Flag, SetViaShortOpt)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "-v"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("verbose"));
}

TEST(Flag, DefaultsFalseWhenAbsent)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_FALSE(p.get<bool>("verbose"));
}

TEST(Flag, CombinedShortFlags)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    p.add_flag('x', "extra",   "extra");
    p.add_flag('q', "quiet",   "quiet");
    Args a({"prog", "-vxq"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("verbose"));
    EXPECT_TRUE(p.get<bool>("extra"));
    EXPECT_TRUE(p.get<bool>("quiet"));
}

// ========================================================================
// Positional arguments
// ========================================================================

TEST(Positional, BasicTwoArgs)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("input",  "input file");
    p.add_positional<std::string>("output", "output file");
    Args a({"prog", "in.txt", "out.txt"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("input"),  "in.txt");
    EXPECT_EQ(p.get<std::string>("output"), "out.txt");
}

TEST(Positional, Variadic)
{
    ArgumentParser p("prog", "test");
    p.add_positional_variadic<std::string>("files", "input files");
    Args a({"prog", "a.txt", "b.txt", "c.txt"});
    p.parse_args(a.argc(), a.argv());
    auto files = p.get<std::vector<std::string>>("files");
    ASSERT_EQ(files.size(), static_cast<size_t>(3));
    EXPECT_EQ(files[0], "a.txt");
    EXPECT_EQ(files[1], "b.txt");
    EXPECT_EQ(files[2], "c.txt");
}

// ========================================================================
// Optional / Required
// ========================================================================

TEST(Positional, VariadicDuplicateThrows)
{
    ArgumentParser p("prog", "test");
    p.add_positional_variadic<std::string>("files", "input files");
    EXPECT_THROW(
        p.add_positional_variadic<std::string>("more", "more files"),
        std::runtime_error);
}

TEST(Optional, DefaultUsedWhenAbsent)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 8080);
}

TEST(Required, MissingThrows)
{
    ArgumentParser p("prog", "test");
    p.add_required<int>("count", "count");
    // Extra token so argc >= 2 (avoids the print_help+exit shortcut).
    Args a({"prog", "notanopt"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Parse, UnknownLongOptThrows)
{
    ArgumentParser p("prog", "test");
    Args a({"prog", "--unknown"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

// ========================================================================
// Range constraint
// ========================================================================

TEST(Range, ValidValuePasses)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int, Range<int>>('a', "age", "age", Expected<int>(20), Range<int>{18, 99});
    Args a({"prog", "--age", "30"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("age"), 30);
}

TEST(Range, OutOfBoundsThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int, Range<int>>('a', "age", "age", Expected<int>(20), Range<int>{18, 99});
    Args a({"prog", "--age", "200"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Range, OpenBounds)
{
    Range<double> r(0.0, 1.0, BoundType::Open);
    EXPECT_FALSE(r.check(0.0));
    EXPECT_FALSE(r.check(1.0));
    EXPECT_TRUE(r.check(0.5));
}

TEST(Range, LeftOpenBounds)
{
    Range<double> r(0.0, 1.0, BoundType::LeftOpen);
    EXPECT_FALSE(r.check(0.0));
    EXPECT_TRUE(r.check(1.0));
    EXPECT_TRUE(r.check(0.5));
}

TEST(Range, RightOpenBounds)
{
    Range<double> r(0.0, 1.0, BoundType::RightOpen);
    EXPECT_TRUE(r.check(0.0));
    EXPECT_FALSE(r.check(1.0));
    EXPECT_TRUE(r.check(0.5));
}

// ========================================================================
// Choices constraint
// ========================================================================

TEST(Range, NoDefaultDoesNotThrowAtRegistration)
{
    // Registering a Range constraint with no default must not throw.
    EXPECT_NO_THROW((
        ArgumentParser("prog", "test").add_optional<int, Range<int>>(
            'a', "age", "age", Expected<int>(), Range<int>{18, 99})
    ));
}

TEST(Choices, NoDefaultDoesNotThrowAtRegistration)
{
    // Registering a Choices constraint with no default must not throw.
    EXPECT_NO_THROW((
        ArgumentParser("prog", "test").add_optional<std::string, Choices<std::string>>(
            'c', "color", "color",
            Expected<std::string>(),
            Choices<std::string>{{"red", "green", "blue"}})
    ));
}

TEST(Choices, ValidChoicePasses)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string, Choices<std::string>>(
        'c', "color", "color",
        Expected<std::string>(std::string("red")),
        Choices<std::string>{{"red", "green", "blue"}});
    Args a({"prog", "--color", "green"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("color"), "green");
}

TEST(Choices, InvalidChoiceThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string, Choices<std::string>>(
        'c', "color", "color",
        Expected<std::string>(std::string("red")),
        Choices<std::string>{{"red", "green", "blue"}});
    Args a({"prog", "--color", "purple"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Choices, InvalidDefaultThrowsAtRegistration)
{
    EXPECT_THROW(
        (ArgumentParser("prog", "test").add_optional<std::string, Choices<std::string>>(
            'c', "color", "color",
            Expected<std::string>(std::string("orange")),
            Choices<std::string>{{"red", "green", "blue"}})),
        std::runtime_error);
}

// ========================================================================
// Repeatable arguments
// ========================================================================

TEST(Repeatable, StringType)
{
    ArgumentParser p("prog", "test");
    p.add_repeatable<std::string>('f', "field", "field");
    Args a({"prog", "-f", "x", "-f", "y", "-f", "z"});
    p.parse_args(a.argc(), a.argv());
    auto fields = p.get<std::vector<std::string>>("field");
    ASSERT_EQ(fields.size(), static_cast<size_t>(3));
    EXPECT_EQ(fields[0], "x");
    EXPECT_EQ(fields[1], "y");
    EXPECT_EQ(fields[2], "z");
}

TEST(Repeatable, IntType)
{
    ArgumentParser p("prog", "test");
    p.add_repeatable<int>('n', "num", "number");
    Args a({"prog", "-n", "1", "-n", "2", "-n", "3"});
    p.parse_args(a.argc(), a.argv());
    auto nums = p.get<std::vector<int>>("num");
    ASSERT_EQ(nums.size(), static_cast<size_t>(3));
    EXPECT_EQ(nums[0], 1);
    EXPECT_EQ(nums[1], 2);
    EXPECT_EQ(nums[2], 3);
}

// ========================================================================
// was_set()
// ========================================================================

TEST(WasSet, TrueWhenProvided)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog", "--port", "9090"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.was_set("port"));
    EXPECT_TRUE(p.was_set('p'));
}

TEST(WasSet, FalseWhenAbsent)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_FALSE(p.was_set("port"));
    EXPECT_FALSE(p.was_set('p'));
}

// ========================================================================
// Deprecation
// ========================================================================

TEST(Deprecate, UsageThrows)
{
    ArgumentParser p("prog", "test");
    p.add_flag('o', "old-flag", "old flag");
    p.deprecate("old-flag");
    Args a({"prog", "--old-flag"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Deprecate, NonExistentReturnsFalse)
{
    ArgumentParser p("prog", "test");
    EXPECT_FALSE(p.deprecate("no-such-arg"));
}

// ========================================================================
// get<>() error handling
// ========================================================================

TEST(Get, TypeMismatchThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_THROW(p.get<std::string>("port"), std::runtime_error);
}

TEST(Get, UnknownNameThrows)
{
    ArgumentParser p("prog", "test");
    Args a({"prog"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_THROW(p.get<int>("nope"), std::runtime_error);
}

// ========================================================================
// Special token / edge cases
// ========================================================================

TEST(Parse, EndOfOptsMarker)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("first",  "first");
    p.add_positional<std::string>("second", "second");
    Args a({"prog", "--", "--not-an-opt", "also-positional"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("first"),  "--not-an-opt");
    EXPECT_EQ(p.get<std::string>("second"), "also-positional");
}

TEST(Parse, NegativeDoubleAsPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<double>("value", "a numeric value");
    Args a({"prog", "-3.14"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_NEAR(p.get<double>("value"), -3.14, 1e-9);
}

TEST(Parse, NegativeIntAsPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<int>("value", "an integer");
    Args a({"prog", "-42"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("value"), -42);
}

// ========================================================================
// BoolParse: test via ArgumentParser public API
// ========================================================================

TEST(BoolParse, TrueVariants)
{
    for (const char *v : {"true", "True", "TRUE", "1"})
    {
        ArgumentParser p("prog", "");
        p.add_optional<bool>('f', "flag", "flag", false);
        Args a({"prog", "--flag", v});
        p.parse_args(a.argc(), a.argv());
        EXPECT_TRUE(p.get<bool>("flag")) << "failed for: " << v;
    }
}

TEST(BoolParse, FalseVariants)
{
    for (const char *v : {"false", "False", "FALSE", "0"})
    {
        ArgumentParser p("prog", "");
        p.add_optional<bool>('f', "flag", "flag", true);
        Args a({"prog", "--flag", v});
        p.parse_args(a.argc(), a.argv());
        EXPECT_FALSE(p.get<bool>("flag")) << "failed for: " << v;
    }
}

TEST(BoolParse, InvalidStringThrows)
{
    ArgumentParser p("prog", "");
    p.add_optional<bool>('f', "flag", "flag", false);
    Args a({"prog", "--flag", "yes"});
    EXPECT_THROW(p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

// ========================================================================
// Type-specific parsing
// ========================================================================

TEST(Double, ParsePrecision)
{
    ArgumentParser p("prog", "test");
    p.add_optional<double>('d', "discount", "discount", 0.5);
    Args a({"prog", "--discount", "0.75"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_NEAR(p.get<double>("discount"), 0.75, 1e-9);
}

TEST(FileType, PathStored)
{
    ArgumentParser p("prog", "test");
    p.add_optional<File>('i', "input", "input file", File("default.txt"));
    Args a({"prog", "--input", "myfile.vcf"});
    p.parse_args(a.argc(), a.argv());
    File f = p.get<File>("input");
    EXPECT_EQ(std::string(f), "myfile.vcf");
}

// ========================================================================
// Metadata
// ========================================================================

TEST(Metadata, Version)
{
    ArgumentParser p("prog", "test");
    p.set_version("2.3.1");
    EXPECT_EQ(p.get_version(), "2.3.1");
}

TEST(Metadata, Description)
{
    ArgumentParser p("prog", "initial");
    p.set_description("updated");
    EXPECT_EQ(p.get_description(), "updated");
}

// ========================================================================
// exists()
// ========================================================================

TEST(Exists, LongAndShortOpt)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    EXPECT_TRUE(p.exists("verbose"));
    EXPECT_FALSE(p.exists("nonexistent"));
    EXPECT_TRUE(p.exists('v'));
    EXPECT_FALSE(p.exists('z'));
}

// ========================================================================
// Mixed scenarios
// ========================================================================

TEST(Mixed, OptsAndPositionals)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    p.add_flag('v', "verbose", "verbose");
    p.add_positional<std::string>("input", "input");
    Args a({"prog", "--port=3000", "data.txt", "-v"});
    p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 3000);
    EXPECT_TRUE(p.get<bool>("verbose"));
    EXPECT_EQ(p.get<std::string>("input"), "data.txt");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

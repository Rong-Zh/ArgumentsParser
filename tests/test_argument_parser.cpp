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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "Alice");
}

TEST(LongOpt, EqualsSeparated)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog", "--port=9090"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 9090);
}

TEST(ShortOpt, SpaceSeparated)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string>('n', "name", "name", std::string("default"));
    Args a({"prog", "-n", "Bob"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "Bob");
}

TEST(ShortOpt, NoSpace)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 0);
    Args a({"prog", "-p9000"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 9000);
}

TEST(ShortOpt, NegativeIntValue)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('a', "alpha", "a number", 0);
    Args a({"prog", "-a", "-1"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("alpha"), -1);
}

TEST(ShortOpt, NegativeDoubleValue)
{
    ArgumentParser p("prog", "test");
    p.add_optional<double>('x', "val", "a float", 0.0);
    Args a({"prog", "-x", "-3.14"});
    (void)p.parse_args(a.argc(), a.argv());
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("verbose"));
}

TEST(Flag, SetViaShortOpt)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "-v"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.get<bool>("verbose"));
}

TEST(Flag, DefaultsFalseWhenAbsent)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_FALSE(p.get<bool>("verbose"));
}

TEST(Flag, CombinedShortFlags)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    p.add_flag('x', "extra",   "extra");
    p.add_flag('q', "quiet",   "quiet");
    Args a({"prog", "-vxq"});
    (void)p.parse_args(a.argc(), a.argv());
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("input"),  "in.txt");
    EXPECT_EQ(p.get<std::string>("output"), "out.txt");
}

TEST(Positional, Variadic)
{
    ArgumentParser p("prog", "test");
    p.add_positional_variadic<std::string>("files", "input files");
    Args a({"prog", "a.txt", "b.txt", "c.txt"});
    (void)p.parse_args(a.argc(), a.argv());
    auto files = p.get<std::vector<std::string>>("files");
    ASSERT_EQ(files.size(), static_cast<size_t>(3));
    EXPECT_EQ(files[0], "a.txt");
    EXPECT_EQ(files[1], "b.txt");
    EXPECT_EQ(files[2], "c.txt");
}

TEST(Positional, RequiredVariadic)
{
    ArgumentParser p("prog", "test");
    p.add_positional_variadic<std::string>("files", "files", true);

    Args missing({"prog"});
    testing::internal::CaptureStdout();
    EXPECT_THROW((void)p.parse_args(missing.argc(), missing.argv()), std::runtime_error);
    EXPECT_EQ(testing::internal::GetCapturedStdout().find("Usage:"), std::string::npos);

    Args present({"prog", "one.txt", "two.txt"});
    EXPECT_NO_THROW((void)p.parse_args(present.argc(), present.argv()));
    EXPECT_EQ(p.get<std::vector<std::string>>("files"),
              std::vector<std::string>({"one.txt", "two.txt"}));
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 8080);
}

TEST(Required, MissingThrowsWithoutPrintingHelp)
{
    ArgumentParser p("prog", "test");
    p.add_required<int>("count", "count");
    p.add_help();
    Args a({"prog"});
    testing::internal::CaptureStdout();
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output.find("Usage:"), std::string::npos);
    EXPECT_EQ(output.find("--count"), std::string::npos);
}

TEST(Required, GenericOptionUsesIndependentRequiredFlag)
{
    ArgumentParser p("prog", "test");
    p.add_argument<int>('n', "number", ArgType::Option, "number", Expected<int>(), true);
    p.add_flag('v', "verbose", "verbose");

    Args missing({"prog", "--verbose"});
    EXPECT_THROW((void)p.parse_args(missing.argc(), missing.argv()), std::runtime_error);

    Args present({"prog", "--number", "42"});
    EXPECT_NO_THROW((void)p.parse_args(present.argc(), present.argv()));
    EXPECT_EQ(p.get<int>("number"), 42);
}

TEST(Parse, UnknownLongOptThrows)
{
    ArgumentParser p("prog", "test");
    Args a({"prog", "--unknown"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

// ========================================================================
// Range constraint
// ========================================================================

TEST(Range, ValidValuePasses)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int, Range<int>>('a', "age", "age", Expected<int>(20), Range<int>{18, 99});
    Args a({"prog", "--age", "30"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("age"), 30);
}

TEST(Range, OutOfBoundsThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int, Range<int>>('a', "age", "age", Expected<int>(20), Range<int>{18, 99});
    Args a({"prog", "--age", "200"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
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
    (void)p.parse_args(a.argc(), a.argv());
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
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
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
    (void)p.parse_args(a.argc(), a.argv());
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
    (void)p.parse_args(a.argc(), a.argv());
    auto nums = p.get<std::vector<int>>("num");
    ASSERT_EQ(nums.size(), static_cast<size_t>(3));
    EXPECT_EQ(nums[0], 1);
    EXPECT_EQ(nums[1], 2);
    EXPECT_EQ(nums[2], 3);
}

TEST(Repeatable, Required)
{
    ArgumentParser p("prog", "test");
    p.add_repeatable<std::string>('t', "tag", "tag", true);

    Args missing({"prog"});
    testing::internal::CaptureStdout();
    EXPECT_THROW((void)p.parse_args(missing.argc(), missing.argv()), std::runtime_error);
    EXPECT_EQ(testing::internal::GetCapturedStdout().find("Usage:"), std::string::npos);

    Args present({"prog", "--tag", "one", "--tag", "two"});
    EXPECT_NO_THROW((void)p.parse_args(present.argc(), present.argv()));
    EXPECT_EQ(p.get<std::vector<std::string>>("tag"),
              std::vector<std::string>({"one", "two"}));
}

// ========================================================================
// was_set()
// ========================================================================

TEST(WasSet, TrueWhenProvided)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog", "--port", "9090"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_TRUE(p.was_set("port"));
    EXPECT_TRUE(p.was_set('p'));
}

TEST(WasSet, FalseWhenAbsent)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 8080);
    Args a({"prog"});
    (void)p.parse_args(a.argc(), a.argv());
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
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_THROW(p.get<std::string>("port"), std::runtime_error);
}

TEST(Get, UnknownNameThrows)
{
    ArgumentParser p("prog", "test");
    Args a({"prog"});
    (void)p.parse_args(a.argc(), a.argv());
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("first"),  "--not-an-opt");
    EXPECT_EQ(p.get<std::string>("second"), "also-positional");
}

TEST(Parse, SingleDashIsPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("input", "input");
    Args a({"prog", "-"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("input"), "-");
}

TEST(Parse, NegativeDoubleAsPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<double>("value", "a numeric value");
    Args a({"prog", "-3.14"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_NEAR(p.get<double>("value"), -3.14, 1e-9);
}

TEST(Parse, NegativeIntAsPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<int>("value", "an integer");
    Args a({"prog", "-42"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("value"), -42);
}

// ========================================================================
// BoolParse: test via ArgumentParser public API
// ========================================================================

TEST(BoolParse, TrueVariants)
{
    for (const char *v : {"true", "True", "TRUE", "yes", "YES", "on", "ON", "1"})
    {
        ArgumentParser p("prog", "");
        p.add_optional<bool>('f', "flag", "flag", false);
        Args a({"prog", "--flag", v});
        (void)p.parse_args(a.argc(), a.argv());
        EXPECT_TRUE(p.get<bool>("flag")) << "failed for: " << v;
    }
}

TEST(BoolParse, FalseVariants)
{
    for (const char *v : {"false", "False", "FALSE", "no", "NO", "off", "OFF", "0"})
    {
        ArgumentParser p("prog", "");
        p.add_optional<bool>('f', "flag", "flag", true);
        Args a({"prog", "--flag", v});
        (void)p.parse_args(a.argc(), a.argv());
        EXPECT_FALSE(p.get<bool>("flag")) << "failed for: " << v;
    }
}

TEST(BoolParse, InvalidStringThrows)
{
    ArgumentParser p("prog", "");
    p.add_optional<bool>('f', "flag", "flag", false);
    Args a({"prog", "--flag", "maybe"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(BoolParse, MixedCaseVariants)
{
    const std::vector<std::string> true_values = {"tRuE", "yEs", "oN"};
    for (const auto& value : true_values)
    {
        ArgumentParser p("prog", "test");
        p.add_optional<bool>('f', "flag", "flag", false);
        Args a({"prog", "--flag", value});
        (void)p.parse_args(a.argc(), a.argv());
        EXPECT_TRUE(p.get<bool>("flag"));
    }

    const std::vector<std::string> false_values = {"fAlSe", "nO", "oFf"};
    for (const auto& value : false_values)
    {
        ArgumentParser p("prog", "test");
        p.add_optional<bool>('f', "flag", "flag", true);
        Args a({"prog", "--flag", value});
        (void)p.parse_args(a.argc(), a.argv());
        EXPECT_FALSE(p.get<bool>("flag"));
    }
}

// ========================================================================
// Type-specific parsing
// ========================================================================

TEST(Double, ParsePrecision)
{
    ArgumentParser p("prog", "test");
    p.add_optional<double>('d', "discount", "discount", 0.5);
    Args a({"prog", "--discount", "0.75"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_NEAR(p.get<double>("discount"), 0.75, 1e-9);
}

TEST(FileType, PathStored)
{
    ArgumentParser p("prog", "test");
    p.add_optional<File>('i', "input", "input file", File("default.txt"));
    Args a({"prog", "--input", "myfile.vcf"});
    (void)p.parse_args(a.argc(), a.argv());
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

TEST(Metadata, VersionArgument)
{
    ArgumentParser p("prog", "test");
    p.set_version("2.3.1");
    p.add_version();

    EXPECT_TRUE(p.exists('V'));
    EXPECT_TRUE(p.exists("version"));
    EXPECT_EQ(p.get_version(), "2.3.1");
}

TEST(Metadata, HelpReturnsWithoutExiting)
{
    ArgumentParser p("prog", "test");
    p.add_required<int>("number", "number");
    p.add_positional<std::string>("input", "input file");
    p.add_help();
    Args a({"prog", "--help"});
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW((void)p.parse_args(a.argc(), a.argv()));
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Usage:"), std::string::npos);
    EXPECT_EQ(output.find("Version:"), std::string::npos);
    EXPECT_NE(output.find("Positional arguments:"), std::string::npos);
    EXPECT_NE(output.find("input file"), std::string::npos);
}

TEST(Metadata, VersionReturnsWithoutExiting)
{
    ArgumentParser p("prog", "test");
    p.set_version("2.3.1");
    p.add_version();
    Args a({"prog", "--version"});
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW((void)p.parse_args(a.argc(), a.argv()));
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("2.3.1"), std::string::npos);
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
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<int>("port"), 3000);
    EXPECT_TRUE(p.get<bool>("verbose"));
    EXPECT_EQ(p.get<std::string>("input"), "data.txt");
}

TEST(ParseEdgeCases, EmptyInlineValueIsPreserved)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string>('n', "name", "name", "default");
    Args a({"prog", "--name="});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("name"), "");
}

TEST(ParseEdgeCases, FlagWithInlineValueThrows)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "--verbose=false"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(ParseEdgeCases, ExtraPositionalThrows)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("input", "input");
    Args a({"prog", "one", "two"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(ParseEdgeCases, NumericTrailingCharactersThrow)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('n', "number", "number", 0);
    Args a({"prog", "--number", "123abc"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(ParseEdgeCases, ConversionErrorContainsArgumentContext)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('n', "number", "number", 0);
    Args a({"prog", "--number", "abc"});
    try
    {
        (void)p.parse_args(a.argc(), a.argv());
        FAIL() << "Expected parse failure";
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        EXPECT_EQ(message, "Invalid value 'abc' for argument '--number': expected int.");
    }
}

TEST(ParseEdgeCases, MissingNumericValueBeforeRegisteredOption)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 80);
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "--port", "--verbose"});
    try
    {
        (void)p.parse_args(a.argc(), a.argv());
        FAIL() << "Expected missing value failure";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_EQ(std::string(error.what()), "Argument '--port' requires a value before '--verbose'.");
    }
}

TEST(ParseEdgeCases, RangeErrorHasNaturalDescription)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int, Range<int>>('a', "age", "age", 20, Range<int>(18, 99));
    Args a({"prog", "--age", "200"});
    try
    {
        (void)p.parse_args(a.argc(), a.argv());
        FAIL() << "Expected range failure";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_EQ(std::string(error.what()),
                  "Invalid value '200' for argument '--age': expected int in range [18, 99].");
    }
}

TEST(ParseEdgeCases, HelpLikeValueIsNotSpecial)
{
    ArgumentParser p("prog", "test");
    p.add_optional<std::string>('o', "output", "output", "default");
    p.add_help();
    Args a({"prog", "--output", "--help"});
    (void)p.parse_args(a.argc(), a.argv());
    EXPECT_EQ(p.get<std::string>("output"), "--help");
}

TEST(Registration, RejectsInvalidOptionNames)
{
    ArgumentParser p("prog", "test");
    EXPECT_THROW(p.add_flag('-', "bad", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("bad=name", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("--bad", "bad"), std::runtime_error);
    EXPECT_THROW(p.add_flag("bad name", "bad"), std::runtime_error);
}

TEST(Registration, RequiredFlagIsRejected)
{
    ArgumentParser p("prog", "test");
    EXPECT_THROW(
        p.add_argument<bool>('v', "verbose", ArgType::Flag, "verbose", Expected<bool>(), true),
        std::runtime_error);
}


TEST(Positional, CannotRegisterAfterVariadic)
{
    ArgumentParser p("prog", "test");
    p.add_positional_variadic<std::string>("files", "files");
    EXPECT_THROW(p.add_positional<std::string>("output", "output"), std::runtime_error);
}

TEST(Positional, RequiredPositional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("input", "input", true);
    Args valid({"prog", "file.txt"});
    (void)p.parse_args(valid.argc(), valid.argv());
    EXPECT_EQ(p.get<std::string>("input"), "file.txt");

    ArgumentParser missing("prog", "test");
    missing.add_positional<std::string>("input", "input", true);
    Args absent({"prog", "--"});
    EXPECT_THROW((void)missing.parse_args(absent.argc(), absent.argv()), std::runtime_error);
}

TEST(Positional, RequiredCannotFollowOptional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("output", "output");
    EXPECT_THROW(p.add_positional<std::string>("input", "input", true), std::runtime_error);
}

TEST(Positional, HelpDistinguishesRequiredAndOptional)
{
    ArgumentParser p("prog", "test");
    p.add_positional<std::string>("input", "input", true);
    p.add_positional<std::string>("output", "output");
    testing::internal::CaptureStdout();
    p.print_help();
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("<input> [output]"), std::string::npos);
}

TEST(Parse, RepeatedParseResetsValues)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('n', "number", "number", 7);
    p.add_flag('v', "verbose", "verbose");
    p.add_repeatable<std::string>('t', "tag", "tag");

    Args first({"prog", "--number", "9", "-v", "-t", "one"});
    (void)p.parse_args(first.argc(), first.argv());
    EXPECT_EQ(p.get<int>("number"), 9);
    EXPECT_TRUE(p.get<bool>("verbose"));

    Args second({"prog", "-t", "two"});
    (void)p.parse_args(second.argc(), second.argv());
    EXPECT_EQ(p.get<int>("number"), 7);
    EXPECT_FALSE(p.get<bool>("verbose"));
    EXPECT_EQ(p.get<std::vector<std::string>>("tag"), std::vector<std::string>({"two"}));
}

TEST(Parse, RejectsInvalidArgv)
{
    ArgumentParser p("prog", "test");
    EXPECT_THROW((void)p.parse_args(0, nullptr), std::runtime_error);
}

TEST(Parse, UnsignedRejectsNegativeValue)
{
    ArgumentParser p("prog", "test");
    p.add_optional<unsigned int>('n', "number", "number", 0U);
    Args a({"prog", "--number=-1"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Parse, IntegerOverflowThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('n', "number", "number", 0);
    Args a({"prog", "--number=999999999999999999999999"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Parse, DuplicateOptionThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 80);
    Args a({"prog", "--port", "8000", "--port", "9000"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Parse, DuplicateAcrossShortAndLongThrows)
{
    ArgumentParser p("prog", "test");
    p.add_optional<int>('p', "port", "port", 80);
    Args a({"prog", "-p", "8000", "--port", "9000"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

TEST(Parse, DuplicateFlagThrows)
{
    ArgumentParser p("prog", "test");
    p.add_flag('v', "verbose", "verbose");
    Args a({"prog", "-v", "--verbose"});
    EXPECT_THROW((void)p.parse_args(a.argc(), a.argv()), std::runtime_error);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

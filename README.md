# ArgumentsParser

A lightweight, header-only C++11 command-line argument parsing library with support for both simple argument parsing and complex subcommand structures.

## Features

### Core Features
- ✅ **Short options**: `-a`, `-p 10`, `-p10`
- ✅ **Combined short flags**: `-abc` (multiple boolean flags)
- ✅ **Long options**: `--param=10`, `--param 10`
- ✅ **Positional arguments**: Order-based unnamed arguments
- ✅ **Flag arguments**: Boolean switches (no value required)
- ✅ **Required/Optional arguments**: Enforce or suggest parameters
- ✅ **Default values**: Fallback values for optional arguments
- ✅ **Type safety**: Template-based type checking
- ✅ **Subcommands**: Git-style subcommand support (e.g., `git add`, `git commit`)
- ✅ **Automatic help generation**: `-h/--help` with formatted output
- ✅ **Argument validation**: Range and choice constraints
- ✅ **Deprecation support**: Mark arguments as deprecated
- ✅ **Custom types**: File type with existence checking
- ✅ **Repeatable arguments**: Collect multiple values into `std::vector<T>`
- ✅ **C++11 compatible**: No external dependencies

## Requirements

- C++11 or later
- Standard library only (no external dependencies)

## Installation

This is a header-only library. Simply include the headers in your project:

```cpp
#include "ArgumentParser.h"      // For simple argument parsing
#include "SubCommandParser.h"    // For subcommand support
```

## Quick Start

### Example 1: Simple Argument Parsing

```cpp
#include "ArgumentParser.h"
#include <iostream>

int main(int argc, char *argv[])
{
    ArgumentParser parser("myapp", "My application description");
    parser.set_version("1.0.0");
    parser.add_version('V', "version", "Show version information");

    // Add required argument
    parser.add_required<int>("number", "An integer number");

    // Add optional argument with default value
    parser.add_optional<std::string>('n', "name", "Your name", "World");

    // Add flag
    parser.add_flag('v', "verbose", "Enable verbose output");

    // Add positional arguments
    parser.add_positional<std::string>("input", "Input file");
    parser.add_positional<std::string>("output", "Output file");
    // Add help
    parser.add_help('h', "help", "Show help message");
    
    parser.parse_args(argc, argv);
    int number = parser.get<int>("number");
    std::string name = parser.get<std::string>("name");
    bool verbose = parser.get<bool>("verbose");
    std::string input = parser.get<std::string>("input");

    std::cout << "Hello, " << name << "!" << std::endl;
    std::cout << "Number: " << number << std::endl;
    if (verbose) {
        std::cout << "Verbose mode enabled" << std::endl;
    }
    return EXIT_SUCCESS;
}
```

**Usage:**
```bash
myapp --number 42 --name Alice -v input.txt output.txt
myapp --number 42 input.txt output.txt          # Uses default name "World"
myapp -h                                         # Show help
```

### Example 2: Subcommand Support

```cpp
#include "SubCommandParser.h"
#include <iostream>

int main(int argc, char *argv[])
{
    SubCommandParser app("git-demo", "Git-like command demo");
    app.set_version("1.0.0");

    // Add "add" subcommand
    auto &add_parser = app.add_subcommand("add", "Add two numbers",
        [](ArgumentOptions &parser) -> int {
            int a = parser.get<int>("first");
            int b = parser.get<int>("second");
            std::cout << "Result: " << (a + b) << std::endl;
            return EXIT_SUCCESS;
        });
    add_parser.add_required<int>('a', "first", "First number");
    add_parser.add_required<int>('b', "second", "Second number");
    add_parser.add_help();

    // Add "multiply" subcommand
    auto &mul_parser = app.add_subcommand("multiply", "Multiply two numbers",
        [](ArgumentOptions &parser) -> int {
            int a = parser.get<int>("first");
            int b = parser.get<int>("second");
            std::cout << "Result: " << (a * b) << std::endl;
            return EXIT_SUCCESS;
        });
    mul_parser.add_required<int>('a', "first", "First number");
    mul_parser.add_required<int>('b', "second", "Second number");
    mul_parser.add_help();

    app.parse_args(argc, argv);
    
    return app.execute();  // Returns subcomman
}
```

**Usage:**
```bash
git-demo add -a 10 -b 20          # Output: Result: 30
git-demo multiply -a 5 -b 6       # Output: Result: 30
git-demo add -h                   # Show help for "add" subcommand
git-demo -h                       # Show main help with all subcommands
```

## Argument Types

### 1. Required Arguments
Must be provided by the user, or an error is thrown.

```cpp
parser.add_required<int>("count", "Number of items");
parser.add_required<std::string>('f', "file", "Input file path");
```

**Usage:** `myapp --count 10 --file input.txt`

### 2. Optional Arguments
Have default values, can be omitted.

```cpp
parser.add_optional<int>("port", "Port number", 8080);
parser.add_optional<std::string>('h', "host", "Host address", "localhost");
```

**Usage:** `myapp --port 9000` or `myapp` (uses default 8080)

### 3. Flag Arguments
Boolean switches, presence indicates `true`.

```cpp
parser.add_flag('v', "verbose", "Verbose output");
parser.add_flag("debug", "Enable debug mode");
```

**Usage:** `myapp -v` or `myapp --verbose` or `myapp -v --debug`

### 4. Positional Arguments
Order-based arguments without flags.

```cpp
parser.add_positional<std::string>("input", "Input file");
parser.add_positional<std::string>("output", "Output file");
```

**Usage:** `myapp input.txt output.txt`

### 5. Repeatable Arguments
Collect multiple values into a `std::vector<T>`. The template type parameter **must be specified explicitly**.

```cpp
// Collect multiple values into std::vector<std::string>
parser.add_repeatable<std::string>('f', "field", "Field name (repeatable)");
parser.add_repeatable<int>("count", "Count value (repeatable)");

// Retrieve as vector
auto fields = parser.get<std::vector<std::string>>("field");
auto counts = parser.get<std::vector<int>>("count");
```

**Usage:** `myapp -f name -f age -f score` → `fields = {"name", "age", "score"}`

> **Note:** `add_repeatable<T>` requires an explicit template argument — the compiler cannot deduce `T` from the string parameters.

## Advanced Features

### 1. Argument Constraints

#### Range Constraint
```cpp
parser.add_optional<int, Range<int>>(
    'a', "age", "Age", 25, 
    Range<int>(18, 65, BoundType::Closed)  // [18, 65]
);
```

**Bound Types:**
- `BoundType::Closed`: `[min, max]`
- `BoundType::Open`: `(min, max)`
- `BoundType::LeftOpen`: `(min, max]`
- `BoundType::RightOpen`: `[min, max)`

#### Choices Constraint
```cpp
parser.add_optional<std::string, Choices<std::string>>(
    'c', "color", "Color", "red",
    Choices<std::string>{{"red", "green", "blue"}}
);
```

### 2. File Type with Validation
```cpp
parser.add_optional<File>('i', "input", "Input file", File("default.txt"));

File input_file = parser.get<File>("input");
if (input_file.exists()) {
    std::cout << "File exists: " << std::string(input_file) << std::endl;
}
```

### 3. Argument Deprecation
```cpp
parser.add_flag('o', "old-flag", "Old flag (deprecated)");
parser.deprecate("old-flag");  // Mark as deprecated

// Using deprecated argument will throw an error
```

### 4. Custom Help and Footnotes
```cpp
parser.add_help('h', "help", "Show this help message");
parser.footnote("Maintainer: Your Name, Email: your@email.com");
```

## Supported Option Formats

### Short Options
```bash
-a              # Flag
-p 10           # Option with value (space-separated)
-p10            # Option with value (no space)
-abc            # Combined flags (only for flags)
-p10 -abc       # Mixed
```

### Long Options
```bash
--param=10      # Option with value (equals sign)
--param 10      # Option with value (space-separated)
--verbose       # Flag
```

## Supported Types

Built-in type support:
- `int`
- `float`
- `double`
- `bool` (`true/false`, `1/0`, `True/False`, `TRUE/FALSE`)
- `std::string`
- `File` (custom type with existence checking)
- `std::vector<T>` (any of the above as element type, used with `add_repeatable`)

## Help Output Example

```
myapp: Application description
Version: 1.0.0
Usage: myapp [options] <input> <output>

Required arguments:
  --number        int     An integer number

Optional arguments:
  -n, --name      string  Your name (default: World)
  -v, --verbose   bool    Enable verbose output
  -h, --help      bool    Show help message

Maintainer: Your Name, Email: your@email.com
```

## Best Practices

1. **Always add help**: Call `parser.add_help()` for user-friendly CLI
2. **Use exceptions**: Wrap `parse_args()` in try-catch
3. **Return exit codes**: Return `EXIT_SUCCESS` or `EXIT_FAILURE` from `main`
4. **Validate early**: Use constraints (Range, Choices) instead of manual validation
5. **Use subcommands**: For complex applications with multiple operations
6. **Type safety**: Specify template types explicitly: `parser.get<int>("count")`
7. **Repeatable type**: Always specify element type for `add_repeatable`: `add_repeatable<std::string>(...)`, not `add_repeatable(...)`

## Building Examples
```bash
g++ -std=c++11 -O2 -I. ./tests/example1.cpp -o example1
./example1
```

## Project Structure

```
ArgumentsParser/
├── ArgumentParser.h       # Simple argument parsing
├── SubCommandParser.h     # Subcommand support
├── tests/
│   ├── example1.cpp       # Basic usage example
│   └── example2.cpp       # Subcommand example
└── README.md              # This file
```

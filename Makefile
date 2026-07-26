CXX          := g++
CXXFLAGS     := -std=c++11 -Wall -Wextra -I.
CXXFLAGS_TEST:= -std=c++11 -Wall -Wextra -I.
GTEST_CFLAGS := $(shell pkg-config --cflags gtest)
GTEST_LIBS   := $(shell pkg-config --libs gtest) -lgtest_main -pthread

BUILD    := build
TESTS    := $(BUILD)/test_argument_parser $(BUILD)/test_subcommand_parser \
	$(BUILD)/test_output_stream $(BUILD)/test_include_both $(BUILD)/test_include_both_reverse
EXAMPLES := $(BUILD)/example1 $(BUILD)/example2

.PHONY: all examples test clean

all: $(TESTS) $(EXAMPLES)

examples: $(EXAMPLES)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_argument_parser: tests/test_argument_parser.cpp ArgumentParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS_TEST) $(GTEST_CFLAGS) -o $@ $< $(GTEST_LIBS)

$(BUILD)/test_subcommand_parser: tests/test_subcommand_parser.cpp SubCommandParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS_TEST) $(GTEST_CFLAGS) -o $@ $< $(GTEST_LIBS)

$(BUILD)/test_output_stream: tests/test_output_stream.cpp ArgumentParser.h SubCommandParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS_TEST) $(GTEST_CFLAGS) -o $@ $< $(GTEST_LIBS)

$(BUILD)/test_include_both: tests/test_include_both.cpp ArgumentParser.h SubCommandParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/test_include_both_reverse: tests/test_include_both_reverse.cpp ArgumentParser.h SubCommandParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/example1: tests/example1.cpp ArgumentParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/example2: tests/example2.cpp SubCommandParser.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(TESTS)
	@$(BUILD)/test_argument_parser
	@$(BUILD)/test_subcommand_parser
	@$(BUILD)/test_include_both
	@$(BUILD)/test_include_both_reverse

clean:
	rm -rf $(BUILD)

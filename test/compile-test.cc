// Formatting library for C++ - formatting library tests
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#include <string>
#include <type_traits>

// Check that fmt/compile.h compiles with windows.h included before it.
#ifdef _WIN32
#  include <windows.h>
#endif

#include "fmt/compile.h"
#include "gmock.h"
#include "gtest-extra.h"
#include "util.h"

// compiletime_prepared_parts_type_provider is useful only with relaxed
// constexpr.
#if FMT_USE_CONSTEXPR
template <unsigned EXPECTED_PARTS_COUNT, typename Format>
void check_prepared_parts_type(Format format) {
  typedef fmt::detail::compiled_format_base<decltype(format)> provider;
  typedef fmt::detail::format_part<char>
      expected_parts_type[EXPECTED_PARTS_COUNT];
  static_assert(std::is_same<typename provider::parts_container,
                             expected_parts_type>::value,
                "CompileTimePreparedPartsTypeProvider test failed");
}

TEST(CompileTest, CompileTimePreparedPartsTypeProvider) {
  check_prepared_parts_type<1u>(FMT_STRING("text"));
  check_prepared_parts_type<1u>(FMT_STRING("{}"));
  check_prepared_parts_type<2u>(FMT_STRING("text{}"));
  check_prepared_parts_type<2u>(FMT_STRING("{}text"));
  check_prepared_parts_type<3u>(FMT_STRING("text{}text"));
  check_prepared_parts_type<3u>(FMT_STRING("{:{}.{}} {:{}}"));

  check_prepared_parts_type<3u>(FMT_STRING("{{{}}}"));   // '{', 'argument', '}'
  check_prepared_parts_type<2u>(FMT_STRING("text{{"));   // 'text', '{'
  check_prepared_parts_type<3u>(FMT_STRING("text{{ "));  // 'text', '{', ' '
  check_prepared_parts_type<2u>(FMT_STRING("}}text"));   // '}', text
  check_prepared_parts_type<2u>(FMT_STRING("text}}text"));  // 'text}', 'text'
  check_prepared_parts_type<4u>(
      FMT_STRING("text{{}}text"));  // 'text', '{', '}', 'text'
}
#endif

TEST(CompileTest, PassStringLiteralFormat) {
  const auto prepared = fmt::detail::compile<int>("test {}");
  EXPECT_EQ("test 42", fmt::format(prepared, 42));
  const auto wprepared = fmt::detail::compile<int>(L"test {}");
  EXPECT_EQ(L"test 42", fmt::format(wprepared, 42));
}

TEST(CompileTest, FormatToArrayOfChars) {
  char buffer[32] = {0};
  const auto prepared = fmt::detail::compile<int>("4{}");
  fmt::format_to(fmt::detail::make_checked(buffer, 32), prepared, 2);
  EXPECT_EQ(std::string("42"), buffer);
  wchar_t wbuffer[32] = {0};
  const auto wprepared = fmt::detail::compile<int>(L"4{}");
  fmt::format_to(fmt::detail::make_checked(wbuffer, 32), wprepared, 2);
  EXPECT_EQ(std::wstring(L"42"), wbuffer);
}

TEST(CompileTest, FormatToIterator) {
  std::string s(2, ' ');
  const auto prepared = fmt::detail::compile<int>("4{}");
  fmt::format_to(s.begin(), prepared, 2);
  EXPECT_EQ("42", s);
  std::wstring ws(2, L' ');
  const auto wprepared = fmt::detail::compile<int>(L"4{}");
  fmt::format_to(ws.begin(), wprepared, 2);
  EXPECT_EQ(L"42", ws);
}

TEST(CompileTest, FormatToN) {
  char buf[5];
  auto f = fmt::detail::compile<int>("{:10}");
  auto result = fmt::format_to_n(buf, 5, f, 42);
  EXPECT_EQ(result.size, 10);
  EXPECT_EQ(result.out, buf + 5);
  EXPECT_EQ(fmt::string_view(buf, 5), "     ");
}

TEST(CompileTest, FormattedSize) {
  auto f = fmt::detail::compile<int>("{:10}");
  EXPECT_EQ(fmt::formatted_size(f, 42), 10);
}

TEST(CompileTest, MultipleTypes) {
  auto f = fmt::detail::compile<int, int>("{} {}");
  EXPECT_EQ(fmt::format(f, 42, 42), "42 42");
}

struct test_formattable {};

FMT_BEGIN_NAMESPACE
template <> struct formatter<test_formattable> : formatter<const char*> {
  template <typename FormatContext>
  auto format(test_formattable, FormatContext& ctx) -> decltype(ctx.out()) {
    return formatter<const char*>::format("foo", ctx);
  }
};
FMT_END_NAMESPACE

TEST(CompileTest, FormatUserDefinedType) {
  auto f = fmt::detail::compile<test_formattable>("{}");
  EXPECT_EQ(fmt::format(f, test_formattable()), "foo");
}

TEST(CompileTest, EmptyFormatString) {
  auto f = fmt::detail::compile<>("");
  EXPECT_EQ(fmt::format(f), "");
}

#ifdef __cpp_if_constexpr
TEST(CompileTest, FormatDefault) {
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), 42));
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), 42u));
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), 42ll));
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), 42ull));
  EXPECT_EQ("true", fmt::format(FMT_COMPILE("{}"), true));
  EXPECT_EQ("x", fmt::format(FMT_COMPILE("{}"), 'x'));
  EXPECT_EQ("4.2", fmt::format(FMT_COMPILE("{}"), 4.2));
  EXPECT_EQ("foo", fmt::format(FMT_COMPILE("{}"), "foo"));
  EXPECT_EQ("foo", fmt::format(FMT_COMPILE("{}"), std::string("foo")));
  EXPECT_EQ("foo", fmt::format(FMT_COMPILE("{}"), test_formattable()));
#  ifdef __cpp_lib_byte
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), std::byte{42}));
#  endif
}

TEST(CompileTest, FormatWideString) {
  EXPECT_EQ(L"42", fmt::format(FMT_COMPILE(L"{}"), 42));
}

struct test_custom_formattable {};

FMT_BEGIN_NAMESPACE
template <> struct formatter<test_custom_formattable> {
  enum class output_type { two, four } type{output_type::two};

  FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    while (it != end && *it != '}') {
      ++it;
    }
    auto spec = string_view(ctx.begin(), static_cast<size_t>(it - ctx.begin()));
    auto tag = string_view("custom");
    if (spec.size() == tag.size()) {
      bool is_same = true;
      for (size_t index = 0; index < spec.size(); ++index) {
        if (spec[index] != tag[index]) {
          is_same = false;
          break;
        }
      }
      type = is_same ? output_type::four : output_type::two;
    } else {
      type = output_type::two;
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const test_custom_formattable&, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return format_to(ctx.out(), type == output_type::two ? "{:>2}" : "{:>4}",
                     42);
  }
};
FMT_END_NAMESPACE

TEST(CompileTest, FormatSpecs) {
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{:x}"), 0x42));
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), test_custom_formattable()));
  EXPECT_EQ("  42",
            fmt::format(FMT_COMPILE("{:custom}"), test_custom_formattable()));
}

struct test_dynamic_formattable {};

FMT_BEGIN_NAMESPACE
template <> struct formatter<test_dynamic_formattable> {
  size_t amount = 0;
  detail::arg_ref<char> width_refs[3];

  FMT_CONSTEXPR auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    amount = static_cast<size_t>(*ctx.begin() - '0');
    if (amount >= 1) {
      width_refs[0] = detail::arg_ref<char>(ctx.next_arg_id());
    }
    if (amount >= 2) {
      width_refs[1] = detail::arg_ref<char>(ctx.next_arg_id());
    }
    if (amount >= 3) {
      width_refs[2] = detail::arg_ref<char>(ctx.next_arg_id());
    }
    return ctx.begin() + 1;
  }

  template <typename FormatContext>
  auto format(const test_dynamic_formattable&, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    int widths[3]{};
    for (size_t i = 0; i < amount; ++i) {
      detail::handle_dynamic_spec<detail::width_checker>(widths[i],
                                                         width_refs[i], ctx);
    }
    if (amount == 1) {
      return format_to(ctx.out(), "{:{}}", 41, widths[0]);
    } else if (amount == 2) {
      return format_to(ctx.out(), "{:{}}{:{}}", 41, widths[0], 42, widths[1]);
    } else if (amount == 3) {
      return format_to(ctx.out(), "{:{}}{:{}}{:{}}", 41, widths[0], 42,
                       widths[1], 43, widths[2]);
    } else {
      throw format_error("formatting error");
    }
  }
};
FMT_END_NAMESPACE

TEST(CompileTest, DynamicFormatSpecs) {
  EXPECT_EQ("  42foo  ",
            fmt::format(FMT_COMPILE("{:{}}{:{}}"), 42, 4, "foo", 5));
  EXPECT_EQ("  41",
            fmt::format(FMT_COMPILE("{:1}"), test_dynamic_formattable(), 4));
  EXPECT_EQ(" 41   42",
            fmt::format(FMT_COMPILE("{:2}"), test_dynamic_formattable(), 3, 5));
  EXPECT_EQ("   41 42  43", fmt::format(FMT_COMPILE("{:3}"),
                                        test_dynamic_formattable(), 5, 3, 4));
}

TEST(CompileTest, ManualOrdering) {
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{0}"), 42));
  EXPECT_EQ(" -42", fmt::format(FMT_COMPILE("{0:4}"), -42));
  EXPECT_EQ("41 43", fmt::format(FMT_COMPILE("{0} {1}"), 41, 43));
  EXPECT_EQ("41 43", fmt::format(FMT_COMPILE("{1} {0}"), 43, 41));
  EXPECT_EQ("41 43", fmt::format(FMT_COMPILE("{0} {2}"), 41, 42, 43));
  EXPECT_EQ("  41   43", fmt::format(FMT_COMPILE("{1:{2}} {0:4}"), 43, 41, 4));
  EXPECT_EQ("42   42",
            fmt::format(FMT_COMPILE("{1} {0:custom}"),
                        test_custom_formattable(), test_custom_formattable()));
  EXPECT_EQ(
      "true 42 42 foo 0x1234 foo",
      fmt::format(FMT_COMPILE("{0} {1} {2} {3} {4} {5}"), true, 42, 42.0f,
                  "foo", reinterpret_cast<void*>(0x1234), test_formattable()));
  EXPECT_EQ(L"42", fmt::format(FMT_COMPILE(L"{0}"), 42));
}

TEST(CompileTest, Named) {
  EXPECT_EQ("42", fmt::format(FMT_COMPILE("{}"), fmt::arg("arg", 42)));
  EXPECT_EQ("41 43", fmt::format(FMT_COMPILE("{} {}"), fmt::arg("arg", 41),
                                 fmt::arg("arg", 43)));

  EXPECT_EQ("foobar",
            fmt::format(FMT_COMPILE("{a0}{a1}"), fmt::arg("a0", "foo"),
                        fmt::arg("a1", "bar")));
  EXPECT_EQ("foobar",
            fmt::format(/*FMT_COMPILE(*/ "{}{a1}" /*)*/, fmt::arg("a0", "foo"),
                        fmt::arg("a1", "bar")));
  EXPECT_EQ("foofoo",
            fmt::format(/*FMT_COMPILE(*/ "{a0}{}" /*)*/, fmt::arg("a0", "foo"),
                        fmt::arg("a1", "bar")));
  EXPECT_EQ("foobar",
            fmt::format(/*FMT_COMPILE(*/ "{a0}{1}" /*)*/, fmt::arg("a0", "foo"),
                        fmt::arg("a1", "bar")));
  EXPECT_EQ("foobar",
            fmt::format(/*FMT_COMPILE(*/ "{0}{a1}" /*)*/, fmt::arg("a0", "foo"),
                        fmt::arg("a1", "bar")));

  EXPECT_EQ("foobar",
            fmt::format(FMT_COMPILE("{}{a1}"), "foo", fmt::arg("a1", "bar")));
  EXPECT_EQ("foobar",
            fmt::format(FMT_COMPILE("{a0}{a1}"), fmt::arg("a1", "bar"),
                        fmt::arg("a2", "baz"), fmt::arg("a0", "foo")));
  EXPECT_EQ(" bar foo ",
            fmt::format(FMT_COMPILE(" {foo} {bar} "), fmt::arg("foo", "bar"),
                        fmt::arg("bar", "foo")));

  EXPECT_THROW(fmt::format(FMT_COMPILE("{invalid}"), fmt::arg("valid", 42)),
               fmt::format_error);
}

TEST(CompileTest, FormatTo) {
  char buf[8];
  auto end = fmt::format_to(buf, FMT_COMPILE("{}"), 42);
  *end = '\0';
  EXPECT_STREQ("42", buf);
  end = fmt::format_to(buf, FMT_COMPILE("{:x}"), 42);
  *end = '\0';
  EXPECT_STREQ("2a", buf);
}

TEST(CompileTest, FormatToNWithCompileMacro) {
  constexpr auto buffer_size = 8;
  char buffer[buffer_size];
  auto res = fmt::format_to_n(buffer, buffer_size, FMT_COMPILE("{}"), 42);
  *res.out = '\0';
  EXPECT_STREQ("42", buffer);
  res = fmt::format_to_n(buffer, buffer_size, FMT_COMPILE("{:x}"), 42);
  *res.out = '\0';
  EXPECT_STREQ("2a", buffer);
}

TEST(CompileTest, TextAndArg) {
  EXPECT_EQ(">>>42<<<", fmt::format(FMT_COMPILE(">>>{}<<<"), 42));
  EXPECT_EQ("42!", fmt::format(FMT_COMPILE("{}!"), 42));
}

TEST(CompileTest, Empty) { EXPECT_EQ("", fmt::format(FMT_COMPILE(""))); }
#endif

#if FMT_USE_NONTYPE_TEMPLATE_PARAMETERS
TEST(CompileTest, CompileFormatStringLiteral) {
  using namespace fmt::literals;
  EXPECT_EQ("", fmt::format(""_cf));
  EXPECT_EQ("42", fmt::format("{}"_cf, 42));
}
#endif

#if __cplusplus >= 202002L || \
    (__cplusplus >= 201709L && FMT_GCC_VERSION >= 1002)
template <size_t max_string_length, typename Char = char> struct test_string {
  template <typename T> constexpr bool operator==(const T& rhs) const noexcept {
    return fmt::basic_string_view<Char>(rhs).compare(buffer.data()) == 0;
  }
  std::array<Char, max_string_length> buffer{};
};

template <size_t max_string_length, typename Char = char, typename... Args>
consteval auto test_format(auto format, const Args&... args) {
  test_string<max_string_length, Char> string{};
  fmt::format_to(string.buffer.data(), format, args...);
  return string;
}

TEST(CompileTimeFormattingTest, Bool) {
  EXPECT_EQ("true", test_format<5>(FMT_COMPILE("{}"), true));
  EXPECT_EQ("false", test_format<6>(FMT_COMPILE("{}"), false));
  EXPECT_EQ("true ", test_format<6>(FMT_COMPILE("{:5}"), true));
  EXPECT_EQ("1", test_format<2>(FMT_COMPILE("{:d}"), true));
}

TEST(CompileTimeFormattingTest, Integer) {
  EXPECT_EQ("42", test_format<3>(FMT_COMPILE("{}"), 42));
  EXPECT_EQ("420", test_format<4>(FMT_COMPILE("{}"), 420));
  EXPECT_EQ("42 42", test_format<6>(FMT_COMPILE("{} {}"), 42, 42));
  EXPECT_EQ("42 42",
            test_format<6>(FMT_COMPILE("{} {}"), uint32_t{42}, uint64_t{42}));

  EXPECT_EQ("+42", test_format<4>(FMT_COMPILE("{:+}"), 42));
  EXPECT_EQ("42", test_format<3>(FMT_COMPILE("{:-}"), 42));
  EXPECT_EQ(" 42", test_format<4>(FMT_COMPILE("{: }"), 42));

  EXPECT_EQ("-0042", test_format<6>(FMT_COMPILE("{:05}"), -42));

  EXPECT_EQ("101010", test_format<7>(FMT_COMPILE("{:b}"), 42));
  EXPECT_EQ("0b101010", test_format<9>(FMT_COMPILE("{:#b}"), 42));
  EXPECT_EQ("0B101010", test_format<9>(FMT_COMPILE("{:#B}"), 42));
  EXPECT_EQ("042", test_format<4>(FMT_COMPILE("{:#o}"), 042));
  EXPECT_EQ("0x4a", test_format<5>(FMT_COMPILE("{:#x}"), 0x4a));
  EXPECT_EQ("0X4A", test_format<5>(FMT_COMPILE("{:#X}"), 0x4a));

  EXPECT_EQ("   42", test_format<6>(FMT_COMPILE("{:5}"), 42));
  EXPECT_EQ("   42", test_format<6>(FMT_COMPILE("{:5}"), 42ll));
  EXPECT_EQ("   42", test_format<6>(FMT_COMPILE("{:5}"), 42ull));

  EXPECT_EQ("42  ", test_format<5>(FMT_COMPILE("{:<4}"), 42));
  EXPECT_EQ("  42", test_format<5>(FMT_COMPILE("{:>4}"), 42));
  EXPECT_EQ(" 42 ", test_format<5>(FMT_COMPILE("{:^4}"), 42));
  EXPECT_EQ("**-42", test_format<6>(FMT_COMPILE("{:*>5}"), -42));
}

TEST(CompileTimeFormattingTest, Char) {
  EXPECT_EQ("c", test_format<2>(FMT_COMPILE("{}"), 'c'));

  EXPECT_EQ("c  ", test_format<4>(FMT_COMPILE("{:3}"), 'c'));
  EXPECT_EQ("99", test_format<3>(FMT_COMPILE("{:d}"), 'c'));
}

TEST(CompileTimeFormattingTest, String) {
  EXPECT_EQ("42", test_format<3>(FMT_COMPILE("{}"), "42"));
  EXPECT_EQ("The answer is 42",
            test_format<17>(FMT_COMPILE("{} is {}"), "The answer", "42"));

  EXPECT_EQ("abc**", test_format<6>(FMT_COMPILE("{:*<5}"), "abc"));
  EXPECT_EQ("**🤡**", test_format<9>(FMT_COMPILE("{:*^6}"), "🤡"));
}

TEST(CompileTimeFormattingTest, Combination) {
  EXPECT_EQ("420, true, answer",
            test_format<18>(FMT_COMPILE("{}, {}, {}"), 420, true, "answer"));

  EXPECT_EQ(" -42", test_format<5>(FMT_COMPILE("{:{}}"), -42, 4));
}

TEST(CompileTimeFormattingTest, MultiByteFill) {
  EXPECT_EQ("жж42", test_format<8>(FMT_COMPILE("{:ж>4}"), 42));
}
#endif

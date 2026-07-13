#include <string_view>

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4702)
#endif
#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#if XMOLE2_HAVE_BQLOG
#  include <bq_log/bq_log.h>
#endif
#include <simdutf.h>

#include <boost/uuid/uuid.hpp>
#include <fmt/format.h>
#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>
#include <gtest/gtest.h>
#include <magic_enum/magic_enum.hpp>
#include <minizip-ng/mz.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <pugixml.hpp>
#include <re2/re2.h>

namespace
{

enum class DependencyState
{
  Ready,
};

TEST(Dependencies, ImportedTargetsCompileAndLink)
{
  auto values = absl::flat_hash_map<std::string_view, int> {
    { "answer", 42 }
  };
  EXPECT_EQ(values.at("answer"), 42);
  EXPECT_EQ(absl::StrCat("xmole", 2), "xmole2");
  EXPECT_EQ(fmt::format("xmole{}", 2), "xmole2");
  EXPECT_EQ(magic_enum::enum_name(DependencyState::Ready), "Ready");

  auto xml          = pugi::xml_document {};
  auto const parsed = xml.load_string("<root value=\"42\"/>");
  ASSERT_TRUE(parsed);
  EXPECT_EQ(xml.child("root").attribute("value").as_int(), 42);

  EXPECT_TRUE(RE2::FullMatch("xmole2", "xmole[0-9]"));
  EXPECT_NE(EVP_sha256(), nullptr);
  EXPECT_GT(OpenSSL_version_num(), 0UL);
  EXPECT_STREQ(MZ_VERSION, "4.0.10");
  EXPECT_TRUE(simdutf::validate_utf8("xmole2", 6));

  constexpr auto kMap = frozen::make_unordered_map<std::string_view, int>({
      { "xmole", 2 },
  });
  EXPECT_EQ(kMap.at("xmole"), 2);

  auto const uuid = boost::uuids::uuid {};
  EXPECT_TRUE(uuid.is_nil());

#if XMOLE2_HAVE_BQLOG
  auto const version = bq::log::get_version();
  static_cast<void>(version);
#endif
}

} // namespace


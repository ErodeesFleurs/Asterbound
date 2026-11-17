import std;
import star.asset_path;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.directives;
import star.hash;
import star.string;
import star.text_formatter;

#include "gtest/gtest.h"

using namespace Star;
using namespace std::literals;

namespace {
struct FilenameCase {
  std::string_view m_path;
  std::string_view m_directory;
  std::string_view m_filename;
  std::string_view m_extension;
};

struct SplitCase {
  std::string_view m_path;
  std::string_view m_base_path;
  std::optional<std::string_view> m_sub_path;
  std::string_view m_canonical;
};
}

TEST(AssetPathTest, FilenameRangesExcludeSubPathsAndDirectiveSlashes) {
  FilenameCase const cases[] = {
      {"/a/b.png:x", "/a/", "b.png", "png"},
      {"/a/b.png:x:y?replace=/other/file.jpg", "/a/", "b.png", "png"},
      {"/a/b.png?replace=/other/file.jpg", "/a/", "b.png", "png"},
      {"b.png:x?flipx", "", "b.png", "png"},
      {"/a.ext/noext:x?flipx", "/a.ext/", "noext", ""},
      {"/a/.hidden", "/a/", ".hidden", "hidden"},
      {"/a/archive.tar.gz:x", "/a/", "archive.tar.gz", "gz"},
      {"/a/file.:x", "/a/", "file.", ""},
      {"/a\\b.png:x", "/", "a\\b.png", "png"},
      {"/é/\0名.p\0ng:x?flipx"sv, "/é/", "\0名.p\0ng"sv, "p\0ng"sv},
  };
  for (auto const& test_case : cases) {
    SCOPED_TRACE(std::string(test_case.m_path));
    String const path(test_case.m_path);
    EXPECT_EQ(AssetPath::directory(path).utf8(), test_case.m_directory);
    EXPECT_EQ(AssetPath::filename(path).utf8(), test_case.m_filename);
    EXPECT_EQ(AssetPath::extension(path).utf8(), test_case.m_extension);
  }
}

TEST(AssetPathTest, EmptyAndDirectoryOnlyLocatorsHaveNoFilename) {
  FilenameCase const cases[] = {
      {"", "", "", ""},
      {"/", "/", "", ""},
      {"/a/", "/a/", "", ""},
      {"/a.ext/:x?replace=/b.png", "/a.ext/", "", ""},
      {":x?flipx", "", "", ""},
      {"?replace=/a/b.png", "", "", ""},
  };
  for (auto const& test_case : cases) {
    SCOPED_TRACE(std::string(test_case.m_path));
    String const path(test_case.m_path);
    EXPECT_EQ(AssetPath::directory(path).utf8(), test_case.m_directory);
    EXPECT_EQ(AssetPath::filename(path).utf8(), test_case.m_filename);
    EXPECT_EQ(AssetPath::extension(path).utf8(), test_case.m_extension);
  }
}

TEST(AssetPathTest, SplitNormalizesEmptySubPathsAndDirectiveSeparators) {
  SplitCase const cases[] = {
      {"", "", {}, ""},
      {"/a:", "/a", {}, "/a"},
      {"/a:?", "/a", {}, "/a"},
      {"/a:??flipx???flipy??", "/a", {}, "/a?flipx?flipy"},
      {"/a:x", "/a", "x", "/a:x"},
      {"/a::", "/a", ":", "/a::"},
      {"/a:x:y:z??flipx???flipy??", "/a", "x:y:z", "/a:x:y:z?flipx?flipy"},
      {"/a???", "/a", {}, "/a"},
      {"/a?unknown:x:y??flipx", "/a", {}, "/a?unknown:x:y?flipx"},
  };
  for (auto const& test_case : cases) {
    SCOPED_TRACE(std::string(test_case.m_path));
    auto const path = AssetPath::split(String(test_case.m_path));
    EXPECT_EQ(path.m_base_path.utf8(), test_case.m_base_path);
    ASSERT_EQ(path.m_sub_path.has_value(), test_case.m_sub_path.has_value());
    if (test_case.m_sub_path)
      EXPECT_EQ(path.m_sub_path->utf8(), *test_case.m_sub_path);
    EXPECT_EQ(AssetPath::join(path).utf8(), test_case.m_canonical);
    EXPECT_EQ(AssetPath::join(AssetPath(test_case.m_path)).utf8(), test_case.m_canonical);
  }
}

TEST(AssetPathTest, ViewConstructionOwnsOnlyTheSuppliedByteExtent) {
  auto const locator = "/é/a\0b.png:x\0y?flipx"sv;
  std::string const prefix = "ignored:";
  std::string owner = prefix + std::string(locator) + "?flipy/ignored";
  auto const view = std::string_view(owner).substr(prefix.size(), locator.size());
  AssetPath const path(view);
  auto const split = AssetPath::split(String(view));
  owner.assign(owner.size(), '#');
  EXPECT_EQ(path, split);
  EXPECT_EQ(path.m_base_path.utf8(), "/é/a\0b.png"sv);
  ASSERT_TRUE(path.m_sub_path);
  EXPECT_EQ(path.m_sub_path->utf8(), "x\0y"sv);
  EXPECT_EQ(AssetPath::join(path).utf8(), locator);
}

TEST(AssetPathTest, SubPathEditsNormalizeExistingDirectivesButKeepReplacementRaw) {
  String const path = "/a.png:old:frame??flipx???flipy??";
  EXPECT_EQ(AssetPath::set_sub_path(path, ""), "/a.png:?flipx?flipy");
  EXPECT_EQ(AssetPath::set_sub_path("/a.png", ""), "/a.png:");
  EXPECT_EQ(AssetPath::set_sub_path(path, "new:part?raw"), "/a.png:new:part?raw?flipx?flipy");
  EXPECT_EQ(AssetPath::set_sub_path("/a.png:old???", "new"), "/a.png:new");
  EXPECT_EQ(AssetPath::remove_sub_path(path), "/a.png?flipx?flipy");
  EXPECT_EQ(AssetPath::remove_sub_path("/a.png:??flipx??"), "/a.png?flipx");
  EXPECT_EQ(AssetPath::remove_sub_path("/a.png???flipx??"), "/a.png?flipx");
  EXPECT_EQ(AssetPath::remove_sub_path("/a.png:"), "/a.png");

  AssetPath components("/a.png");
  components.m_sub_path.emplace();
  EXPECT_EQ(AssetPath::join(components), "/a.png:");
  EXPECT_FALSE(components == AssetPath("/a.png"));
}

TEST(AssetPathTest, DirectiveEditsPreserveExactQuestionMarkBoundaries) {
  struct BoundaryCase {
    std::string_view m_path;
    std::string_view m_directives;
    std::string_view m_expected;
  };
  BoundaryCase const cases[] = {
      {"", "?flipx", "?flipx"},
      {"", "", ""},
      {"/a??", "", "/a??"},
      {"/a", "flipx", "/a?flipx"},
      {"/a?", "flipx", "/a?flipx"},
      {"/a", "?flipx", "/a?flipx"},
      {"/a?", "?flipx", "/a?flipx"},
      {"/a??", "??flipx??", "/a???flipx??"},
      {"/a", "?", "/a?"},
      {"/a?", "?", "/a?"},
      {"/a:x\0y"sv, "flipx", "/a:x\0y?flipx"sv},
  };
  for (auto const& test_case : cases) {
    SCOPED_TRACE(std::string(test_case.m_path));
    EXPECT_EQ(AssetPath::add_directives(String(test_case.m_path), String(test_case.m_directives)).utf8(),
              test_case.m_expected);
  }
  EXPECT_EQ(AssetPath::get_directives("/a:x??flipx???flipy??"), "?flipx???flipy??");
  EXPECT_EQ(AssetPath::get_directives("/a:x"), "");
  EXPECT_EQ(AssetPath::remove_directives("/a:x??flipx???flipy??"), "/a:x");
  EXPECT_EQ(AssetPath::remove_directives("/a:x"), "/a:x");
}

TEST(AssetPathTest, RelativeResolutionConcatenatesWithoutFilesystemNormalization) {
  EXPECT_EQ(AssetPath::relative_to("/a/b.png:x?replace=/ignored/", "../c//d\\e.png:y?flipx"),
            "/a/../c//d\\e.png:y?flipx");
  EXPECT_EQ(AssetPath::relative_to("/a/b/", "./c"), "/a/b/./c");
  EXPECT_EQ(AssetPath::relative_to("/a/b.png:x", ""), "/a/");
  EXPECT_EQ(AssetPath::relative_to("/a/b.png:x", "/c/../d//e:??flipx"), "/c/../d//e:??flipx");
  EXPECT_EQ(AssetPath::relative_to("b.png:x", "c.png"), "c.png");
  String const source("/é\0/a.png:x"sv);
  String const given("../名\0.png:f?flipx"sv);
  EXPECT_EQ(AssetPath::relative_to(source, given).utf8(), "/é\0/../名\0.png:f?flipx"sv);
}

TEST(AssetPathTest, EmbeddedRecognitionIsStrictAndIdentityIsByteSensitive) {
  for (std::string_view path : {"/opensb_data;", "/opensb_data;text;AbC", "/opensb_data;text;aBc:frame?flipx"}) {
    EXPECT_TRUE(AssetPath::is_embedded(String(path)));
    EXPECT_TRUE(AssetPath::is_remote(String(path)));
  }
  for (std::string_view path : {"", "/opensb_data", "/opensb_datax;text;AbC", "/OpenSB_data;text;AbC",
                               "opensb_data;text;AbC", "https://example.test/a.png"}) {
    EXPECT_FALSE(AssetPath::is_embedded(String(path)));
    EXPECT_FALSE(AssetPath::is_remote(String(path)));
  }

  std::unordered_set<AssetPath, Star::hash<AssetPath>> paths;
  for (std::string_view locator : {"/opensb_data;text;AbC", "/opensb_data;text;aBc", "/A.png:x?flipx",
                                  "/a.png:x?flipx", "/a.png:X?flipx", "/a.png:x?FLIPX"}) {
    AssetPath const path(locator);
    AssetPath const copy = path;
    EXPECT_EQ(path, copy);
    EXPECT_EQ(Star::hash<AssetPath>{}(path), Star::hash<AssetPath>{}(copy));
    EXPECT_TRUE(paths.insert(path).second);
    EXPECT_FALSE(paths.insert(copy).second);
  }
  AssetPath const no_sub_path("/a.png");
  AssetPath empty_sub_path = no_sub_path;
  empty_sub_path.m_sub_path.emplace();
  EXPECT_TRUE(paths.insert(no_sub_path).second);
  EXPECT_TRUE(paths.insert(empty_sub_path).second);
}

TEST(AssetPathTest, MovesResetSourceComponentsAndPreserveSelfAssignment) {
  AssetPath original("/a.png:frame?flipx?flipy");
  auto const expected = AssetPath::join(original);
  auto const expected_hash = Star::hash<AssetPath>{}(original);
  AssetPath moved(std::move(original));
  EXPECT_EQ(AssetPath::join(moved), expected);
  EXPECT_EQ(Star::hash<AssetPath>{}(moved), expected_hash);
  EXPECT_FALSE(original.m_sub_path);
  EXPECT_TRUE(original.m_directives.empty());
  EXPECT_EQ(original.m_directives, DirectivesGroup{});
  EXPECT_EQ(original.m_directives.hash(), DirectivesGroup{}.hash());

  AssetPath assigned("/discard.png:discard?flipx");
  assigned = std::move(moved);
  EXPECT_EQ(AssetPath::join(assigned), expected);
  EXPECT_FALSE(moved.m_sub_path);
  EXPECT_EQ(moved.m_directives, DirectivesGroup{});
  auto& same_path = assigned;
  assigned = std::move(same_path);
  EXPECT_EQ(AssetPath::join(assigned), expected);
  EXPECT_EQ(Star::hash<AssetPath>{}(assigned), expected_hash);

  assigned = AssetPath("/no-sub.png");
  EXPECT_FALSE(assigned.m_sub_path);
  EXPECT_TRUE(assigned.m_directives.empty());
  original = AssetPath("/reused.png:next?flipy");
  EXPECT_EQ(AssetPath::join(original), "/reused.png:next?flipy");
}

TEST(AssetPathTest, ComponentMoveConsumesOptionalAndDirectiveArguments) {
  String base_path("/a.png");
  std::optional<String> sub_path("frame");
  DirectivesGroup directives(String("??flipx???flipy??"));
  AssetPath const path(std::move(base_path), std::move(sub_path), std::move(directives));
  EXPECT_EQ(AssetPath::join(path), "/a.png:frame?flipx?flipy");
  EXPECT_FALSE(sub_path);
  EXPECT_EQ(directives, DirectivesGroup{});
  EXPECT_EQ(directives.hash(), DirectivesGroup{}.hash());

  String empty_base("/empty.png");
  std::optional<String> empty_sub_path(std::in_place);
  DirectivesGroup empty_directives;
  AssetPath const empty_path(std::move(empty_base), std::move(empty_sub_path), std::move(empty_directives));
  ASSERT_TRUE(empty_path.m_sub_path);
  EXPECT_TRUE(empty_path.m_sub_path->empty());
  EXPECT_FALSE(empty_sub_path);
  EXPECT_EQ(AssetPath::join(empty_path), "/empty.png:");
}

TEST(AssetPathTest, JoinTextAndWireUseTheSameCanonicalDirectiveEntries) {
  for (std::string_view locator : {"/a.png:x??flipx???flipy??"sv, "/a.png:???"sv,
                                   "/é/a\0.png:x\0y??flipx???"sv, ""sv}) {
    AssetPath const path(locator);
    auto const canonical = AssetPath::join(path);
    TextBuffer output;
    output.append("prefix:");
    write_text(output, path);
    EXPECT_EQ(output.view(), std::string("prefix:") + canonical.utf8());
    auto const path_wire = DataStreamBuffer::serialize(path);
    auto const string_wire = DataStreamBuffer::serialize(canonical);
    ASSERT_TRUE(path_wire);
    ASSERT_TRUE(string_wire);
    EXPECT_EQ(*path_wire, *string_wire);
    AssetPath decoded("/discard.png:old?flipx");
    ASSERT_TRUE(DataStreamBuffer::deserialize(decoded, path_wire->span()));
    EXPECT_EQ(AssetPath::join(decoded), canonical);
  }
}

TEST(AssetPathTest, TruncatedWireLeavesEveryDestinationComponentUnchanged) {
  auto const wire = DataStreamBuffer::serialize(AssetPath("/replacement.png:new?flipx?flipy"));
  ASSERT_TRUE(wire);
  AssetPath const original("/original.png:old:frame?flipy");
  auto const original_hash = Star::hash<AssetPath>{}(original);
  for (std::size_t extent = 0; extent < wire->size(); ++extent) {
    SCOPED_TRACE(extent);
    AssetPath destination = original;
    auto const read = DataStreamBuffer::deserialize(destination, wire->span().first(extent));
    EXPECT_FALSE(read);
    EXPECT_EQ(destination, original);
    EXPECT_EQ(AssetPath::join(destination), "/original.png:old:frame?flipy");
    EXPECT_EQ(Star::hash<AssetPath>{}(destination), original_hash);
  }
}

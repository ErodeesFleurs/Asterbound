#include "gtest/gtest.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

import std;
import star.algorithm;
import star.btree_database;
import star.byte_array;
import star.file;
import star.format;
import star.io_device;
import star.logging;
import star.random;
import star.string;

using namespace Star;

namespace {
class BTreeRepackerTest : public testing::Test {
protected:
  static constexpr std::size_t block_size = 128;
  static constexpr std::size_t value_size = 8192;
  Logger logger;
  String directory, executable, input, output;

  void SetUp() override {
    auto random = RandomService::create();
    ASSERT_TRUE(random);
    auto created = File::temporary_directory(**random);
    ASSERT_TRUE(created);
    directory = std::move(*created);
    input = File::relative_to(directory, "input.db");
    output = File::relative_to(directory, "output.db");
    // Works both in the build tree and after installing the test/tool pair.
    auto self = File::full_path("/proc/self/exe");
    ASSERT_TRUE(self);
    executable = File::relative_to(File::dir_name(*self), "btree_repacker");

    BTreeDatabase db(logger, "RepackerTest", 1);
    auto file = File::open(input, IOMode::ReadWrite);
    ASSERT_TRUE(file);
    ASSERT_TRUE(db.set_io_device(std::move(*file)));
    ASSERT_TRUE(db.set_block_size(block_size));
    ASSERT_TRUE(db.set_auto_commit(false));
    ASSERT_TRUE(db.open());
    ASSERT_TRUE(db.insert(ByteArray::from_string("a"), ByteArray(value_size, 'A')));
    ASSERT_TRUE(db.insert(ByteArray::from_string("b"), ByteArray(value_size, 'B')));
    ASSERT_TRUE(db.commit());
    ASSERT_TRUE(db.close(true));
  }

  void TearDown() override {
    if (!directory.empty())
      EXPECT_TRUE(File::remove_all(directory));
  }

  void run_tool(String const& target, int code, std::string const& diagnostic = "", rlim_t file_limit = 0) {
    // Fork only to install child-local limits and exec the real binary. A
    // re-exec of core_tests would build a different fixture directory.
    auto style = std::exchange(testing::GTEST_FLAG(death_test_style), "fast");
    auto restore = make_scope_exit([&] { testing::GTEST_FLAG(death_test_style) = std::move(style); });
    auto const& command = executable.utf8();
    auto const& source = input.utf8();
    auto const& destination = target.utf8();
    auto invoke = [&] {
      if (file_limit) {
        // A runner may redirect stdout to a regular log already over the
        // limit. Only the staged database should encounter that write limit.
        int sink = ::open("/dev/null", O_WRONLY);
        if (sink < 0 || ::dup2(sink, STDOUT_FILENO) < 0)
          std::_Exit(126);
        if (sink != STDOUT_FILENO)
          ::close(sink);
        struct rlimit limit{file_limit, file_limit};
        if (::signal(SIGXFSZ, SIG_IGN) == SIG_ERR || ::setrlimit(RLIMIT_FSIZE, &limit) != 0)
          std::_Exit(126);
      }
      ::execl(command.c_str(), command.c_str(), source.c_str(), destination.c_str(), nullptr);
      std::_Exit(127);
    };
    ASSERT_EXIT(invoke(), testing::ExitedWithCode(code), diagnostic);
  }

  void check_records(String const& path, bool has_first = true) {
    BTreeDatabase db(logger);
    auto file = File::open(path, IOMode::Read);
    ASSERT_TRUE(file);
    ASSERT_TRUE(db.set_io_device(std::move(*file)));
    ASSERT_TRUE(db.open());
    EXPECT_EQ(db.content_identifier(), String("RepackerTest"));
    EXPECT_EQ(db.block_size(), block_size);
    EXPECT_EQ(db.key_size(), 1u);
    EXPECT_EQ(db.record_count(), has_first ? 2u : 1u);
    auto first = db.find(ByteArray::from_string("a"));
    auto second = db.find(ByteArray::from_string("b"));
    ASSERT_TRUE(first);
    EXPECT_EQ(first->has_value(), has_first);
    if (*first)
      EXPECT_EQ(**first, ByteArray(value_size, 'A'));
    ASSERT_TRUE(second && *second);
    EXPECT_EQ(**second, ByteArray(value_size, 'B'));
    ASSERT_TRUE(db.close(true));
  }
};

TEST_F(BTreeRepackerTest, ReplacesSamePathAndAliasesWithoutTruncatingTheInput) {
  auto original = File::read_file(input);
  ASSERT_TRUE(original);
  ASSERT_EQ(::link(input.utf8().c_str(), output.utf8().c_str()), 0);
  run_tool(output, 0);
  check_records(output);
  EXPECT_EQ(File::read_file(input), *original);
  std::error_code error;
  EXPECT_FALSE(std::filesystem::equivalent(input.utf8(), output.utf8(), error));
  ASSERT_FALSE(error);

  ASSERT_TRUE(File::remove(output));
  ASSERT_EQ(::symlink(input.utf8().c_str(), output.utf8().c_str()), 0);
  run_tool(output, 0);
  EXPECT_EQ(File::is_symlink(output), false);
  check_records(output);
  EXPECT_EQ(File::read_file(input), *original);

  run_tool(input, 0);
  check_records(input);
  EXPECT_EQ(File::exists(input + ".new"), false);
}

TEST_F(BTreeRepackerTest, SkipsDamagedSourceLeafAndRecoversItsSibling) {
  auto bytes = File::read_file(input);
  ASSERT_TRUE(bytes);
  auto data = bytes->span();
  // BTreeDB5 first leaf block: LL, big-endian element count, first key.
  // Both records are larger than a block, so each has its own leaf.
  bool damaged = false;
  for (std::size_t pos = 512; pos + block_size <= data.size(); pos += block_size) {
    if (std::ranges::equal(data.subspan(pos, 7), std::string_view("LL\0\0\0\1a", 7))) {
      data[pos] = '?';
      damaged = true;
    }
  }
  ASSERT_TRUE(damaged);
  ASSERT_TRUE(File::write_file(*bytes, input));
  run_tool(output, 0);
  check_records(output, false);
  EXPECT_EQ(File::read_file(input), *bytes);
}

TEST_F(BTreeRepackerTest, DestinationInsertFailurePreservesOldOutputAndInput) {
  auto original = File::read_file(input);
  ASSERT_TRUE(original);
  String sentinel = "existing output";
  ASSERT_TRUE(File::write_file(sentinel, output));
  // The header and empty root fit; inserting the first real leaf cannot fit.
  run_tool(output, 1, "Error writing destination BTree", 4096);
  EXPECT_EQ(File::read_file_string(output), sentinel);
  EXPECT_EQ(File::exists(output + ".new"), false);
  run_tool(input, 1, "Error writing destination BTree", 4096);
  EXPECT_EQ(File::read_file(input), *original);
  EXPECT_EQ(File::exists(input + ".new"), false);
}

TEST_F(BTreeRepackerTest, DoesNotTouchExistingStageAndDiscardsStageOnRenameFailure) {
  auto original = File::read_file(input);
  ASSERT_TRUE(original);
  String sentinel = "unchanged";
  ASSERT_TRUE(File::write_file(sentinel, output));
  auto stage = output + ".new";
  ASSERT_EQ(::symlink(input.utf8().c_str(), stage.utf8().c_str()), 0);
  run_tool(output, 1);
  EXPECT_EQ(File::is_symlink(stage), true);
  EXPECT_EQ(File::read_file_string(output), sentinel);
  EXPECT_EQ(File::read_file(input), *original);

  ASSERT_TRUE(File::remove(stage));
  ASSERT_TRUE(File::remove(output));
  ASSERT_TRUE(File::create_directory(output));
  auto marker = File::relative_to(output, "marker");
  ASSERT_TRUE(File::write_file(sentinel, marker));
  run_tool(output, 1, "rename error");
  EXPECT_EQ(File::read_file_string(marker), sentinel);
  EXPECT_EQ(File::exists(stage), false);
}
} // namespace

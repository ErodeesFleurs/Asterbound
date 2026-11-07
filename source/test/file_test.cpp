#include <stdio.h>
#include "vorbis/vorbisfile.h"
#ifdef STAR_SYSTEM_LINUX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

import star.file;
import star.io_device;
import star.string;
import star.buffer;
import star.format;
import star.random;

import std;

#include "gtest/gtest.h"
import star.io_device_callbacks;

import star.byte_array;
import star.algorithm;
using namespace Star;

static_assert(!std::is_copy_constructible_v<File>);
static_assert(!std::is_copy_assignable_v<File>);
static_assert(!std::is_move_constructible_v<File>);
static_assert(!std::is_move_assignable_v<File>);

namespace {
  // An IODevice that reports more bytes than it was asked for, the way
  // File::pread used to turn a failed read into SIZE_MAX.
  struct OverlongReadDevice : IODevice {
    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char>) override { return 0; }
    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override { return 0; }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return 0; }
    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset, IOSeek) override { return {}; }
    [[nodiscard]] std::expected<std::size_t, StarException> read_absolute(StreamOffset, std::span<char>) override {
      return std::numeric_limits<std::size_t>::max();
    }
    [[nodiscard]] std::expected<std::size_t, StarException> write_absolute(StreamOffset,
                                                                           std::span<char const>) override {
      return std::numeric_limits<std::size_t>::max();
    }
    [[nodiscard]] std::expected<std::shared_ptr<::Star::IODevice>, StarException> clone() override { return std::make_shared<OverlongReadDevice>(); }
  };
}

TEST(FileTest, FullReadRejectsImpossibleLengths) {
  auto device = std::make_shared<OverlongReadDevice>();
  char buffer[4];
  // Without the length check these underflow the remaining length and loop
  // forever instead of failing.
  auto read = device->read_full_absolute(0, buffer);
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<IOException>());
  auto write = device->write_full_absolute(0, buffer);
  ASSERT_FALSE(write);
  EXPECT_TRUE(write.error().is<IOException>());
}

namespace {
  std::filesystem::path fsPath(String const& path) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<char8_t const*>(path.utf8().data()), path.utf8Size()));
  }

  // Never follows symlinks, so it is safe even when the removal under test
  // misbehaves.
  void removeTree(String const& path) {
    std::error_code ignored;
    std::filesystem::remove_all(fsPath(path), ignored);
  }
}

TEST(FileTest, DirectoryOpenFailsWithoutPreventingSubsequentFileReads) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });

  auto opened = File::open(*directory, IOMode::Read);
  ASSERT_FALSE(opened);
  EXPECT_TRUE(opened.error().is<IOException>());
  auto contents = File::read_file(*directory);
  ASSERT_FALSE(contents);
  EXPECT_TRUE(contents.error().is<IOException>());

  auto file = File::relative_to(*directory, "contents");
  String payload("ordinary file remains readable");
  ASSERT_TRUE(File::write_file(payload, file));
  EXPECT_EQ(File::read_file_string(file), payload);
}

TEST(FileTest, ClonePreservesTruncatedFileAndIndependentPosition) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto path = File::relative_to(*directory, "clone");
  auto opened = File::open(path, IOMode::ReadWrite | IOMode::Truncate | IOMode::Append);
  ASSERT_TRUE(opened);
  auto& file = **opened;
  auto payload = ByteArray::fromCString("0123456789abcdef");
  ASSERT_TRUE(file.write_full(payload.span()));
  ASSERT_TRUE(file.seek(5));
  auto cloned = file.clone();
  ASSERT_TRUE(cloned);
  EXPECT_EQ(file.size(), static_cast<StreamOffset>(payload.size()));
  EXPECT_EQ(file.pos(), 5);
  EXPECT_EQ((*cloned)->pos(), 5);
  EXPECT_FALSE((*cloned)->mode() & IOMode::Truncate);
  EXPECT_FALSE((*cloned)->mode() & IOMode::Append);
  char bytes[4]{};
  ASSERT_TRUE((*cloned)->read_full(bytes));
  EXPECT_EQ(ByteArray(bytes), ByteArray::fromCString("5678"));
  EXPECT_EQ(file.pos(), 5);
  EXPECT_EQ((*cloned)->pos(), 9);
  ASSERT_TRUE(file.seek(2));
  EXPECT_EQ((*cloned)->pos(), 9);
  EXPECT_EQ(File::read_file(path), payload);
  EXPECT_TRUE((*cloned)->close());
  EXPECT_TRUE(file.close());
}

TEST(FileTest, CloneDoesNotRecreateAnUnlinkedOriginal) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto path = File::relative_to(*directory, "unlinked");
  auto opened = File::open(path, IOMode::ReadWrite);
  ASSERT_TRUE(opened);
  ASSERT_TRUE((*opened)->write_full(std::span<char const>("kept", 4)));
  ASSERT_TRUE(File::remove(path));
  auto cloned = (*opened)->clone();
  ASSERT_FALSE(cloned);
  EXPECT_TRUE(cloned.error().is<IOException>());
  EXPECT_EQ(File::exists(path), false);
  EXPECT_EQ((*opened)->pos(), 4);
  char bytes[4]{};
  ASSERT_TRUE((*opened)->read_full_absolute(0, bytes));
  EXPECT_EQ(ByteArray(bytes), ByteArray::fromCString("kept"));
  EXPECT_TRUE((*opened)->close());

  auto ephemeral = File::ephemeral_file(**random);
  ASSERT_TRUE(ephemeral);
  auto unnamedClone = (*ephemeral)->clone();
  ASSERT_FALSE(unnamedClone);
  EXPECT_TRUE(unnamedClone.error().is<IOException>());
  EXPECT_TRUE((*ephemeral)->close());
}

TEST(FileTest, WholeReadsPreserveEmptyBinaryLargeAndUtf8Contents) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto path = File::relative_to(*directory, "whole");
  for (std::size_t size :
       {std::size_t(0), std::size_t(1), std::size_t(1024), std::size_t(65536), std::size_t(3 * 1024 * 1024 + 137)}) {
    SCOPED_TRACE(size);
    std::string bytes(size, '\0');
    for (std::size_t i = 0; i < size; ++i)
      bytes[i] = static_cast<char>(i % 251);
    auto payload = ByteArray(std::span<char const>(bytes));
    ASSERT_TRUE(File::write_file(payload, path));
    auto binary = File::read_file(path);
    ASSERT_TRUE(binary);
    EXPECT_EQ(*binary, payload);
    auto text = File::read_file_string(path);
    ASSERT_TRUE(text);
    EXPECT_EQ(text->utf8(), bytes);
  }
  String utf8("目录-é / данные / 🌌\n");
  ASSERT_TRUE(File::write_file(utf8, path));
  EXPECT_EQ(File::read_file_string(path), utf8);
  EXPECT_EQ(File::read_file(path), ByteArray(std::span<char const>(utf8.utf8())));
}

TEST(FileTest, SizeAndPositionedIoPreserveSequentialCursor) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto opened = File::ephemeral_file(**random);
  ASSERT_TRUE(opened);
  auto& file = **opened;
  std::string initial(64 + 4 * 4096, '#');
  ASSERT_TRUE(file.write_full(std::span<char const>(initial)));
  ASSERT_TRUE(file.seek(7));
  EXPECT_EQ(file.size(), static_cast<StreamOffset>(initial.size()));
  EXPECT_EQ(file.pos(), 7);
  EXPECT_EQ(file.at_end(), false);
  EXPECT_EQ(file.pos(), 7);
  ASSERT_TRUE(file.resize(static_cast<StreamOffset>(initial.size() + 1)));
  EXPECT_EQ(file.pos(), 7);
  ASSERT_TRUE(file.resize(static_cast<StreamOffset>(initial.size())));
  EXPECT_EQ(file.pos(), 7);

  std::atomic<bool> failed = false;
  std::vector<std::jthread> workers;
  for (unsigned worker = 0; worker < 4; ++worker) {
    workers.emplace_back([&, worker]() {
      std::array<char, 4096> expected;
      expected.fill(static_cast<char>('a' + worker));
      auto offset = static_cast<StreamOffset>(64 + worker * expected.size());
      for (unsigned iteration = 0; iteration < 64; ++iteration) {
        if (!file.write_full_absolute(offset, expected)) {
          failed = true;
          return;
        }
        std::array<char, 4096> actual{};
        if (!file.read_full_absolute(offset, actual) || actual != expected) {
          failed = true;
          return;
        }
      }
    });
  }
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    ASSERT_TRUE(file.seek(7));
    EXPECT_EQ(file.size(), static_cast<StreamOffset>(initial.size()));
    EXPECT_EQ(file.at_end(), false);
    std::array<char, 16> actual{};
    ASSERT_TRUE(file.read_full(actual));
    EXPECT_TRUE(std::ranges::all_of(actual, [](char c) { return c == '#'; }));
    EXPECT_EQ(file.pos(), 23);
  }
  workers.clear();
  EXPECT_FALSE(failed);
  EXPECT_EQ(file.pos(), 23);
  ASSERT_TRUE(file.seek(0, IOSeek::End));
  EXPECT_EQ(file.at_end(), true);
  EXPECT_EQ(file.pos(), static_cast<StreamOffset>(initial.size()));
  EXPECT_TRUE(file.close());
}

TEST(FileTest, ClosedResizeDoesNotOpenOrAlterAFile) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto path = File::relative_to(*directory, "closed");
  ASSERT_TRUE(File::write_file(String("unchanged"), path));
  File file(path);
  auto resized = file.resize(1);
  ASSERT_FALSE(resized);
  EXPECT_TRUE(resized.error().is<IOException>());
  EXPECT_FALSE(file.is_open());
  EXPECT_EQ(File::read_file_string(path), "unchanged");
  ASSERT_TRUE(file.open(IOMode::Read));
  EXPECT_FALSE(file.resize(1));
  EXPECT_TRUE(file.close());
  EXPECT_EQ(File::read_file_string(path), "unchanged");
}

TEST(FileTest, All) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto fileResult = File::ephemeral_file(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  ASSERT_TRUE(file->resize(1000));
  ASSERT_TRUE(file->resize(0));
  ASSERT_TRUE(file->resize(500));
  auto size = file->size();
  ASSERT_TRUE(size);
  EXPECT_EQ(*size, 500);

  auto dirResult = File::temporary_directory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  ASSERT_TRUE(File::create_directory(File::relative_to(dir, "inner")));
  EXPECT_EQ(File::is_directory(File::relative_to(dir, "inner") + "/"), true);
  EXPECT_TRUE(File::remove_all(dir));

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  EXPECT_EQ(File::base_name("/foo/bar"), "bar");
  EXPECT_EQ(File::base_name("\\foo\\bar\\"), "bar");
  EXPECT_EQ(File::base_name("/foo/bar/baz"), "baz");
  EXPECT_EQ(File::dir_name("\\foo\\bar"), "\\foo");
  EXPECT_EQ(File::dir_name("/foo\\bar/"), "/foo");
  EXPECT_EQ(File::dir_name("/foo/bar\\baz"), "/foo/bar");
  EXPECT_EQ(File::dir_name("foo/bar/baz"), "foo/bar");

  EXPECT_EQ(File::relative_to("c:\\foo\\", "bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo", "bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo\\", "\\bar"), "\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo\\", ".\\bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo\\.", ".\\bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo\\.", "c:\\bar"), "c:\\bar");
  EXPECT_EQ(File::relative_to("c:\\foo\\.", "c:bar\\"), "c:bar\\");
  EXPECT_EQ(File::relative_to("c:\\foo.", "bar"), "c:\\foo.\\bar");
#else
  EXPECT_EQ(File::base_name("/foo/bar"), "bar");
  EXPECT_EQ(File::base_name("/foo/bar/"), "bar");
  EXPECT_EQ(File::base_name("/foo/bar/baz"), "baz");
  EXPECT_EQ(File::dir_name("/foo/bar"), "/foo");
  EXPECT_EQ(File::dir_name("/foo/bar/"), "/foo");
  EXPECT_EQ(File::dir_name("/foo/bar/baz"), "/foo/bar");
  EXPECT_EQ(File::dir_name("foo/bar/baz"), "foo/bar");

  EXPECT_EQ(File::relative_to("/foo", "bar"), "/foo/bar");
  EXPECT_EQ(File::relative_to("/foo", "bar/"), "/foo/bar/");
  EXPECT_EQ(File::relative_to("/foo", "/bar/"), "/bar/");
#endif
}

TEST(FileTest, CopyOverwritesBinaryFilesAndRotatesBackups) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto source = File::relative_to(*directory, "source-日");
  auto target = File::relative_to(*directory, "target-日");
  std::string bytes(8193, '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<char>(i % 251);
  String payload(std::move(bytes));
  ASSERT_TRUE(File::write_file(payload, source));
  ASSERT_TRUE(File::write_file(String(std::string(16384, 'x')), target));
  ASSERT_TRUE(File::copy(source, target));
  EXPECT_EQ(File::read_file_string(target), payload);
  EXPECT_EQ(File::read_file_string(source), payload);

  ASSERT_TRUE(File::write_file(String("first"), target));
  ASSERT_TRUE(File::backup_file_in_sequence(target, 2));
  ASSERT_TRUE(File::write_file(String("second"), target));
  ASSERT_TRUE(File::backup_file_in_sequence(target, 2));
  EXPECT_EQ(File::read_file_string(target + ".1"), "second");
  EXPECT_EQ(File::read_file_string(target + ".2"), "first");
  EXPECT_EQ(File::read_file_string(target), "second");

  ASSERT_TRUE(File::write_file(String(), source));
  ASSERT_TRUE(File::copy(source, target));
  EXPECT_EQ(File::read_file_string(target), "");
}

TEST(FileTest, CopyErrorsDoNotTruncateSourceOrExistingTarget) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto source = File::relative_to(*directory, "source");
  auto target = File::relative_to(*directory, "target");
  ASSERT_TRUE(File::write_file(String("source bytes"), source));
  ASSERT_TRUE(File::write_file(String("target bytes"), target));
  auto sameFile = File::copy(source, source);
  ASSERT_FALSE(sameFile);
  EXPECT_TRUE(sameFile.error().is<IOException>());
  EXPECT_EQ(File::read_file_string(source), "source bytes");
  EXPECT_FALSE(File::copy(File::relative_to(*directory, "missing"), target));
  EXPECT_FALSE(File::copy(*directory, target));
  EXPECT_EQ(File::read_file_string(target), "target bytes");
  EXPECT_FALSE(File::copy(source, File::relative_to(*directory, "missing/target")));

#ifdef STAR_SYSTEM_LINUX
  auto alias = File::relative_to(*directory, "alias");
  ASSERT_EQ(::link(source.utf8Ptr(), alias.utf8Ptr()), 0);
  EXPECT_FALSE(File::copy(source, alias));
  EXPECT_EQ(File::read_file_string(source), "source bytes");
#endif
}

TEST(FileTest, DirectoryHelpersPreserveFailureBoundaries) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporary_directory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto child = File::relative_to(*directory, "nested/child");
  ASSERT_TRUE(File::create_directories(child));
  EXPECT_TRUE(File::create_directories(child));
  auto existing = File::create_directory(child);
  ASSERT_FALSE(existing);
  EXPECT_TRUE(existing.error().is<IOException>());

  auto file = File::relative_to(*directory, "regular");
  ASSERT_TRUE(File::write_file(String("kept"), file));
  EXPECT_FALSE(File::create_directory(file));
  EXPECT_FALSE(File::create_directories(file));
  EXPECT_FALSE(File::create_directories(File::relative_to(file, "child")));
  EXPECT_FALSE(File::remove_all(file));
  auto contents = File::read_file_string(file);
  ASSERT_TRUE(contents);
  EXPECT_EQ(*contents, "kept");
  EXPECT_FALSE(File::directory_entries(file));

  auto missing = File::relative_to(*directory, "missing");
  EXPECT_FALSE(File::remove(missing));
  EXPECT_FALSE(File::remove_all(missing));
  EXPECT_EQ(File::exists(missing), false);
  EXPECT_EQ(File::is_regular_file(missing), false);
  EXPECT_EQ(File::is_directory(missing), false);
  EXPECT_EQ(File::is_symlink(missing), false);
  EXPECT_EQ(File::exists(File::relative_to(file, "child")), false);
  EXPECT_EQ(File::is_directory(File::relative_to(file, "child")), false);
  EXPECT_FALSE(File::create_directory(""));
  EXPECT_FALSE(File::create_directories(""));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  EXPECT_FALSE(File::create_directories("\\"));
  EXPECT_FALSE(File::create_directories("/"));
#else
  EXPECT_TRUE(File::create_directories("/"));
#endif
}

TEST(FileTest, CurrentDirectoryChangesAndReportsErrors) {
  auto original = File::current_directory();
  ASSERT_TRUE(original);
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporary_directory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto restore = finally([&]() { EXPECT_TRUE(File::change_directory(*original)); });
  auto child = File::relative_to(*directory, "目录-é");
  ASSERT_TRUE(File::create_directory(child));
  ASSERT_TRUE(File::change_directory(child));
  auto current = File::current_directory();
  auto expected = File::full_path(child);
  ASSERT_TRUE(current);
  ASSERT_TRUE(expected);
  EXPECT_EQ(*current, *expected);
  EXPECT_FALSE(File::change_directory(File::relative_to(child, "missing")));
  auto unchanged = File::current_directory();
  ASSERT_TRUE(unchanged);
  EXPECT_EQ(*unchanged, *current);
}

TEST(FileTest, DirectoryListingPreservesDotsAndUtf8Names) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporary_directory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  String child = "目录-é";
  String file = "данные.txt";
  ASSERT_TRUE(File::create_directory(File::relative_to(*directory, child)));
  ASSERT_TRUE(File::write_file(String("contents"), File::relative_to(*directory, file)));

  auto entries = File::directory_entries(*directory);
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 2u);
  auto contains = [](auto const& list, String const& name, bool directoryEntry) {
    return std::ranges::any_of(list, [&](auto const& entry) {
      return entry.first == name && entry.second == directoryEntry;
    });
  };
  EXPECT_TRUE(contains(*entries, child, true));
  EXPECT_TRUE(contains(*entries, file, false));

  auto withDots = File::directory_entries(*directory, false);
  ASSERT_TRUE(withDots);
  EXPECT_EQ(withDots->size(), 4u);
  EXPECT_TRUE(contains(*withDots, ".", true));
  EXPECT_TRUE(contains(*withDots, "..", true));
  EXPECT_TRUE(contains(*withDots, child, true));
  EXPECT_TRUE(contains(*withDots, file, false));
}

#ifndef STAR_SYSTEM_FAMILY_WINDOWS
TEST(FileTest, FullPathRequiresExistingTargetsAndResolvesSymlinks) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporary_directory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto real = File::relative_to(*directory, "real");
  ASSERT_TRUE(File::create_directory(real));
  auto resolved = File::full_path(real + "/../real/");
  auto canonical = File::full_path(real);
  ASSERT_TRUE(resolved);
  ASSERT_TRUE(canonical);
  EXPECT_EQ(*resolved, *canonical);
  EXPECT_FALSE(File::full_path(File::relative_to(*directory, "missing")));

  auto link = File::relative_to(*directory, "link");
  std::error_code error;
  std::filesystem::create_directory_symlink(fsPath(real), fsPath(link), error);
  if (error)
    GTEST_SKIP() << "cannot create directory symlinks here: " << error.message();
  auto linked = File::full_path(link);
  ASSERT_TRUE(linked);
  EXPECT_EQ(*linked, *canonical);
  EXPECT_TRUE(File::create_directories(link));
  EXPECT_FALSE(File::create_directory(link));
  EXPECT_EQ(File::is_directory(link), true);

  auto broken = File::relative_to(*directory, "broken");
  std::filesystem::create_symlink(fsPath(File::relative_to(*directory, "missing")), fsPath(broken), error);
  ASSERT_FALSE(error) << error.message();
  EXPECT_EQ(File::is_symlink(broken), true);
  EXPECT_EQ(File::exists(broken), false);
  EXPECT_FALSE(File::full_path(broken));
  EXPECT_TRUE(File::remove_all(broken));
  EXPECT_EQ(File::is_symlink(broken), false);
}
#endif

TEST(FileTest, RemoveDirectoryRecursiveKeepsSymlinkTargets) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  // The removal used to descend into directory symlinks, so a link pointing
  // outside of the directory deleted the target's contents.
  auto dirResult = File::temporary_directory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto outsideResult = File::temporary_directory(random);
  ASSERT_TRUE(outsideResult);
  auto outside = std::move(*outsideResult);
  auto finallyGuard = finally([&dir, &outside]() {
    removeTree(dir);
    removeTree(outside);
  });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relative_to(outside, "real")), ec);
  ASSERT_TRUE(File::write_file(String("kept"), File::relative_to(File::relative_to(outside, "real"), "keep.txt")));
  ASSERT_TRUE(File::write_file(String("doomed"), File::relative_to(dir, "file.txt")));
  std::filesystem::create_directory_symlink(fsPath(File::relative_to(outside, "real")),
                                            fsPath(File::relative_to(dir, "link")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();

  EXPECT_EQ(File::is_symlink(File::relative_to(dir, "link")), true);
  EXPECT_EQ(File::is_symlink(File::relative_to(dir, "file.txt")), false);

  EXPECT_TRUE(File::remove_all(dir));
  EXPECT_EQ(File::is_directory(dir), false);
  // The directory the symlink pointed at has to be untouched.
  EXPECT_EQ(File::is_regular_file(File::relative_to(File::relative_to(outside, "real"), "keep.txt")), true);

  // The root itself can be a link, with or without a trailing separator.
  for (std::string_view suffix : {"", "/"}) {
    std::filesystem::create_directory_symlink(fsPath(File::relative_to(outside, "real")), fsPath(dir), ec);
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_TRUE(File::remove_all(dir + suffix));
    EXPECT_EQ(File::is_symlink(dir), false);
    EXPECT_EQ(File::is_regular_file(File::relative_to(File::relative_to(outside, "real"), "keep.txt")), true);
  }
}

TEST(FileTest, RemoveDirectoryRecursiveHandlesSymlinkLoops) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  // A symlink pointing at one of its own parent directories used to make the
  // removal walk the same tree over and over until it errored out half way
  // through ("remove error: No such file or directory").
  auto dirResult = File::temporary_directory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto finallyGuard = finally([&dir]() { removeTree(dir); });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relative_to(dir, "real")), ec);
  ASSERT_TRUE(File::write_file(String("doomed"), File::relative_to(File::relative_to(dir, "real"), "file.txt")));
  std::filesystem::create_directory_symlink(fsPath(dir), fsPath(File::relative_to(dir, "loop")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();

  EXPECT_TRUE(File::remove_all(dir));
  EXPECT_EQ(File::is_directory(dir), false);
}

TEST(FileTest, OggDeviceCallbacksReadAndSeek) {
  Buffer device(ByteArray::fromCString("abcdef"));
  auto adapterResult = IODeviceCallbacks::create(device);
  ASSERT_TRUE(adapterResult);
  auto adapter = std::move(*adapterResult);
  ov_callbacks callbacks{};
  adapter.setupOggCallbacks(callbacks);

  char output[4]{};
  EXPECT_EQ(callbacks.read_func(output, 2, 2, &adapter), 2u);
  EXPECT_FALSE(adapter.takeError());
  EXPECT_EQ(ByteArray(std::span(output).first(4)), ByteArray::fromCString("abcd"));
  EXPECT_EQ(callbacks.tell_func(&adapter), 4);
  EXPECT_FALSE(adapter.takeError());

  EXPECT_EQ(callbacks.seek_func(&adapter, -2, SEEK_CUR), 0);
  EXPECT_FALSE(adapter.takeError());
  EXPECT_EQ(callbacks.tell_func(&adapter), 2);
  EXPECT_FALSE(adapter.takeError());
  EXPECT_EQ(callbacks.read_func(output, 2, 2, &adapter), 2u);
  EXPECT_FALSE(adapter.takeError());
  EXPECT_EQ(ByteArray(std::span(output).first(4)), ByteArray::fromCString("cdef"));
  EXPECT_EQ(callbacks.read_func(output, 2, 2, &adapter), 0u);
  EXPECT_FALSE(adapter.takeError());
}

#ifdef STAR_SYSTEM_LINUX
TEST(FileTest, WholeReadsConsumeStreamsWhoseMetadataSizeIsZero) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto path = File::relative_to(*directory, "whole-read-pipe");
  ASSERT_EQ(::mkfifo(path.utf8Ptr(), 0600), 0);
  std::string bytes(4096, '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<char>(i % 251);
  auto payload = ByteArray(std::span<char const>(bytes));
  for (bool as_text : {false, true}) {
    std::atomic<bool> writer_failed = false;
    std::jthread writer([&]() {
      // The initial write-only open waits for the reader. The ReadWrite handle
      // then keeps a read endpoint alive even if a broken reader returns early,
      // so this regression cannot terminate the process with SIGPIPE.
      File initial(path);
      if (!initial.open(IOMode::Write)) {
        writer_failed = true;
        return;
      }
      File stream(path);
      if (!stream.open(IOMode::ReadWrite)) {
        writer_failed = true;
        return;
      }
      if (!initial.close() || !stream.write_full(payload.span()) || !stream.close())
        writer_failed = true;
    });
    if (as_text) {
      auto contents = File::read_file_string(path);
      ASSERT_TRUE(contents);
      EXPECT_EQ(contents->utf8(), bytes);
    } else {
      auto contents = File::read_file(path);
      ASSERT_TRUE(contents);
      EXPECT_EQ(*contents, payload);
    }
    writer.join();
    EXPECT_FALSE(writer_failed);
  }
}

namespace {
  int descriptorForFile(String const& path) {
    for (auto const& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
      std::error_code ec;
      auto target = std::filesystem::read_symlink(entry.path(), ec);
      if (!ec && target == fsPath(path))
        return std::stoi(entry.path().filename().string());
    }
    return -1;
  }

}

TEST(FileTest, AppendSeekFailureReleasesDescriptor) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto dirResult = File::temporary_directory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto cleanup = finally([&]() { removeTree(dir); });
  auto path = File::relative_to(dir, "pipe");
  ASSERT_EQ(::mkfifo(path.utf8Ptr(), 0600), 0);
  // Exception symbolization may cache ELF descriptors; only this FIFO is owned here.
  for (unsigned i = 0; i < 8; ++i) {
    auto opened = File::open(path, IOMode::ReadWrite | IOMode::Append);
    ASSERT_FALSE(opened);
    EXPECT_TRUE(opened.error().is<IOException>());
    EXPECT_LT(descriptorForFile(path), 0);
  }
}

TEST(FileTest, FailedCloseConsumesDescriptorOwnership) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto fileResult = File::temporary_file(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  auto path = file->file_name();
  auto cleanup = finally([&]() { EXPECT_TRUE(File::remove(path)); });
  int descriptor = descriptorForFile(path);
  ASSERT_GE(descriptor, 0);
  ASSERT_EQ(::close(descriptor), 0);

  auto closed = file->close();
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_FALSE(file->is_open());
  EXPECT_TRUE(file->close());
  // No destructor retry of the invalid or potentially reused descriptor.
  file.reset();
}

TEST(FileTest, DestructorReportsCloseFailureWithoutTerminating) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto fileResult = File::temporary_file(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  auto path = file->file_name();
  auto cleanup = finally([&]() { EXPECT_TRUE(File::remove(path)); });
  int descriptor = descriptorForFile(path);
  ASSERT_GE(descriptor, 0);
  ASSERT_EQ(::close(descriptor), 0);
  file.reset();
}

TEST(FileTest, SyncFailureReachesCaller) {
  auto fileResult = File::open("/dev/full", IOMode::Write);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  auto synced = file->sync();
  ASSERT_FALSE(synced);
  EXPECT_TRUE(synced.error().is<IOException>());
  EXPECT_TRUE(file->close());
}
#endif

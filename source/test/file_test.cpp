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

namespace {
  // An IODevice that reports more bytes than it was asked for, the way
  // File::pread used to turn a failed read into SIZE_MAX.
  struct OverlongReadDevice : IODevice {
    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char>) override { return 0; }
    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override { return 0; }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return 0; }
    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset, IOSeek) override { return {}; }
    [[nodiscard]] std::expected<std::size_t, StarException> readAbsolute(StreamOffset, std::span<char>) override { return std::numeric_limits<std::size_t>::max(); }
    [[nodiscard]] std::expected<std::size_t, StarException> writeAbsolute(StreamOffset, std::span<char const>) override { return std::numeric_limits<std::size_t>::max(); }
    [[nodiscard]] std::expected<std::shared_ptr<::Star::IODevice>, StarException> clone() override { return std::make_shared<OverlongReadDevice>(); }
  };
}

TEST(FileTest, FullReadRejectsImpossibleLengths) {
  auto device = std::make_shared<OverlongReadDevice>();
  char buffer[4];
  // Without the length check these underflow the remaining length and loop
  // forever instead of failing.
  auto read = device->readFullAbsolute(0, buffer);
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<IOException>());
  auto write = device->writeFullAbsolute(0, buffer);
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
  auto directory = File::temporaryDirectory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });

  auto opened = File::open(*directory, IOMode::Read);
  ASSERT_FALSE(opened);
  EXPECT_TRUE(opened.error().is<IOException>());
  auto contents = File::readFile(*directory);
  ASSERT_FALSE(contents);
  EXPECT_TRUE(contents.error().is<IOException>());

  auto file = File::relativeTo(*directory, "contents");
  String payload("ordinary file remains readable");
  ASSERT_TRUE(File::writeFile(payload, file));
  EXPECT_EQ(File::readFileString(file), payload);
}

TEST(FileTest, All) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto fileResult = File::ephemeralFile(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  ASSERT_TRUE(file->resize(1000));
  ASSERT_TRUE(file->resize(0));
  ASSERT_TRUE(file->resize(500));
  auto size = file->size();
  ASSERT_TRUE(size);
  EXPECT_EQ(*size, 500);

  auto dirResult = File::temporaryDirectory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(dir, "inner")));
  EXPECT_EQ(File::isDirectory(File::relativeTo(dir, "inner") + "/"), true);
  EXPECT_TRUE(File::removeDirectoryRecursive(dir));

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  EXPECT_EQ(File::baseName("/foo/bar"), "bar");
  EXPECT_EQ(File::baseName("\\foo\\bar\\"), "bar");
  EXPECT_EQ(File::baseName("/foo/bar/baz"), "baz");
  EXPECT_EQ(File::dirName("\\foo\\bar"), "\\foo");
  EXPECT_EQ(File::dirName("/foo\\bar/"), "/foo");
  EXPECT_EQ(File::dirName("/foo/bar\\baz"), "/foo/bar");
  EXPECT_EQ(File::dirName("foo/bar/baz"), "foo/bar");

  EXPECT_EQ(File::relativeTo("c:\\foo\\", "bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo", "bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo\\", "\\bar"), "\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo\\", ".\\bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo\\.", ".\\bar"), "c:\\foo\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo\\.", "c:\\bar"), "c:\\bar");
  EXPECT_EQ(File::relativeTo("c:\\foo\\.", "c:bar\\"), "c:bar\\");
  EXPECT_EQ(File::relativeTo("c:\\foo.", "bar"), "c:\\foo.\\bar");
#else
  EXPECT_EQ(File::baseName("/foo/bar"), "bar");
  EXPECT_EQ(File::baseName("/foo/bar/"), "bar");
  EXPECT_EQ(File::baseName("/foo/bar/baz"), "baz");
  EXPECT_EQ(File::dirName("/foo/bar"), "/foo");
  EXPECT_EQ(File::dirName("/foo/bar/"), "/foo");
  EXPECT_EQ(File::dirName("/foo/bar/baz"), "/foo/bar");
  EXPECT_EQ(File::dirName("foo/bar/baz"), "foo/bar");

  EXPECT_EQ(File::relativeTo("/foo", "bar"), "/foo/bar");
  EXPECT_EQ(File::relativeTo("/foo", "bar/"), "/foo/bar/");
  EXPECT_EQ(File::relativeTo("/foo", "/bar/"), "/bar/");
#endif
}

TEST(FileTest, CopyOverwritesBinaryFilesAndRotatesBackups) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporaryDirectory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto source = File::relativeTo(*directory, "source-日");
  auto target = File::relativeTo(*directory, "target-日");
  std::string bytes(8193, '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<char>(i % 251);
  String payload(std::move(bytes));
  ASSERT_TRUE(File::writeFile(payload, source));
  ASSERT_TRUE(File::writeFile(String(std::string(16384, 'x')), target));
  ASSERT_TRUE(File::copy(source, target));
  EXPECT_EQ(File::readFileString(target), payload);
  EXPECT_EQ(File::readFileString(source), payload);

  ASSERT_TRUE(File::writeFile(String("first"), target));
  ASSERT_TRUE(File::backupFileInSequence(target, 2));
  ASSERT_TRUE(File::writeFile(String("second"), target));
  ASSERT_TRUE(File::backupFileInSequence(target, 2));
  EXPECT_EQ(File::readFileString(target + ".1"), "second");
  EXPECT_EQ(File::readFileString(target + ".2"), "first");
  EXPECT_EQ(File::readFileString(target), "second");

  ASSERT_TRUE(File::writeFile(String(), source));
  ASSERT_TRUE(File::copy(source, target));
  EXPECT_EQ(File::readFileString(target), "");
}

TEST(FileTest, CopyErrorsDoNotTruncateSourceOrExistingTarget) {
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporaryDirectory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto source = File::relativeTo(*directory, "source");
  auto target = File::relativeTo(*directory, "target");
  ASSERT_TRUE(File::writeFile(String("source bytes"), source));
  ASSERT_TRUE(File::writeFile(String("target bytes"), target));
  auto sameFile = File::copy(source, source);
  ASSERT_FALSE(sameFile);
  EXPECT_TRUE(sameFile.error().is<IOException>());
  EXPECT_EQ(File::readFileString(source), "source bytes");
  EXPECT_FALSE(File::copy(File::relativeTo(*directory, "missing"), target));
  EXPECT_FALSE(File::copy(*directory, target));
  EXPECT_EQ(File::readFileString(target), "target bytes");
  EXPECT_FALSE(File::copy(source, File::relativeTo(*directory, "missing/target")));

#ifdef STAR_SYSTEM_LINUX
  auto alias = File::relativeTo(*directory, "alias");
  ASSERT_EQ(::link(source.utf8Ptr(), alias.utf8Ptr()), 0);
  EXPECT_FALSE(File::copy(source, alias));
  EXPECT_EQ(File::readFileString(source), "source bytes");
#endif
}

TEST(FileTest, DirectoryHelpersPreserveFailureBoundaries) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporaryDirectory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto child = File::relativeTo(*directory, "nested/child");
  ASSERT_TRUE(File::makeDirectoryRecursive(child));
  EXPECT_TRUE(File::makeDirectoryRecursive(child));
  auto existing = File::makeDirectory(child);
  ASSERT_FALSE(existing);
  EXPECT_TRUE(existing.error().is<IOException>());

  auto file = File::relativeTo(*directory, "regular");
  ASSERT_TRUE(File::writeFile(String("kept"), file));
  EXPECT_FALSE(File::makeDirectory(file));
  EXPECT_FALSE(File::makeDirectoryRecursive(file));
  EXPECT_FALSE(File::makeDirectoryRecursive(File::relativeTo(file, "child")));
  EXPECT_FALSE(File::removeDirectoryRecursive(file));
  auto contents = File::readFileString(file);
  ASSERT_TRUE(contents);
  EXPECT_EQ(*contents, "kept");
  EXPECT_FALSE(File::dirList(file));

  auto missing = File::relativeTo(*directory, "missing");
  EXPECT_FALSE(File::remove(missing));
  EXPECT_FALSE(File::removeDirectoryRecursive(missing));
  EXPECT_EQ(File::exists(missing), false);
  EXPECT_EQ(File::isFile(missing), false);
  EXPECT_EQ(File::isDirectory(missing), false);
  EXPECT_EQ(File::isSymlink(missing), false);
  EXPECT_EQ(File::exists(File::relativeTo(file, "child")), false);
  EXPECT_EQ(File::isDirectory(File::relativeTo(file, "child")), false);
  EXPECT_FALSE(File::makeDirectory(""));
  EXPECT_FALSE(File::makeDirectoryRecursive(""));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  EXPECT_FALSE(File::makeDirectoryRecursive("\\"));
  EXPECT_FALSE(File::makeDirectoryRecursive("/"));
#else
  EXPECT_TRUE(File::makeDirectoryRecursive("/"));
#endif
}

TEST(FileTest, CurrentDirectoryChangesAndReportsErrors) {
  auto original = File::currentDirectory();
  ASSERT_TRUE(original);
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporaryDirectory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto restore = finally([&]() { EXPECT_TRUE(File::changeDirectory(*original)); });
  auto child = File::relativeTo(*directory, "目录-é");
  ASSERT_TRUE(File::makeDirectory(child));
  ASSERT_TRUE(File::changeDirectory(child));
  auto current = File::currentDirectory();
  auto expected = File::fullPath(child);
  ASSERT_TRUE(current);
  ASSERT_TRUE(expected);
  EXPECT_EQ(*current, *expected);
  EXPECT_FALSE(File::changeDirectory(File::relativeTo(child, "missing")));
  auto unchanged = File::currentDirectory();
  ASSERT_TRUE(unchanged);
  EXPECT_EQ(*unchanged, *current);
}

TEST(FileTest, DirectoryListingPreservesDotsAndUtf8Names) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto directory = File::temporaryDirectory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  String child = "目录-é";
  String file = "данные.txt";
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(*directory, child)));
  ASSERT_TRUE(File::writeFile(String("contents"), File::relativeTo(*directory, file)));

  auto entries = File::dirList(*directory);
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 2u);
  auto contains = [](auto const& list, String const& name, bool directoryEntry) {
    return std::ranges::any_of(list, [&](auto const& entry) {
      return entry.first == name && entry.second == directoryEntry;
    });
  };
  EXPECT_TRUE(contains(*entries, child, true));
  EXPECT_TRUE(contains(*entries, file, false));

  auto withDots = File::dirList(*directory, false);
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
  auto directory = File::temporaryDirectory(**randomResult);
  ASSERT_TRUE(directory);
  auto cleanup = finally([&]() { removeTree(*directory); });
  auto real = File::relativeTo(*directory, "real");
  ASSERT_TRUE(File::makeDirectory(real));
  auto resolved = File::fullPath(real + "/../real/");
  auto canonical = File::fullPath(real);
  ASSERT_TRUE(resolved);
  ASSERT_TRUE(canonical);
  EXPECT_EQ(*resolved, *canonical);
  EXPECT_FALSE(File::fullPath(File::relativeTo(*directory, "missing")));

  auto link = File::relativeTo(*directory, "link");
  std::error_code error;
  std::filesystem::create_directory_symlink(fsPath(real), fsPath(link), error);
  if (error)
    GTEST_SKIP() << "cannot create directory symlinks here: " << error.message();
  auto linked = File::fullPath(link);
  ASSERT_TRUE(linked);
  EXPECT_EQ(*linked, *canonical);
  EXPECT_TRUE(File::makeDirectoryRecursive(link));
  EXPECT_FALSE(File::makeDirectory(link));
  EXPECT_EQ(File::isDirectory(link), true);

  auto broken = File::relativeTo(*directory, "broken");
  std::filesystem::create_symlink(fsPath(File::relativeTo(*directory, "missing")), fsPath(broken), error);
  ASSERT_FALSE(error) << error.message();
  EXPECT_EQ(File::isSymlink(broken), true);
  EXPECT_EQ(File::exists(broken), false);
  EXPECT_FALSE(File::fullPath(broken));
  EXPECT_TRUE(File::removeDirectoryRecursive(broken));
  EXPECT_EQ(File::isSymlink(broken), false);
}
#endif

TEST(FileTest, RemoveDirectoryRecursiveKeepsSymlinkTargets) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  // The removal used to descend into directory symlinks, so a link pointing
  // outside of the directory deleted the target's contents.
  auto dirResult = File::temporaryDirectory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto outsideResult = File::temporaryDirectory(random);
  ASSERT_TRUE(outsideResult);
  auto outside = std::move(*outsideResult);
  auto finallyGuard = finally([&dir, &outside]() {
    removeTree(dir);
    removeTree(outside);
  });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relativeTo(outside, "real")), ec);
  ASSERT_TRUE(File::writeFile(String("kept"), File::relativeTo(File::relativeTo(outside, "real"), "keep.txt")));
  ASSERT_TRUE(File::writeFile(String("doomed"), File::relativeTo(dir, "file.txt")));
  std::filesystem::create_directory_symlink(
      fsPath(File::relativeTo(outside, "real")), fsPath(File::relativeTo(dir, "link")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();

  EXPECT_EQ(File::isSymlink(File::relativeTo(dir, "link")), true);
  EXPECT_EQ(File::isSymlink(File::relativeTo(dir, "file.txt")), false);

  EXPECT_TRUE(File::removeDirectoryRecursive(dir));
  EXPECT_EQ(File::isDirectory(dir), false);
  // The directory the symlink pointed at has to be untouched.
  EXPECT_EQ(File::isFile(File::relativeTo(File::relativeTo(outside, "real"), "keep.txt")), true);

  // The root itself can be a link, with or without a trailing separator.
  for (std::string_view suffix : {"", "/"}) {
    std::filesystem::create_directory_symlink(fsPath(File::relativeTo(outside, "real")), fsPath(dir), ec);
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_TRUE(File::removeDirectoryRecursive(dir + suffix));
    EXPECT_EQ(File::isSymlink(dir), false);
    EXPECT_EQ(File::isFile(File::relativeTo(File::relativeTo(outside, "real"), "keep.txt")), true);
  }
}

TEST(FileTest, RemoveDirectoryRecursiveHandlesSymlinkLoops) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  // A symlink pointing at one of its own parent directories used to make the
  // removal walk the same tree over and over until it errored out half way
  // through ("remove error: No such file or directory").
  auto dirResult = File::temporaryDirectory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto finallyGuard = finally([&dir]() { removeTree(dir); });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relativeTo(dir, "real")), ec);
  ASSERT_TRUE(File::writeFile(String("doomed"), File::relativeTo(File::relativeTo(dir, "real"), "file.txt")));
  std::filesystem::create_directory_symlink(fsPath(dir), fsPath(File::relativeTo(dir, "loop")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();

  EXPECT_TRUE(File::removeDirectoryRecursive(dir));
  EXPECT_EQ(File::isDirectory(dir), false);
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
  auto dirResult = File::temporaryDirectory(random);
  ASSERT_TRUE(dirResult);
  auto dir = std::move(*dirResult);
  auto cleanup = finally([&]() { removeTree(dir); });
  auto path = File::relativeTo(dir, "pipe");
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
  auto fileResult = File::temporaryFile(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  auto path = file->fileName();
  auto cleanup = finally([&]() { EXPECT_TRUE(File::remove(path)); });
  int descriptor = descriptorForFile(path);
  ASSERT_GE(descriptor, 0);
  ASSERT_EQ(::close(descriptor), 0);

  auto closed = file->close();
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_FALSE(file->isOpen());
  EXPECT_TRUE(file->close());
  // No destructor retry of the invalid or potentially reused descriptor.
  file.reset();
}

TEST(FileTest, DestructorReportsCloseFailureWithoutTerminating) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  auto fileResult = File::temporaryFile(random);
  ASSERT_TRUE(fileResult);
  auto file = std::move(*fileResult);
  auto path = file->fileName();
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

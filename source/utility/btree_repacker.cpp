#include <cstdio>
import star.set;
import star.io_device;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.file;
import star.string;
import star.ordered_set;
import star.option_parser;
import std;
import star.format;
import star.output_proxy;
import star.version;
import star.version_option_parser;
import star.logging;
import star.native_runtime;

import star.byte_array;
using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> repackBTree(int argc, char** argv) {
  Logger logger;
  auto startTime = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& optParse = *parser;
  optParse.setVersionName("Btree Repacker");
  optParse.setSummary("Repacks a Starbound BTree file to shrink its file size");
  optParse.addArgument("input file path", OptionParser::Required, "Path to the BTree to be repacked");
  optParse.addArgument("output filename", OptionParser::Optional, "Output BTree file");

  auto parsed = optParse.commandParseOrDie(argc, argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto inputArgument = opts.arguments.at(0);
  if (!inputArgument)
    return std::unexpected(std::move(inputArgument.error()));
  String bTreePath = inputArgument->get();
  String outputFilename = opts.arguments.get(1, bTreePath + ".repack");

  auto outputDirectory = File::full_path(File::dir_name(outputFilename));
  if (!outputDirectory)
    return std::unexpected(std::move(outputDirectory.error()));
  outputFilename = File::relative_to(*outputDirectory, File::base_name(outputFilename));
  // Open the old database.
  BTreeDatabase db(logger);
  auto inputFile = File::open(bTreePath, IOMode::Read);
  if (!inputFile)
    return std::unexpected(std::move(inputFile.error()));
  auto inputDevice = db.setIODevice(std::move(*inputFile));
  if (!inputDevice)
    return std::unexpected(std::move(inputDevice.error()));
  auto openedInput = db.open();
  if (!openedInput)
    return std::unexpected(std::move(openedInput.error()));

  // Make a new database with the original layout settings.
  BTreeDatabase newDb(logger);
  auto blockSize = newDb.setBlockSize(db.blockSize());
  if (!blockSize)
    return std::unexpected(std::move(blockSize.error()));
  auto contentIdentifier = newDb.setContentIdentifier(db.contentIdentifier());
  if (!contentIdentifier)
    return std::unexpected(std::move(contentIdentifier.error()));
  auto keySize = newDb.setKeySize(db.keySize());
  if (!keySize)
    return std::unexpected(std::move(keySize.error()));
  auto autoCommit = newDb.setAutoCommit(false);
  if (!autoCommit)
    return std::unexpected(std::move(autoCommit.error()));

  auto outputFile = File::open(outputFilename, IOMode::ReadWrite | IOMode::Truncate);
  if (!outputFile)
    return std::unexpected(std::move(outputFile.error()));
  auto outputDevice = newDb.setIODevice(std::move(*outputFile));
  if (!outputDevice)
    return std::unexpected(std::move(outputDevice.error()));
  auto openedOutput = newDb.open();
  if (!openedOutput)
    return std::unexpected(std::move(openedOutput.error()));
  if (auto written = Star::print("Repacking {}...\n", bTreePath); !written)
    return std::unexpected(std::move(written.error()));

  unsigned count = 0, overwritten = 0;
  auto visitor = [&](ByteArray key, ByteArray data) -> std::expected<void, StarException> {
    auto inserted = newDb.insert(key, data);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    if (*inserted)
      ++overwritten;
    ++count;
    return {};
  };
  auto errorHandler = [&](String const& error, ExceptionDiagnostic const& diagnostic) -> std::expected<void, StarException> {
    if (auto written = Star::print("{}: {}\n", error, Star::output_exception(diagnostic, false)); !written)
      return std::unexpected(std::move(written.error()));
    return {};
  };
  auto recovered = db.recoverAll(visitor, errorHandler);
  if (!recovered)
    return std::unexpected(std::move(recovered.error()));

  auto closedInput = db.close(true);
  if (!closedInput)
    return std::unexpected(std::move(closedInput.error()));
  auto committed = newDb.commit();
  if (!committed)
    return std::unexpected(std::move(committed.error()));
  auto closedOutput = newDb.close(true);
  if (!closedOutput)
    return std::unexpected(std::move(closedOutput.error()));

  if (auto written = Star::print("Repacked BTree to {} in {:.6f}s\n({} inserts, {} overwritten)\n", outputFilename, std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count(), count, overwritten); !written)
    return std::unexpected(std::move(written.error()));
  return 0;
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(nativeRuntime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto result = repackBTree(argc, argv);
  if (!result) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

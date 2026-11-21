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
static std::expected<int, StarException> repack_btree(std::span<char const* const> argv) {
  Logger logger;
  auto startTime = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& option_parser = *parser;
  option_parser.set_version_name("Btree Repacker");
  option_parser.set_summary("Repacks a Starbound BTree file to shrink its file size");
  option_parser.add_argument("input file path", OptionParser::Required, "Path to the BTree to be repacked");
  option_parser.add_argument("output filename", OptionParser::Optional, "Output BTree file");

  auto parsed = option_parser.command_parse_or_die(argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto input_argument = opts.arguments.at(0);
  if (!input_argument)
    return std::unexpected(std::move(input_argument.error()));
  auto const& b_tree_path = input_argument->get();
  String output_filename = opts.arguments.size() > 1 ? std::move(opts.arguments[1]) : b_tree_path + ".repack";

  auto outputDirectory = File::full_path(File::dir_name(output_filename));
  if (!outputDirectory)
    return std::unexpected(std::move(outputDirectory.error()));
  output_filename = File::relative_to(*outputDirectory, File::base_name(output_filename));
  // Open the old database.
  BTreeDatabase db(logger);
  auto input_file = File::open(b_tree_path, IOMode::Read);
  if (!input_file)
    return std::unexpected(std::move(input_file.error()));
  auto inputDevice = db.set_io_device(std::move(*input_file));
  if (!inputDevice)
    return std::unexpected(std::move(inputDevice.error()));
  auto openedInput = db.open();
  if (!openedInput)
    return std::unexpected(std::move(openedInput.error()));

  // Make a new database with the original layout settings.
  BTreeDatabase newDb(logger);
  auto blockSize = newDb.set_block_size(db.block_size());
  if (!blockSize)
    return std::unexpected(std::move(blockSize.error()));
  auto contentIdentifier = newDb.set_content_identifier(db.content_identifier());
  if (!contentIdentifier)
    return std::unexpected(std::move(contentIdentifier.error()));
  auto keySize = newDb.set_key_size(db.key_size());
  if (!keySize)
    return std::unexpected(std::move(keySize.error()));
  auto autoCommit = newDb.set_auto_commit(false);
  if (!autoCommit)
    return std::unexpected(std::move(autoCommit.error()));

  auto outputFile = File::open(output_filename, IOMode::ReadWrite | IOMode::Truncate);
  if (!outputFile)
    return std::unexpected(std::move(outputFile.error()));
  auto outputDevice = newDb.set_io_device(std::move(*outputFile));
  if (!outputDevice)
    return std::unexpected(std::move(outputDevice.error()));
  auto openedOutput = newDb.open();
  if (!openedOutput)
    return std::unexpected(std::move(openedOutput.error()));
  if (auto written = Star::print("Repacking {}...\n", b_tree_path); !written)
    return std::unexpected(std::move(written.error()));

  unsigned count = 0, overwritten = 0;
  auto visitor = [&](ByteArray const& key, ByteArray const& data) -> std::expected<void, StarException> {
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
  auto recovered = db.recover_all(visitor, errorHandler);
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

  if (auto written = Star::print("Repacked BTree to {} in {:.6f}s\n({} inserts, {} overwritten)\n", output_filename, std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count(), count, overwritten); !written)
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
  auto result = repack_btree({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

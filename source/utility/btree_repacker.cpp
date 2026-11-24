#include <cstdio>
import star.algorithm;
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
import star.version_option_parser;
import star.logging;
import star.native_runtime;

import star.byte_array;
using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> repack_btree(std::span<String const> argv) {
  Logger logger;
  auto start_time = std::chrono::steady_clock::now();

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

  auto output_directory = File::full_path(File::dir_name(output_filename));
  if (!output_directory)
    return std::unexpected(std::move(output_directory.error()));
  output_filename = File::relative_to(*output_directory, File::base_name(output_filename));
  // Open the old database.
  BTreeDatabase db(logger);
  auto input_file = File::open(b_tree_path, IOMode::Read);
  if (!input_file)
    return std::unexpected(std::move(input_file.error()));
  if (auto attached = db.set_io_device(std::move(*input_file)); !attached)
    return std::unexpected(std::move(attached.error()));
  if (auto opened = db.open(); !opened)
    return std::unexpected(std::move(opened.error()));

  // Never open the requested output for writing: it may be the input, or
  // another name for it. Only the process that creates this stage can clean it up.
  String stage_path = output_filename + ".new";
  auto stage = File::create_new_read_write(stage_path);
  if (!stage)
    return std::unexpected(std::move(stage.error()));
  auto cleanup = make_scope_exit([&] {
    if (auto removed = File::remove(stage_path); !removed)
      EmergencyLog::error("Could not remove repack staging file '{}': {}", stage_path, removed.error().what());
  });

  BTreeDatabase destination(logger, db.content_identifier(), db.key_size());
  if (auto configured = destination.set_block_size(db.block_size()); !configured)
    return std::unexpected(std::move(configured.error()));
  if (auto configured = destination.set_auto_commit(false); !configured)
    return std::unexpected(std::move(configured.error()));
  if (auto attached = destination.set_io_device(std::move(*stage)); !attached)
    return std::unexpected(std::move(attached.error()));
  if (auto opened = destination.open(); !opened)
    return std::unexpected(std::move(opened.error()));
  if (auto written = Star::print("Repacking {}...\n", b_tree_path); !written)
    return std::unexpected(std::move(written.error()));

  std::uint64_t count = 0, overwritten = 0;
  bool destination_failed = false;
  auto visitor = [&](ByteArray const& key, ByteArray const& data) -> std::expected<void, StarException> {
    auto inserted = destination.insert(key, data);
    if (!inserted) {
      destination_failed = true;
      return std::unexpected(std::move(inserted.error()));
    }
    if (*inserted)
      ++overwritten;
    ++count;
    return {};
  };
  auto error_handler = [&](String const& error, ExceptionDiagnostic const& diagnostic) -> std::expected<void, StarException> {
    // recover_all also routes visitor failures here. Only damaged source
    // nodes can be skipped; a failed insert invalidates the entire output.
    if (destination_failed)
      return std::unexpected(DBException("Error writing destination BTree", diagnostic));
    if (auto written = Star::print("{}: {}\n", error, Star::output_exception(diagnostic, false)); !written)
      return std::unexpected(std::move(written.error()));
    return {};
  };
  auto recovered = db.recover_all(visitor, error_handler);
  if (!recovered)
    return std::unexpected(std::move(recovered.error()));

  if (auto closed = db.close(true); !closed)
    return std::unexpected(std::move(closed.error()));
  if (auto committed = destination.commit(); !committed)
    return std::unexpected(std::move(committed.error()));
  if (auto closed = destination.close(true); !closed)
    return std::unexpected(std::move(closed.error()));
  if (auto replaced = File::rename(stage_path, output_filename); !replaced)
    return std::unexpected(std::move(replaced.error()));
  cleanup.release();

  // The replacement has already succeeded. A broken output stream cannot undo it.
  if (auto written = Star::print("Repacked BTree to {} in {:.6f}s\n({} inserts, {} overwritten)\n", output_filename, std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count(), count, overwritten); !written)
    EmergencyLog::error("Could not report successful repack: {}", written.error().what());
  return 0;
}

int main(int argc, char** argv) {
  auto native_runtime = NativeRuntime::create();
  if (!native_runtime) {
    if (auto written = Star::print(*stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(native_runtime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto result = repack_btree(native_arguments(argc, argv));
  if (!result) {
    if (auto written = Star::print(*stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

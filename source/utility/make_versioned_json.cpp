#include <cstdio>
import std;
import star.format;
import star.output_proxy;
import star.file;
import star.json;
import star.string;

import star.versioning_database;
import star.native_runtime;

using namespace Star;

int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    if (auto written = Star::print("Error! Caught exception {}\n", Star::output_exception(ExceptionDiagnostic(nativeRuntime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto run = [&]() -> std::expected<int, StarException> {
    if (argc != 3) {
      if (auto written = Star::print("Usage, {} <versioned_json_json> <versioned_json_binary>\n", argv[0]); !written)
        return std::unexpected(std::move(written.error()));
      return -1;
    }

    auto text = File::read_file_string(String(std::string_view(argv[1])));
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto json = Json::parse(*text);
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto versionedJson = VersionedJson::fromJson(*json);
    if (!versionedJson)
      return std::unexpected(std::move(versionedJson.error()));
    auto written = VersionedJson::write_file(*versionedJson, String(std::string_view(argv[2])));
    if (!written)
      return std::unexpected(std::move(written.error()));
    return 0;
  };
  auto result = run();
  if (!result) {
    if (auto written = Star::print("Error! Caught exception {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

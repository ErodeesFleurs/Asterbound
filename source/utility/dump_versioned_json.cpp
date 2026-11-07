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
    coutf("Error! Caught exception {}\n", outputException(ExceptionDiagnostic(nativeRuntime.error()), true));
    return 1;
  }
  auto run = [&]() -> std::expected<int, StarException> {
    if (argc != 3) {
      coutf("Usage, {} <versioned_json_binary> <versioned_json_json>\n", argv[0]);
      return -1;
    }

    auto versionedJson = VersionedJson::read_file(String(std::string_view(argv[1])));
    if (!versionedJson)
      return std::unexpected(std::move(versionedJson.error()));
    auto json = versionedJson->toJson();
    auto text = json.printJson(2);
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto written = File::write_file(*text, String(std::string_view(argv[2])));
    if (!written)
      return std::unexpected(std::move(written.error()));
    return 0;
  };
  auto result = run();
  if (!result) {
    coutf("Error! Caught exception {}\n", outputException(ExceptionDiagnostic(result.error()), true));
    return 1;
  }
  return *result;
}

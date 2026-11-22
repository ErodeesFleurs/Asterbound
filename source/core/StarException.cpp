import std;
import star.format;
import star.logging;
import star.native_stack;
import star.output_proxy;
import star.text_formatter;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.version;
#endif

namespace Star {

struct StarException::CapturedOriginStack {
  explicit CapturedOriginStack(NativeStack&& frames) : frames(std::move(frames)) {}

  NativeStack frames;
  mutable std::once_flag render_once;
  mutable std::string rendered;
};

std::shared_ptr<StarException::CapturedOriginStack const> StarException::capture_origin_stack() {
  // Capture outside make_shared: allocation machinery is not part of the origin.
  return std::make_shared<CapturedOriginStack const>(NativeStack::capture());
}

std::string const& StarException::rendered_origin_stack(CapturedOriginStack const& stack) {
  std::call_once(stack.render_once, [&stack] {
    TextBuffer output;
    stack.frames.append_to(output);
    stack.rendered = std::move(output).str();
  });
  return stack.rendered;
}

OutputProxy output_exception(ExceptionDiagnostic const& diagnostic, bool full_stacktrace) {
  return OutputProxy([payload = diagnostic.m_payload, full_stacktrace](TextBuffer& output) {
    output.append(payload->rendered(full_stacktrace));
  });
}

void print_stack(std::string_view message) {
  TextBuffer output;
  std::format_to(std::back_inserter(output), "Stack Trace ({})...\n", message);
  NativeStack::capture().append_to(output);
  EmergencyLog::log(LogLevel::Info, output.view());
}

void fatal_error(std::string_view message, bool show_stack_trace) {
  TextBuffer output;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::format_to(std::back_inserter(output), "v{} ({})\nFatal Error: {}\n",
      product_version, source_identifier, message);
#else
  std::format_to(std::back_inserter(output), "Fatal Error: {}", message);
  if (show_stack_trace)
    output.push_back('\n');
#endif
  if (show_stack_trace)
    NativeStack::capture().append_to(output);
  EmergencyLog::log(LogLevel::Error, output.view());
  std::abort();
}

}

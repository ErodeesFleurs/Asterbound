// Loaded at runtime only; linking it into the consumer would mask lifetime bugs.
#ifdef _WIN32
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
FIXTURE_EXPORT int asterbound_fixture_transform(int value) {
  return value * 3 + 1;
}

FIXTURE_EXPORT int asterbound_fixture_exchange(int value) {
  static int state;
  int previous = state;
  state = value;
  return previous;
}

import std;
import star.format;
import star.json;
import star.string;
import star.hash;
import star.list;
import star.image;
import star.renderer;
import star.renderer_opengl;
import star.main_thread_dispatcher;
import star.threaded_renderer;
import star.casting;
import star.type_identity;
import star.ref_ptr;
import star.vector;
import star.matrix3;
import star.rect;

namespace Star {
namespace {
using ResourceId = std::uint64_t;
enum class Operation {
  Begin, End, Resize, Config, EffectConfig, Parameter, Scriptable, EffectTexture,
  Switch, Scissor, Texture, GroupTexture, Group, Buffer, BufferSet, Immediate,
  DrawBuffer, Flush, SizeLimit, MultiTexture, MultiSampling, HDR,
  DeleteTexture, DeleteGroup, DeleteBuffer
};
enum class SlotStatus { Available, Sealed, Consuming, Consumed };

// No intrusive references, borrowed views, callbacks or live frontend objects
// occur in a command. Variable-sized data lives in the slot's reusable arenas.
struct Command {
  Command() = default;
  Command(Command&&) = default;
  Command& operator=(Command&&) = default;
  Command(Command const&) = delete;
  Command& operator=(Command const&) = delete;
  Operation operation = Operation::Begin;
  ResourceId id = 0;
  ResourceId parent = 0;
  std::size_t first = 0;
  std::size_t count = 0;
  unsigned number = 0;
  bool flag = false;
  bool synchronous = false;
  TextureAddressing addressing = TextureAddressing::Clamp;
  TextureFiltering filtering = TextureFiltering::Nearest;
  TextureGroupSize groupSize = TextureGroupSize::Medium;
  Vec2U size;
  std::optional<RectI> rect;
  Mat3F transform = Mat3F::identity();
  std::size_t payload = 0;
};
struct NamedValue {
  String name;
  String effect;
  RenderEffectParameter parameter = 0.0f;
};
struct ShaderConfig {
  String name;
  Json config;
  StringMap<String> shaders;
};
struct OwnedImage {
  OwnedImage() = default;
  explicit OwnedImage(Image const& source) : image(source) {}
  OwnedImage(OwnedImage&&) = default;
  OwnedImage& operator=(OwnedImage&&) = default;
  OwnedImage(OwnedImage const&) = delete;
  OwnedImage& operator=(OwnedImage const&) = delete;
  Image image;
};
struct EffectImage {
  String name;
  OwnedImage pixels;
};
struct CommandSlot {
  std::vector<Command> commands;
  std::size_t commandCount = 0;
  std::vector<DetachedRenderPrimitive> primitives;
  std::vector<RenderVertex> vertices;
  std::vector<NamedValue> namedValues;
  std::size_t namedCount = 0;
  std::vector<Json> configs;
  std::vector<ShaderConfig> shaderConfigs;
  // Creation uploads are released after consumption. Only recurring effect
  // uploads retain pixel storage, independently of the command high-water mark.
  std::vector<OwnedImage> creationImages;
  std::vector<EffectImage> effectImages;
  std::size_t effectImageCount = 0;
  std::uint64_t generation = 0;
  std::atomic<SlotStatus> status{SlotStatus::Available};

  Command& append(Operation operation) {
    if (commandCount == commands.size())
      commands.emplace_back();
    Command& command = commands[commandCount++];
    command.operation = operation;
    command.synchronous = false;
    return command;
  }
  NamedValue& named(Command& command) {
    command.payload = namedCount++;
    if (command.payload == namedValues.size())
      namedValues.emplace_back();
    return namedValues[command.payload];
  }
  EffectImage& effectImage(Command& command) {
    command.payload = effectImageCount++;
    if (command.payload == effectImages.size())
      effectImages.emplace_back();
    return effectImages[command.payload];
  }
  void reset() {
    commandCount = 0;
    primitives.clear();
    vertices.clear();
    namedCount = 0;
    effectImageCount = 0;
    configs.clear();
    shaderConfigs.clear();
    creationImages.clear();
  }
};

struct ProxyTexture;
struct ProxyGroup;
struct ProxyBuffer;
}

struct ThreadedRendererState : std::enable_shared_from_this<ThreadedRendererState> {
  explicit ThreadedRendererState(std::shared_ptr<OpenGlRenderer> renderer, MainThreadDispatcher& main)
    : dispatcher(main), backend(std::move(renderer)), id(backend->rendererId()), screen(backend->screenSize()), scissor(backend->scissorRect()) {
    knownEffects["internal"] = true;
  }

  MainThreadDispatcher& dispatcher;
  // Accessed only by main, including destruction. releaseResources clears it
  // before the last worker-owned proxy can release its shared state.
  std::shared_ptr<OpenGlRenderer> backend;
  std::unordered_map<ResourceId, TexturePtr> textures;
  std::unordered_map<ResourceId, std::shared_ptr<TextureGroup>> groups;
  std::unordered_map<ResourceId, std::shared_ptr<RenderBuffer>> buffers;

  std::array<CommandSlot, 2> slots;
  unsigned producer = 0;
  std::uint64_t nextGeneration = 1;
  ResourceId nextResource = 1;
  std::optional<ThreadedRenderer::FrameTicket> outstanding;
  bool preparing = false;
  std::atomic<bool> active{true};
  String id;
  Vec2U screen;
  std::optional<RectI> scissor;
  std::unordered_map<String, bool, Star::hash<String>, std::equal_to<>> knownEffects;
  List<RenderPrimitive> immediate;
  mutable std::mutex errorMutex;
  std::optional<StarException> deviceError;
  std::atomic<bool> failed{false};

  std::expected<void, StarException> status() const {
    if (failed.load(std::memory_order_acquire)) {
      std::lock_guard lock(errorMutex);
      return std::unexpected(*deviceError);
    }
    if (!active.load(std::memory_order_acquire))
      return std::unexpected(RendererException("Threaded renderer has been released"));
    return {};
  }
  void fail(StarException error) {
    std::lock_guard lock(errorMutex);
    if (!deviceError)
      deviceError.emplace(std::move(error));
    failed.store(true, std::memory_order_release);
  }
  CommandSlot& writing() { return slots[producer]; }
  void destroy(Operation operation, ResourceId resource) {
    // Proxy destruction is a worker operation; nothing here touches a GL owner.
    // After the explicit main-thread release no deletion command is needed.
    if (active.load(std::memory_order_acquire))
      writing().append(operation).id = resource;
  }
  std::expected<void, StarException> capture(List<RenderPrimitive> const& source, Command& command);
  std::expected<void, StarException> captureImmediate() {
    if (immediate.empty())
      return {};
    auto& command = writing().append(Operation::Immediate);
    auto captured = capture(immediate, command);
    if (!captured) {
      --writing().commandCount;
      return captured;
    }
    // Clearing can enqueue texture deletions. They must follow the command
    // which now owns the texture's ID, never precede it.
    immediate.clear();
    return {};
  }
  std::expected<std::reference_wrapper<Command>, StarException> append(Operation operation) {
    auto ready = status();
    if (!ready) return std::unexpected(std::move(ready.error()));
    auto captured = captureImmediate();
    if (!captured) return std::unexpected(std::move(captured.error()));
    return std::ref(writing().append(operation));
  }
  static void copyImage(Image& destination, ImageView source) {
    if (destination.size() != source.size || destination.pixelFormat() != source.format)
      destination.reset(source.size, source.format);
    std::copy_n(source.data.begin(), destination.data().size(), destination.data().begin());
  }
  std::expected<TexturePtr, StarException> texture(Image const& image, TextureAddressing addressing, TextureFiltering filtering, ResourceId group = 0);
  std::expected<void, StarException> setBuffer(ResourceId resource, List<RenderPrimitive> const& primitives) {
    auto command = append(Operation::BufferSet);
    if (!command) return std::unexpected(std::move(command.error()));
    command->get().id = resource;
    auto captured = capture(primitives, command->get());
    if (!captured) --writing().commandCount;
    return captured;
  }
  std::expected<void, StarException> execute(CommandSlot& slot);
  std::expected<void, StarException> synchronize() {
    // Main services update N+1 RPCs after pending N's backend/ImGui/swap.
    for (auto const& slot : slots) {
      auto state = slot.status.load(std::memory_order_acquire);
      if (state == SlotStatus::Sealed || state == SlotStatus::Consuming)
        return std::unexpected(RendererException("Renderer barrier serviced before pending frame consumption"));
    }
    auto result = execute(writing());
    writing().reset();
    return result;
  }
  std::expected<void, StarException> barrier() {
    auto captured = captureImmediate();
    if (!captured) return captured;
    return dispatcher.invoke([this] { return synchronize(); });
  }
  template <typename Function>
  auto query(Function&& function) {
    auto captured = captureImmediate();
    if (!captured) fail(std::move(captured.error()));
    // function is a synchronous stack-owned request with owned string values.
    // Execute the ordered barrier and actual getter in one main-thread visit.
    return dispatcher.invoke([this, &function] {
      auto result = synchronize();
      if (!result) fail(std::move(result.error()));
      return std::invoke(function, *backend);
    });
  }
};

namespace {
struct ProxyTexture final : Texture {
  ProxyTexture(std::shared_ptr<ThreadedRendererState> state, ResourceId id, Vec2U size, TextureAddressing addressing, TextureFiltering filtering)
    : state(std::move(state)), resource(id), dimensions(size), address(addressing), filter(filtering) {}
  ~ProxyTexture() override { state->destroy(Operation::DeleteTexture, resource); }
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<ProxyTexture, Texture>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<ProxyTexture>(); }
  Vec2U size() const override { return dimensions; }
  TextureFiltering filtering() const override { return filter; }
  TextureAddressing addressing() const override { return address; }
  std::shared_ptr<ThreadedRendererState> state;
  ResourceId resource;
  Vec2U dimensions;
  TextureAddressing address;
  TextureFiltering filter;
};
struct ProxyGroup final : TextureGroup {
  ProxyGroup(std::shared_ptr<ThreadedRendererState> state, ResourceId id, TextureFiltering filtering)
    : state(std::move(state)), resource(id), filter(filtering) {}
  ~ProxyGroup() override { state->destroy(Operation::DeleteGroup, resource); }
  TextureFiltering filtering() const override { return filter; }
  std::expected<TexturePtr, StarException> create(Image const& image) override {
    return state->texture(image, TextureAddressing::Clamp, filter, resource);
  }
  std::shared_ptr<ThreadedRendererState> state;
  ResourceId resource;
  TextureFiltering filter;
};
struct ProxyBuffer final : RenderBuffer {
  ProxyBuffer(std::shared_ptr<ThreadedRendererState> state, ResourceId id) : state(std::move(state)), resource(id) {}
  ~ProxyBuffer() override { state->destroy(Operation::DeleteBuffer, resource); }
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<ProxyBuffer, RenderBuffer>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<ProxyBuffer>(); }
  std::expected<void, StarException> set(List<RenderPrimitive>& primitives) override {
    // OpenGlRenderer::GlRenderBuffer::set borrows, but does not consume, this
    // list. A detached version is recorded on every mutation, in call order.
    return state->setBuffer(resource, primitives);
  }
  std::shared_ptr<ThreadedRendererState> state;
  ResourceId resource;
};
}

std::expected<void, StarException> ThreadedRendererState::capture(List<RenderPrimitive> const& source, Command& command) {
  auto& slot = writing();
  auto firstPrimitive = slot.primitives.size();
  auto firstVertex = slot.vertices.size();
  for (auto const& primitive : source) {
    auto result = std::visit([&](auto const& shape) -> std::expected<void, StarException> {
      using Shape = std::remove_cvref_t<decltype(shape)>;
      if constexpr (std::is_same_v<Shape, RenderPoly>) {
        // The backend ignores degenerate polygons before inspecting textures.
        if (shape.vertexes.size() <= 2) {
          slot.primitives.push_back(DetachedRenderPrimitive{0, 0, 0, 2});
          return {};
        }
      }
      ResourceId textureId = 0;
      if (shape.texture) {
        auto proxy = asRef<ProxyTexture>(*shape.texture);
        if (!proxy || proxy->get().state.get() != this)
          return std::unexpected(RendererException("Render primitive uses a texture from another renderer"));
        textureId = proxy->get().resource;
      }
      DetachedRenderPrimitive detached;
      detached.texture = textureId;
      detached.first = slot.vertices.size();
      if constexpr (std::is_same_v<Shape, RenderTriangle>) {
        detached.kind = 0;
        detached.count = 3;
        slot.vertices.insert(slot.vertices.end(), {shape.a, shape.b, shape.c});
      } else if constexpr (std::is_same_v<Shape, RenderQuad>) {
        detached.kind = 1;
        detached.count = 4;
        slot.vertices.insert(slot.vertices.end(), {shape.a, shape.b, shape.c, shape.d});
      } else {
        detached.kind = 2;
        detached.count = shape.vertexes.size();
        slot.vertices.insert(slot.vertices.end(), shape.vertexes.begin(), shape.vertexes.end());
      }
      slot.primitives.push_back(detached);
      return {};
    }, primitive);
    if (!result) {
      slot.primitives.resize(firstPrimitive);
      slot.vertices.resize(firstVertex);
      return result;
    }
  }
  command.first = firstPrimitive;
  command.count = source.size();
  return {};
}

std::expected<TexturePtr, StarException> ThreadedRendererState::texture(Image const& image, TextureAddressing addressing, TextureFiltering filtering, ResourceId group) {
  auto command = append(group ? Operation::GroupTexture : Operation::Texture);
  if (!command) return std::unexpected(std::move(command.error()));
  ResourceId resource = nextResource++;
  command->get().id = resource;
  command->get().parent = group;
  command->get().addressing = addressing;
  command->get().filtering = filtering;
  command->get().payload = writing().creationImages.size();
  writing().creationImages.emplace_back(image);
  return TexturePtr(make_ref<ProxyTexture>(shared_from_this(), resource, image.size(), addressing, filtering));
}

static DetachedRenderGeometry geometryView(ThreadedRendererState const& state, CommandSlot const& slot, Command const& command) {
  return {
      std::span(slot.primitives).subspan(command.first, command.count),
      slot.vertices,
      state.textures};
}

std::expected<void, StarException> ThreadedRendererState::execute(CommandSlot& slot) {
  std::optional<StarException> synchronousError;
  for (std::size_t index = 0; index != slot.commandCount; ++index) {
    auto const& command = slot.commands[index];
    // Deletions remain executable after a failed device operation. All other
    // commands stop: a failed creation must never turn into a bogus GL draw.
    bool deletion = command.operation == Operation::DeleteTexture || command.operation == Operation::DeleteGroup || command.operation == Operation::DeleteBuffer;
    if (!deletion && (synchronousError || failed.load(std::memory_order_acquire))) continue;
    auto result = [&]() -> std::expected<void, StarException> {
      switch (command.operation) {
      case Operation::Begin: return backend->startFrame();
      case Operation::End: return backend->finishFrame();
      case Operation::Resize: return backend->setScreenSize(command.size);
      case Operation::Config: return backend->loadConfig(slot.configs[command.payload]);
      case Operation::EffectConfig: {
        auto const& payload = slot.shaderConfigs[command.payload];
        return backend->loadEffectConfig(payload.name, payload.config, payload.shaders);
      }
      case Operation::Parameter: {
        auto const& payload = slot.namedValues[command.payload];
        return backend->setEffectParameter(payload.name, payload.parameter);
      }
      case Operation::Scriptable: {
        auto const& payload = slot.namedValues[command.payload];
        return backend->setEffectScriptableParameter(payload.effect, payload.name, payload.parameter);
      }
      case Operation::EffectTexture: {
        auto const& payload = slot.effectImages[command.payload];
        return backend->setEffectTexture(payload.name, ImageView(payload.pixels.image));
      }
      case Operation::Switch: {
        auto switched = backend->switchEffectConfig(slot.namedValues[command.payload].name);
        if (!switched) return std::unexpected(std::move(switched.error()));
        return {};
      }
      case Operation::Scissor: return backend->setScissorRect(command.rect);
      case Operation::Texture:
      case Operation::GroupTexture: {
        auto created = [&]() -> std::expected<TexturePtr, StarException> {
          if (command.operation == Operation::Texture)
            return backend->createTexture(slot.creationImages[command.payload].image, command.addressing, command.filtering);
          auto group = groups.find(command.parent);
          if (group == groups.end()) return std::unexpected(RendererException("Missing detached texture group"));
          return group->second->create(slot.creationImages[command.payload].image);
        }();
        if (!created) return std::unexpected(std::move(created.error()));
        textures.emplace(command.id, std::move(*created));
        return {};
      }
      case Operation::Group: {
        auto created = backend->createTextureGroup(command.groupSize, command.filtering);
        if (!created) return std::unexpected(std::move(created.error()));
        groups.emplace(command.id, std::move(*created));
        return {};
      }
      case Operation::Buffer: {
        auto created = backend->createRenderBuffer();
        if (!created) return std::unexpected(std::move(created.error()));
        buffers.emplace(command.id, std::move(*created));
        return {};
      }
      case Operation::BufferSet:
      case Operation::Immediate: {
        auto geometry = geometryView(*this, slot, command);
        if (command.operation == Operation::Immediate)
          return backend->appendImmediate(geometry);
        auto buffer = buffers.find(command.id);
        if (buffer == buffers.end()) return std::unexpected(RendererException("Missing detached render buffer"));
        return backend->setRenderBuffer(buffer->second, geometry);
      }
      case Operation::DrawBuffer: {
        auto buffer = buffers.find(command.id);
        if (buffer == buffers.end()) return std::unexpected(RendererException("Missing detached render buffer"));
        return backend->renderBuffer(buffer->second, command.transform);
      }
      case Operation::Flush: return backend->flush(command.transform);
      case Operation::SizeLimit: backend->setSizeLimitEnabled(command.flag); return {};
      case Operation::MultiTexture: backend->setMultiTexturingEnabled(command.flag); return {};
      case Operation::MultiSampling: return backend->setMultiSampling(command.number);
      case Operation::HDR: return backend->setMainHDR(command.flag);
      case Operation::DeleteTexture: textures.erase(command.id); return {};
      case Operation::DeleteGroup: groups.erase(command.id); return {};
      case Operation::DeleteBuffer: buffers.erase(command.id); return {};
      }
      std::unreachable();
    }();
    if (!result) {
      if (command.synchronous)
        synchronousError.emplace(std::move(result.error()));
      else
        fail(std::move(result.error()));
    }
  }
  if (synchronousError) return std::unexpected(std::move(*synchronousError));
  return status();
}

ThreadedRenderer::ThreadedRenderer(std::shared_ptr<OpenGlRenderer> backend, MainThreadDispatcher& dispatcher)
  : m_state(std::make_shared<ThreadedRendererState>(std::move(backend), dispatcher)) {}
ThreadedRenderer::~ThreadedRenderer() = default;
String ThreadedRenderer::rendererId() const { return m_state->id; }
Vec2U ThreadedRenderer::screenSize() const { return m_state->screen; }
std::optional<RectI> ThreadedRenderer::scissorRect() const { return m_state->scissor; }
List<RenderPrimitive>& ThreadedRenderer::immediatePrimitives() { return m_state->immediate; }
void ThreadedRenderer::render(RenderPrimitive primitive) { m_state->immediate.append(std::move(primitive)); }

std::expected<void, StarException> ThreadedRenderer::startFrame() {
  if (m_state->preparing || m_state->outstanding)
    return std::unexpected(RendererException("A renderer frame is already in flight"));
  auto command = m_state->append(Operation::Begin);
  if (!command) return std::unexpected(std::move(command.error()));
  m_state->preparing = true;
  return {};
}
std::expected<void, StarException> ThreadedRenderer::finishFrame() {
  if (!m_state->preparing || m_state->outstanding)
    return std::unexpected(RendererException("No renderer frame is being prepared"));
  auto command = m_state->append(Operation::End);
  if (!command) return std::unexpected(std::move(command.error()));
  auto& slot = m_state->writing();
  slot.generation = m_state->nextGeneration++;
  m_state->outstanding = FrameTicket{slot.generation, m_state->producer};
  slot.status.store(SlotStatus::Sealed, std::memory_order_release);
  m_state->producer ^= 1;
  m_state->preparing = false;
  return {};
}
ThreadedRenderer::FrameTicket ThreadedRenderer::frameTicket() const {
  return m_state->outstanding.value_or(FrameTicket{});
}
std::expected<void, StarException> ThreadedRenderer::consumeFrame(FrameTicket ticket) {
  if (!m_state->dispatcher.isMainThread())
    return std::unexpected(RendererException("Renderer consumption requires the main thread"));
  if (ticket.slot >= m_state->slots.size())
    return std::unexpected(RendererException("Invalid renderer frame ticket"));
  auto& slot = m_state->slots[ticket.slot];
  auto expected = SlotStatus::Sealed;
  if (!slot.status.compare_exchange_strong(expected, SlotStatus::Consuming, std::memory_order_acq_rel))
    return std::unexpected(RendererException("Renderer frame ticket is not sealed"));
  if (slot.generation != ticket.generation) {
    slot.status.store(SlotStatus::Sealed, std::memory_order_release);
    return std::unexpected(RendererException("Stale renderer frame ticket"));
  }
  auto result = m_state->execute(slot);
  slot.status.store(SlotStatus::Consumed, std::memory_order_release);
  return result;
}
std::expected<void, StarException> ThreadedRenderer::retire(FrameTicket ticket) {
  if (!m_state->outstanding || *m_state->outstanding != ticket)
    return std::unexpected(RendererException("Retiring a renderer frame that is not in flight"));
  auto& slot = m_state->slots[ticket.slot];
  if (slot.status.load(std::memory_order_acquire) != SlotStatus::Consumed)
    return std::unexpected(RendererException("Retiring a renderer frame before consumption"));
  slot.reset();
  slot.status.store(SlotStatus::Available, std::memory_order_release);
  m_state->outstanding.reset();
  return m_state->status();
}
std::expected<void, StarException> ThreadedRenderer::drain() {
  if (!m_state->active.load(std::memory_order_acquire)) {
    // Main has already destroyed every GL owner. Destruction of the remaining
    // Application/frontend handles can only retire CPU-side storage.
    m_state->immediate.clear();
    m_state->writing().reset();
    return {};
  }
  // Complete update-time commands or an abandoned preparation without
  // publishing/presenting a synthetic frame. finishFrame also releases the
  // backend immediate buffer's atlas locks after its final flush.
  auto captured = m_state->captureImmediate();
  if (!captured) {
    m_state->fail(std::move(captured.error()));
    m_state->immediate.clear();
  }
  m_state->writing().append(Operation::End);
  m_state->preparing = false;
  return m_state->barrier();
}
std::expected<void, StarException> ThreadedRenderer::releaseResources() {
  if (!m_state->dispatcher.isMainThread())
    return std::unexpected(RendererException("Renderer release requires the main thread"));
  auto result = m_state->status();
  m_state->active.store(false, std::memory_order_release);
  m_state->buffers.clear();
  m_state->textures.clear();
  m_state->groups.clear();
  m_state->backend.reset();
  return result;
}

std::expected<void, StarException> ThreadedRenderer::setScreenSize(Vec2U size) {
  auto command = m_state->append(Operation::Resize);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().size = size;
  m_state->screen = size;
  return {};
}
std::expected<void, StarException> ThreadedRenderer::loadConfig(Json const& config) {
  auto command = m_state->append(Operation::Config);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().payload = m_state->writing().configs.size();
  m_state->writing().configs.push_back(config);
  command->get().synchronous = true;
  auto result = m_state->barrier();
  // Config reload is an ordered, quiescent boundary for stale effect-upload
  // caches. Do not shrink streaming storage during ordinary frames.
  if (result)
    for (auto& slot : m_state->slots)
      slot.effectImages.clear();
  return result;
}
std::expected<void, StarException> ThreadedRenderer::loadEffectConfig(String const& name, Json const& config, StringMap<String> const& shaders) {
  auto command = m_state->append(Operation::EffectConfig);
  if (!command) return std::unexpected(std::move(command.error()));
  auto& payloads = m_state->writing().shaderConfigs;
  command->get().payload = payloads.size();
  payloads.push_back(ShaderConfig{name, config, shaders});
  command->get().synchronous = true;
  auto result = m_state->barrier();
  if (result) m_state->knownEffects[name] = true;
  return result;
}
std::expected<void, StarException> ThreadedRenderer::setEffectParameter(String const& name, RenderEffectParameter const& value) {
  auto command = m_state->append(Operation::Parameter);
  if (!command) return std::unexpected(std::move(command.error()));
  auto& payload = m_state->writing().named(command->get());
  payload.name = name;
  payload.parameter = value;
  return {};
}
std::expected<void, StarException> ThreadedRenderer::setEffectScriptableParameter(String const& effect, String const& name, RenderEffectParameter const& value) {
  auto command = m_state->append(Operation::Scriptable);
  if (!command) return std::unexpected(std::move(command.error()));
  auto& payload = m_state->writing().named(command->get());
  payload.effect = effect;
  payload.name = name;
  payload.parameter = value;
  // Lua pcall must see authored type errors here, not as a later frame failure.
  command->get().synchronous = true;
  return m_state->barrier();
}
std::optional<RenderEffectParameter> ThreadedRenderer::getEffectScriptableParameter(String const& effect, String const& name) {
  return m_state->query([effect = String(effect), name = String(name)](OpenGlRenderer& backend) {
    return backend.getEffectScriptableParameter(effect, name);
  });
}
std::expected<void, StarException> ThreadedRenderer::setEffectTexture(String const& name, ImageView const& image) {
  auto command = m_state->append(Operation::EffectTexture);
  if (!command) return std::unexpected(std::move(command.error()));
  auto& payload = m_state->writing().effectImage(command->get());
  payload.name = name;
  ThreadedRendererState::copyImage(payload.pixels.image, image);
  return {};
}
std::expected<bool, StarException> ThreadedRenderer::switchEffectConfig(String const& name) {
  auto command = m_state->append(Operation::Switch);
  if (!command) return std::unexpected(std::move(command.error()));
  m_state->writing().named(command->get()).name = name;
  return m_state->knownEffects.find(name) != m_state->knownEffects.end();
}
std::expected<void, StarException> ThreadedRenderer::setScissorRect(std::optional<RectI> const& rect) {
  if (rect == m_state->scissor)
    return m_state->status();
  auto command = m_state->append(Operation::Scissor);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().rect = rect;
  m_state->scissor = rect;
  return {};
}
std::expected<TexturePtr, StarException> ThreadedRenderer::createTexture(Image const& image, TextureAddressing addressing, TextureFiltering filtering) {
  return m_state->texture(image, addressing, filtering);
}
void ThreadedRenderer::setSizeLimitEnabled(bool enabled) {
  auto command = m_state->append(Operation::SizeLimit);
  if (!command) { m_state->fail(std::move(command.error())); return; }
  command->get().flag = enabled;
}
void ThreadedRenderer::setMultiTexturingEnabled(bool enabled) {
  auto command = m_state->append(Operation::MultiTexture);
  if (!command) { m_state->fail(std::move(command.error())); return; }
  command->get().flag = enabled;
}
std::expected<void, StarException> ThreadedRenderer::setMultiSampling(unsigned samples) {
  auto command = m_state->append(Operation::MultiSampling);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().number = samples;
  command->get().synchronous = true;
  return m_state->barrier();
}
std::expected<void, StarException> ThreadedRenderer::setMainHDR(bool enabled) {
  auto command = m_state->append(Operation::HDR);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().flag = enabled;
  command->get().synchronous = true;
  return m_state->barrier();
}
std::expected<std::shared_ptr<TextureGroup>, StarException> ThreadedRenderer::createTextureGroup(TextureGroupSize size, TextureFiltering filtering) {
  auto command = m_state->append(Operation::Group);
  if (!command) return std::unexpected(std::move(command.error()));
  ResourceId resource = m_state->nextResource++;
  command->get().id = resource;
  command->get().groupSize = size;
  command->get().filtering = filtering;
  return std::shared_ptr<TextureGroup>(std::make_shared<ProxyGroup>(m_state, resource, filtering));
}
std::expected<std::shared_ptr<RenderBuffer>, StarException> ThreadedRenderer::createRenderBuffer() {
  auto command = m_state->append(Operation::Buffer);
  if (!command) return std::unexpected(std::move(command.error()));
  ResourceId resource = m_state->nextResource++;
  command->get().id = resource;
  return std::shared_ptr<RenderBuffer>(std::make_shared<ProxyBuffer>(m_state, resource));
}
std::expected<void, StarException> ThreadedRenderer::renderBuffer(std::shared_ptr<RenderBuffer> const& buffer, Mat3F const& transform) {
  if (!buffer) return std::unexpected(RendererException("Cannot render a null render buffer"));
  auto proxy = asRef<ProxyBuffer>(*buffer);
  if (!proxy || proxy->get().state != m_state)
    return std::unexpected(RendererException("Render buffer belongs to another renderer"));
  auto command = m_state->append(Operation::DrawBuffer);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().id = proxy->get().resource;
  command->get().transform = transform;
  return {};
}
std::expected<void, StarException> ThreadedRenderer::flush(Mat3F const& transform) {
  auto command = m_state->append(Operation::Flush);
  if (!command) return std::unexpected(std::move(command.error()));
  command->get().transform = transform;
  return {};
}
}

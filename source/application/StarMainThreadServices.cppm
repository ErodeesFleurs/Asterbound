module;
import std;
import star.format;
import star.string;
import star.json;
import star.list;
import star.set;
import star.host_address;
import star.rpc_promise;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.main_thread_dispatcher;

namespace Star {
namespace {

class MainStatisticsService final : public StatisticsService {
public:
  MainStatisticsService(MainThreadDispatcher& dispatcher, std::shared_ptr<StatisticsService> service)
    : m_dispatcher(dispatcher), m_service(std::move(service)) {}

  bool initialized() const override {
    return m_dispatcher.invoke([&] { return m_service->initialized(); });
  }
  std::optional<StarException> error() const override {
    return m_dispatcher.invoke([&] { return m_service->error(); });
  }
  std::expected<bool, StarException> setStat(String const& name, String const& type, Json const& value) override {
    return m_dispatcher.invoke([&] { return m_service->setStat(name, type, value); });
  }
  Json getStat(String const& name, String const& type, Json def) const override {
    return m_dispatcher.invoke([&] { return m_service->getStat(name, type, std::move(def)); });
  }
  bool reportEvent(String const& name, Json const& fields) override {
    return m_dispatcher.invoke([&] { return m_service->reportEvent(name, fields); });
  }
  std::expected<bool, StarException> unlockAchievement(String const& name) override {
    return m_dispatcher.invoke([&] { return m_service->unlockAchievement(name); });
  }
  StringSet achievementsUnlocked() const override {
    return m_dispatcher.invoke([&] { return m_service->achievementsUnlocked(); });
  }
  std::expected<void, StarException> refresh() override {
    return m_dispatcher.invoke([&] { return m_service->refresh(); });
  }
  std::expected<void, StarException> flush() override {
    return m_dispatcher.invoke([&] { return m_service->flush(); });
  }
  bool reset() override {
    return m_dispatcher.invoke([&] { return m_service->reset(); });
  }

private:
  MainThreadDispatcher& m_dispatcher;
  std::shared_ptr<StatisticsService> m_service;
};

class MainP2PNetworkingService final : public P2PNetworkingService {
public:
  MainP2PNetworkingService(MainThreadDispatcher& dispatcher, std::shared_ptr<P2PNetworkingService> service)
    : m_dispatcher(dispatcher), m_service(std::move(service)) {}

  std::expected<void, StarException> setJoinUnavailable() override {
    return m_dispatcher.invoke([&] { return m_service->setJoinUnavailable(); });
  }
  std::expected<void, StarException> setJoinLocal(std::uint32_t capacity) override {
    return m_dispatcher.invoke([&] { return m_service->setJoinLocal(capacity); });
  }
  std::expected<void, StarException> setJoinRemote(HostAddressWithPort location) override {
    return m_dispatcher.invoke([&] { return m_service->setJoinRemote(std::move(location)); });
  }
  std::expected<void, StarException> setActivityData(std::string_view title, std::string_view details,
      std::int64_t startTime, std::optional<std::pair<std::uint16_t, std::uint16_t>> players) override {
    return m_dispatcher.invoke([&] { return m_service->setActivityData(title, details, startTime, players); });
  }
  std::variant<std::monostate, P2PNetworkingPeerId, HostAddressWithPort> pullPendingJoin() override {
    return m_dispatcher.invoke([&] { return m_service->pullPendingJoin(); });
  }
  std::optional<std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>>> pullJoinRequest() override {
    return m_dispatcher.invoke([&] { return m_service->pullJoinRequest(); });
  }
  void setAcceptingP2PConnections(bool accepting) override {
    m_dispatcher.invoke([&] { m_service->setAcceptingP2PConnections(accepting); });
  }
  List<std::unique_ptr<P2PSocket>> acceptP2PConnections() override {
    // Socket ownership is transferable: the service contract explicitly permits
    // a socket and its service to be used on different threads.
    return m_dispatcher.invoke([&] { return m_service->acceptP2PConnections(); });
  }
  std::expected<void, StarException> update() override {
    return m_dispatcher.invoke([&] { return m_service->update(); });
  }
  std::expected<std::unique_ptr<P2PSocket>, StarException> connectToPeer(P2PNetworkingPeerId peerId) override {
    return m_dispatcher.invoke([&] { return m_service->connectToPeer(std::move(peerId)); });
  }

private:
  MainThreadDispatcher& m_dispatcher;
  std::shared_ptr<P2PNetworkingService> m_service;
};

class MainUserGeneratedContentService final : public UserGeneratedContentService {
public:
  MainUserGeneratedContentService(MainThreadDispatcher& dispatcher, std::shared_ptr<UserGeneratedContentService> service)
    : m_dispatcher(dispatcher), m_service(std::move(service)) {}

  StringList subscribedContentIds() const override {
    return m_dispatcher.invoke([&] { return m_service->subscribedContentIds(); });
  }
  std::expected<std::optional<String>, StarException> contentDownloadDirectory(String const& contentId) const override {
    return m_dispatcher.invoke([&] { return m_service->contentDownloadDirectory(contentId); });
  }
  std::expected<UGCState, StarException> triggerContentDownload() override {
    return m_dispatcher.invoke([&] { return m_service->triggerContentDownload(); });
  }

private:
  MainThreadDispatcher& m_dispatcher;
  std::shared_ptr<UserGeneratedContentService> m_service;
};

class MainDesktopService final : public DesktopService {
public:
  MainDesktopService(MainThreadDispatcher& dispatcher, std::shared_ptr<DesktopService> service)
    : m_dispatcher(dispatcher), m_service(std::move(service)) {}

  void openUrl(String const& url) override {
    m_dispatcher.invoke([&] { m_service->openUrl(url); });
  }

private:
  MainThreadDispatcher& m_dispatcher;
  std::shared_ptr<DesktopService> m_service;
};

}

// Keep this container on main until the application and its sockets have shut
// down. Its retained proxies also make final SDK service releases main-owned.
struct MainThreadServices {
  std::shared_ptr<StatisticsService> statistics;
  std::shared_ptr<P2PNetworkingService> p2p;
  std::shared_ptr<UserGeneratedContentService> ugc;
  std::shared_ptr<DesktopService> desktop;

  static MainThreadServices create(MainThreadDispatcher& dispatcher,
      std::shared_ptr<StatisticsService> statistics, std::shared_ptr<P2PNetworkingService> p2p,
      std::shared_ptr<UserGeneratedContentService> ugc, std::shared_ptr<DesktopService> desktop) {
    MainThreadServices result;
    if (statistics)
      result.statistics = std::make_shared<MainStatisticsService>(dispatcher, std::move(statistics));
    if (p2p)
      result.p2p = std::make_shared<MainP2PNetworkingService>(dispatcher, std::move(p2p));
    if (ugc)
      result.ugc = std::make_shared<MainUserGeneratedContentService>(dispatcher, std::move(ugc));
    if (desktop)
      result.desktop = std::make_shared<MainDesktopService>(dispatcher, std::move(desktop));
    return result;
  }
};

}

export module star.main_thread_services;
export namespace Star {
  using ::Star::MainThreadServices;
}

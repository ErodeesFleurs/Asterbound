module;
import std;
import star.type_identity;
import star.format;
import star.string;
import star.gui_context;
import star.pane;

export module star.http_trust_dialog;

export namespace Star {

class HttpTrustDialog final : public Pane {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<HttpTrustDialog, Pane>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<HttpTrustDialog>(); }

  using Reply = std::function<std::expected<void, StarException>(bool allow, bool remember)>;

  HttpTrustDialog(ConstructionKey, GuiContext& gui_context);
  [[nodiscard]] static std::expected<std::shared_ptr<HttpTrustDialog>, StarException> create(GuiContext& gui_context);
  [[nodiscard]] std::expected<void, StarException> display_request(String const& domain, Reply reply);

  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  [[nodiscard]] std::expected<void, StarException> respond(bool allow);
  Reply reply_;
};

}

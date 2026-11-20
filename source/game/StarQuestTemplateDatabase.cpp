import std;
import star.json;
import star.json_extra;
import star.bi_map;
import star.list;
import star.string;
import star.format;
import star.logging;
import star.item_descriptor;
import star.assets;
import star.quest_template_database;
namespace Star {
std::expected<std::shared_ptr<QuestTemplate>, StarException> QuestTemplate::create(Json const& config) {
  auto result = std::shared_ptr<QuestTemplate>(new QuestTemplate());
  result->config = config;
  auto id = config.get_string("id");
  if (!id) return std::unexpected(std::move(id.error()));
  result->templateId = std::move(*id);
  auto title = config.get_string("title", "");
  if (!title) return std::unexpected(std::move(title.error()));
  result->title = std::move(*title);
  auto text = config.get_string("text", "");
  if (!text) return std::unexpected(std::move(text.error()));
  result->text = std::move(*text);
  auto completion = config.get_string("completionText", "");
  if (!completion) return std::unexpected(std::move(completion.error()));
  result->completionText = std::move(*completion);
  auto failure = config.get_string("failureText", "");
  if (!failure) return std::unexpected(std::move(failure.error()));
  result->failureText = std::move(*failure);
  auto parameters = config.get_object("parameters", {});
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  for (auto const& entry : *parameters) {
    auto type = entry.second.get_string("type");
    if (!type) return std::unexpected(std::move(type.error()));
    result->parameterTypes[entry.first] = std::move(*type);
    auto example = entry.second.find("example");
    if (!example)
      return std::unexpected(std::move(example.error()));
    if (*example)
      result->parameterExamples[entry.first] = (**example).get();
  }
  auto rangeJson = config.get("moneyRange", JsonArray{0, 0});
  if (!rangeJson) return std::unexpected(std::move(rangeJson.error()));
  auto range = json_to_vec2u(*rangeJson);
  if (!range) return std::unexpected(std::move(range.error()));
  result->moneyRange = *range;
  auto rewards = config.find("rewards");
  if (!rewards) return std::unexpected(std::move(rewards.error()));
  if (*rewards && !(**rewards).get().is_null()) {
    auto options = (**rewards).get().iterate_array();
    if (!options)
      return std::unexpected(std::move(options.error()));
    for (auto const& option : *options) {
      auto items = json_to_list<ItemDescriptor>(option, [](Json const& entry) { return ItemDescriptor::create(entry); });
      if (!items)
        return std::unexpected(std::move(items.error()));
      result->rewards.push_back(std::move(*items));
    }
  }
  auto rewardParameters = config.get("rewardParameters", JsonArray{});
  if (!rewardParameters) return std::unexpected(std::move(rewardParameters.error()));
  auto rewardNames = json_to_string_list(*rewardParameters);
  if (!rewardNames) return std::unexpected(std::move(rewardNames.error()));
  result->rewardParameters = std::move(*rewardNames);
  auto cinema = config.opt_string("completionCinema");
  if (!cinema) return std::unexpected(std::move(cinema.error()));
  result->completionCinema = std::move(*cinema);
  auto canBeAbandoned = config.get_bool("canBeAbandoned", true);
  if (!canBeAbandoned) return std::unexpected(std::move(canBeAbandoned.error()));
  result->canBeAbandoned = *canBeAbandoned;
  auto ephemeral = config.get_bool("ephemeral", false);
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  result->ephemeral = *ephemeral;
  auto showInLog = config.get_bool("showInLog", true);
  if (!showInLog) return std::unexpected(std::move(showInLog.error()));
  result->showInLog = *showInLog;
  auto showAcceptDialog = config.get_bool("showAcceptDialog", true);
  if (!showAcceptDialog) return std::unexpected(std::move(showAcceptDialog.error()));
  result->showAcceptDialog = *showAcceptDialog;
  auto showCompleteDialog = config.get_bool("showCompleteDialog", true);
  if (!showCompleteDialog) return std::unexpected(std::move(showCompleteDialog.error()));
  result->showCompleteDialog = *showCompleteDialog;
  auto showFailDialog = config.get_bool("showFailDialog", true);
  if (!showFailDialog) return std::unexpected(std::move(showFailDialog.error()));
  result->showFailDialog = *showFailDialog;
  auto mainQuest = config.get_bool("mainQuest", false);
  if (!mainQuest) return std::unexpected(std::move(mainQuest.error()));
  result->mainQuest = *mainQuest;
  auto hideCrossServer = config.get_bool("hideCrossServer", false);
  if (!hideCrossServer) return std::unexpected(std::move(hideCrossServer.error()));
  result->hideCrossServer = *hideCrossServer;
  auto giver = config.get_string("questGiverIndicator", "questgiver");
  if (!giver) return std::unexpected(std::move(giver.error()));
  result->questGiverIndicator = std::move(*giver);
  auto receiver = config.get_string("questReceiverIndicator", "questreceiver");
  if (!receiver) return std::unexpected(std::move(receiver.error()));
  result->questReceiverIndicator = std::move(*receiver);
  auto prerequisites = config.get("prerequisites", JsonArray{});
  if (!prerequisites) return std::unexpected(std::move(prerequisites.error()));
  auto prerequisiteNames = json_to_string_list(*prerequisites);
  if (!prerequisiteNames) return std::unexpected(std::move(prerequisiteNames.error()));
  result->prerequisiteQuests = std::move(*prerequisiteNames);
  auto level = config.opt_uint("requiredShipLevel");
  if (!level) return std::unexpected(std::move(level.error()));
  result->requiredShipLevel = *level;
  auto items = config.find("requiredItems");
  if (!items) return std::unexpected(std::move(items.error()));
  if (*items && !(**items).get().is_null()) {
    auto descriptors =
      json_to_list<ItemDescriptor>((**items).get(), [](Json const& entry) { return ItemDescriptor::create(entry); });
    if (!descriptors)
      return std::unexpected(std::move(descriptors.error()));
    result->requiredItems = std::move(*descriptors);
  }
  auto delta = config.get_uint("updateDelta", 10);
  if (!delta) return std::unexpected(std::move(delta.error()));
  result->updateDelta = *delta;
  auto script = config.opt_string("script");
  if (!script) return std::unexpected(std::move(script.error()));
  result->script = std::move(*script);
  auto scriptConfig = config.get_object("scriptConfig", {});
  if (!scriptConfig) return std::unexpected(std::move(scriptConfig.error()));
  result->scriptConfig = std::move(*scriptConfig);
  auto gui = config.opt("guiConfigs");
  if (!gui) return std::unexpected(std::move(gui.error()));
  if (*gui) {
    auto newQuest = (**gui).opt_string("newQuest");
    if (!newQuest) return std::unexpected(std::move(newQuest.error()));
    result->newQuestGuiConfig = std::move(*newQuest);
    auto complete = (**gui).opt_string("questComplete");
    if (!complete) return std::unexpected(std::move(complete.error()));
    result->questCompleteGuiConfig = std::move(*complete);
    auto failed = (**gui).opt_string("questFailed");
    if (!failed) return std::unexpected(std::move(failed.error()));
    result->questFailedGuiConfig = std::move(*failed);
  }
  return result;
}
std::expected<std::shared_ptr<QuestTemplateDatabase>, StarException> QuestTemplateDatabase::create(std::shared_ptr<Assets const> assets, Logger& logger) {
  auto database = std::shared_ptr<QuestTemplateDatabase>(new QuestTemplateDatabase(logger));
  auto files = assets->scanExtension("questtemplate");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued = assets->queueJsons(files->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto loaded = [&]() -> std::expected<void, StarException> {
    auto config = assets->json(file.string());
    if (!config) return std::unexpected(std::move(config.error()));
    auto quest = QuestTemplate::create(*config);
    if (!quest) return std::unexpected(std::move(quest.error()));
    if (!database->m_templates.insert((*quest)->templateId, *quest).second)
      return std::unexpected(StarException(Star::format("Duplicate quest template '{}'", (*quest)->templateId)));
    return {};
    }();
    if (!loaded)
      return std::unexpected(assets->withAssetContext(std::move(loaded.error()), file.string(), "Loading quest template"));
  }
  return database;
}
List<String> QuestTemplateDatabase::allQuestTemplateIds() const { return m_templates.keys(); }
std::expected<std::shared_ptr<QuestTemplate>, StarException> QuestTemplateDatabase::questTemplate(String const& id) const {
  if (auto result = m_templates.find_ref(id))
    return result->get();
  auto logged = m_logger.error("No quest template found for id '{}'", id);
  if (!logged) return std::unexpected(std::move(logged.error()));
  return std::shared_ptr<QuestTemplate>();
}
}

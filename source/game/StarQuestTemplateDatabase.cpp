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
  auto id = config.getString("id");
  if (!id) return std::unexpected(std::move(id.error()));
  result->templateId = std::move(*id);
  auto title = config.getString("title", "");
  if (!title) return std::unexpected(std::move(title.error()));
  result->title = std::move(*title);
  auto text = config.getString("text", "");
  if (!text) return std::unexpected(std::move(text.error()));
  result->text = std::move(*text);
  auto completion = config.getString("completionText", "");
  if (!completion) return std::unexpected(std::move(completion.error()));
  result->completionText = std::move(*completion);
  auto failure = config.getString("failureText", "");
  if (!failure) return std::unexpected(std::move(failure.error()));
  result->failureText = std::move(*failure);
  auto parameters = config.getObject("parameters", {});
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  for (auto const& entry : *parameters) {
    auto type = entry.second.getString("type");
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
  auto range = jsonToVec2U(*rangeJson);
  if (!range) return std::unexpected(std::move(range.error()));
  result->moneyRange = *range;
  auto rewards = config.find("rewards");
  if (!rewards) return std::unexpected(std::move(rewards.error()));
  if (*rewards && !(**rewards).get().isNull()) {
    auto options = (**rewards).get().iterateArray();
    if (!options)
      return std::unexpected(std::move(options.error()));
    for (auto const& option : *options) {
      auto items = jsonToList<ItemDescriptor>(option, [](Json const& entry) { return ItemDescriptor::create(entry); });
      if (!items)
        return std::unexpected(std::move(items.error()));
      result->rewards.append(std::move(*items));
    }
  }
  auto rewardParameters = config.get("rewardParameters", JsonArray{});
  if (!rewardParameters) return std::unexpected(std::move(rewardParameters.error()));
  auto rewardNames = jsonToStringList(*rewardParameters);
  if (!rewardNames) return std::unexpected(std::move(rewardNames.error()));
  result->rewardParameters = std::move(*rewardNames);
  auto cinema = config.optString("completionCinema");
  if (!cinema) return std::unexpected(std::move(cinema.error()));
  result->completionCinema = std::move(*cinema);
  auto canBeAbandoned = config.getBool("canBeAbandoned", true);
  if (!canBeAbandoned) return std::unexpected(std::move(canBeAbandoned.error()));
  result->canBeAbandoned = *canBeAbandoned;
  auto ephemeral = config.getBool("ephemeral", false);
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  result->ephemeral = *ephemeral;
  auto showInLog = config.getBool("showInLog", true);
  if (!showInLog) return std::unexpected(std::move(showInLog.error()));
  result->showInLog = *showInLog;
  auto showAcceptDialog = config.getBool("showAcceptDialog", true);
  if (!showAcceptDialog) return std::unexpected(std::move(showAcceptDialog.error()));
  result->showAcceptDialog = *showAcceptDialog;
  auto showCompleteDialog = config.getBool("showCompleteDialog", true);
  if (!showCompleteDialog) return std::unexpected(std::move(showCompleteDialog.error()));
  result->showCompleteDialog = *showCompleteDialog;
  auto showFailDialog = config.getBool("showFailDialog", true);
  if (!showFailDialog) return std::unexpected(std::move(showFailDialog.error()));
  result->showFailDialog = *showFailDialog;
  auto mainQuest = config.getBool("mainQuest", false);
  if (!mainQuest) return std::unexpected(std::move(mainQuest.error()));
  result->mainQuest = *mainQuest;
  auto hideCrossServer = config.getBool("hideCrossServer", false);
  if (!hideCrossServer) return std::unexpected(std::move(hideCrossServer.error()));
  result->hideCrossServer = *hideCrossServer;
  auto giver = config.getString("questGiverIndicator", "questgiver");
  if (!giver) return std::unexpected(std::move(giver.error()));
  result->questGiverIndicator = std::move(*giver);
  auto receiver = config.getString("questReceiverIndicator", "questreceiver");
  if (!receiver) return std::unexpected(std::move(receiver.error()));
  result->questReceiverIndicator = std::move(*receiver);
  auto prerequisites = config.get("prerequisites", JsonArray{});
  if (!prerequisites) return std::unexpected(std::move(prerequisites.error()));
  auto prerequisiteNames = jsonToStringList(*prerequisites);
  if (!prerequisiteNames) return std::unexpected(std::move(prerequisiteNames.error()));
  result->prerequisiteQuests = std::move(*prerequisiteNames);
  auto level = config.optUInt("requiredShipLevel");
  if (!level) return std::unexpected(std::move(level.error()));
  result->requiredShipLevel = *level;
  auto items = config.find("requiredItems");
  if (!items) return std::unexpected(std::move(items.error()));
  if (*items && !(**items).get().isNull()) {
    auto descriptors =
      jsonToList<ItemDescriptor>((**items).get(), [](Json const& entry) { return ItemDescriptor::create(entry); });
    if (!descriptors)
      return std::unexpected(std::move(descriptors.error()));
    result->requiredItems = std::move(*descriptors);
  }
  auto delta = config.getUInt("updateDelta", 10);
  if (!delta) return std::unexpected(std::move(delta.error()));
  result->updateDelta = *delta;
  auto script = config.optString("script");
  if (!script) return std::unexpected(std::move(script.error()));
  result->script = std::move(*script);
  auto scriptConfig = config.getObject("scriptConfig", {});
  if (!scriptConfig) return std::unexpected(std::move(scriptConfig.error()));
  result->scriptConfig = std::move(*scriptConfig);
  auto gui = config.opt("guiConfigs");
  if (!gui) return std::unexpected(std::move(gui.error()));
  if (*gui) {
    auto newQuest = (**gui).optString("newQuest");
    if (!newQuest) return std::unexpected(std::move(newQuest.error()));
    result->newQuestGuiConfig = std::move(*newQuest);
    auto complete = (**gui).optString("questComplete");
    if (!complete) return std::unexpected(std::move(complete.error()));
    result->questCompleteGuiConfig = std::move(*complete);
    auto failed = (**gui).optString("questFailed");
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
      return std::unexpected(StarException(strf("Duplicate quest template '{}'", (*quest)->templateId)));
    return {};
    }();
    if (!loaded)
      return std::unexpected(assets->withAssetContext(std::move(loaded.error()), file.string(), "Loading quest template"));
  }
  return database;
}
List<String> QuestTemplateDatabase::allQuestTemplateIds() const { return m_templates.keys(); }
std::expected<std::shared_ptr<QuestTemplate>, StarException> QuestTemplateDatabase::questTemplate(String const& id) const {
  if (auto result = m_templates.maybeRef(id)) return result->get();
  auto logged = m_logger.error("No quest template found for id '{}'", id);
  if (!logged) return std::unexpected(std::move(logged.error()));
  return std::shared_ptr<QuestTemplate>();
}
}

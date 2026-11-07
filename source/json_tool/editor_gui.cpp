#include <QApplication>
#include <QErrorMessage>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>

import std;
import star.format;
import star.string;
import star.list;
import star.json;

import star.file;
import star.json_tool;
import star.json_editor;
import star.formatted_json;
import star.json_path;
import star.logging;

using namespace Star;

int const ImagePreviewWidth = 300;
int const ImagePreviewHeight = 600;

JsonEditor::JsonEditor(std::shared_ptr<::Star::JsonPath::Path> const& path, Options const& options, List<String> const& files)
  : QMainWindow(), m_path(path), m_editFormat(options.editFormat), m_options(options), m_files(files), m_fileIndex(0) {
  auto& background = *new QWidget(this);
  background.setObjectName("background");
  setCentralWidget(&background);
  setWindowTitle("Json Editor");
  resize(1280, 720);

  auto& layout = *new QGridLayout(&background);

  QFont font("Monospace");
  font.setStyleHint(QFont::StyleHint::Monospace);

  m_jsonDocument = std::make_unique<QTextDocument>("Hello world");
  m_jsonDocument->setDefaultFont(font);

  m_statusLabel = std::make_unique<QLabel>(&background);
  layout.addWidget(m_statusLabel.get(), 0, 0, 1, 5);

  m_jsonPreview = std::make_unique<QTextEdit>(this);
  m_jsonPreview->setReadOnly(true);
  m_jsonPreview->setDocument(m_jsonDocument.get());
  layout.addWidget(m_jsonPreview.get(), 1, 0, 1, 5);

  m_backButton = std::make_unique<QPushButton>(&background);
  m_backButton->setText("« Back");
  layout.addWidget(m_backButton.get(), 2, 0);
  connect(m_backButton.get(), &QPushButton::pressed, this, &JsonEditor::back);

  m_pathLabel = std::make_unique<QLabel>(&background);
  layout.addWidget(m_pathLabel.get(), 2, 1);

  m_imageLabel = std::make_unique<QLabel>(&background);
  m_imageLabel->setMaximumSize(ImagePreviewWidth, ImagePreviewHeight);
  m_imageLabel->setMinimumSize(ImagePreviewWidth, ImagePreviewHeight);
  m_imageLabel->setAlignment(Qt::AlignCenter);
  if (!m_options.editorImages.empty())
    layout.addWidget(m_imageLabel.get(), 1, 5);

  m_valueEditor = std::make_unique<QLineEdit>(&background);
  m_valueEditor->setFont(font);
  layout.addWidget(m_valueEditor.get(), 2, 2);
  connect(m_valueEditor.get(), &QLineEdit::returnPressed, this, &JsonEditor::next);
  connect(m_valueEditor.get(), &QLineEdit::textChanged, this, &JsonEditor::updatePreview);

  m_nextButton = std::make_unique<QPushButton>(&background);
  m_nextButton->setText("Next »");
  m_nextButton->setDefault(true);
  layout.addWidget(m_nextButton.get(), 2, 3);
  connect(m_nextButton.get(), &QPushButton::pressed, this, &JsonEditor::next);

  m_errorDialog = std::make_unique<QErrorMessage>(this);
  m_errorDialog->setModal(true);

  displayCurrentFile();
}

void JsonEditor::next() {
  if (!m_valueEditor->isEnabled() || saveChanges()) {
    ++m_fileIndex;
    if (m_fileIndex >= m_files.size()) {
      close();
      return;
    }

    displayCurrentFile();
  }
}

void JsonEditor::back() {
  if (m_fileIndex == 0)
    return;

  --m_fileIndex;
  displayCurrentFile();
}

void JsonEditor::updatePreview(QString const& valueStr) {
  if (!m_valueEditor->isEnabled())
    return;
  auto value = m_editFormat->toJson(valueStr.toStdString());
  if (!value) {
    if (!value.error().is<JsonException>() && !value.error().is<JsonParsingException>())
      m_errorDialog->showMessage(value.error().what());
    return;
  }
  auto preview = addOrSet(false, m_path, m_currentJson, m_options.insertLocation, *value);
  if (!preview) {
    if (!preview.error().is<JsonException>() && !preview.error().is<JsonParsingException>())
      m_errorDialog->showMessage(preview.error().what());
    return;
  }
  auto repr = preview->repr();
  if (!repr) {
    m_errorDialog->showMessage(repr.error().what());
    return;
  }
  m_jsonDocument->setPlainText(repr->utf8Ptr());
}

bool JsonEditor::saveChanges() {
  auto value = m_editFormat->toJson(m_valueEditor->text().toStdString());
  if (!value) {
    m_errorDialog->showMessage(value.error().what());
    return false;
  }
  auto updated = addOrSet(false, m_path, m_currentJson, m_options.insertLocation, *value);
  if (!updated) {
    m_errorDialog->showMessage(updated.error().what());
    return false;
  }
  auto repr = reprWithLineEnding(*updated);
  if (!repr) {
    m_errorDialog->showMessage(repr.error().what());
    return false;
  }
  auto saved = File::overwrite_file_with_rename(*repr, m_files[m_fileIndex]);
  if (!saved) {
    m_errorDialog->showMessage(saved.error().what());
    return false;
  }
  m_currentJson = std::move(*updated);
  return true;
}

void JsonEditor::displayCurrentFile() {
  String const& file = m_files[m_fileIndex];

  std::size_t progress = (m_fileIndex + 1) * 100 / m_files.size();
  String status = strf("Editing file {}/{} ({}%):    {}", m_fileIndex + 1, m_files.size(), progress, file);
  m_statusLabel->setText(status.utf8Ptr());

  m_backButton->setEnabled(m_fileIndex != 0);
  m_nextButton->setText(m_fileIndex == m_files.size() - 1 ? "Done" : "Next »");

  m_pathLabel->setText(m_path->path().utf8Ptr());

  m_imageLabel->setText("No preview");
  m_jsonDocument->setPlainText("");
  m_valueEditor->setEnabled(false);
  m_valueEditor->setText("");

  auto load = [&]() -> std::expected<void, StarException> {
    auto contents = File::read_file_string(file);
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto parsed = FormattedJson::parse(*contents);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    auto repr = parsed->repr();
    if (!repr)
      return std::unexpected(std::move(repr.error()));
    m_currentJson = std::move(*parsed);
    m_jsonDocument->setPlainText(repr->utf8Ptr());
    updateValueEditor();
    updateImagePreview();
    return {};
  };
  auto loaded = load();
  if (!loaded)
    m_errorDialog->showMessage(loaded.error().what());

  m_jsonPreview->moveCursor(QTextCursor::Start);

  m_valueEditor->selectAll();
  m_valueEditor->setFocus(Qt::FocusReason::OtherFocusReason);
}

void JsonEditor::updateValueEditor() {
  auto value = m_path->get(m_currentJson);
  if (!value) {
    if (!value.error().is<JsonPath::TraversalException>()) {
      m_errorDialog->showMessage(value.error().what());
      return;
    }
    value = m_editFormat->getDefault();
    if (!value) {
      m_errorDialog->showMessage(value.error().what());
      return;
    }
  }
  auto text = m_editFormat->fromJson(*value);
  if (!text) {
    m_errorDialog->showMessage(text.error().what());
    if (!text.error().is<JsonException>())
      return;
    auto fallback = m_editFormat->getDefault();
    if (!fallback) {
      m_errorDialog->showMessage(fallback.error().what());
      return;
    }
    text = m_editFormat->fromJson(*fallback);
    if (!text) {
      m_errorDialog->showMessage(text.error().what());
      return;
    }
  }
  m_valueEditor->setText(text->utf8Ptr());
  m_valueEditor->setEnabled(true);
}

void JsonEditor::updateImagePreview() {
  String const& file = m_files[m_fileIndex];
  for (auto const& imagePath : m_options.editorImages) {
    auto value = imagePath->get(m_currentJson);
    if (!value) {
      if (value.error().is<JsonPath::TraversalException>())
        continue;
      m_errorDialog->showMessage(value.error().what());
      return;
    }
    auto image = value->toJson().toString();
    if (!image) {
      m_errorDialog->showMessage(image.error().what());
      return;
    }
    auto imageFile = image->extract(":");
    if (!imageFile) {
      m_errorDialog->showMessage(imageFile.error().what());
      return;
    }
    String imageName = File::relative_to(File::dir_name(file), *imageFile);
    QPixmap pixmap = QPixmap(imageName.utf8Ptr()).scaledToWidth(ImagePreviewWidth);
    if (pixmap.height() > ImagePreviewHeight)
      pixmap = pixmap.scaledToHeight(ImagePreviewHeight);
    m_imageLabel->setPixmap(pixmap);
    break;
  }
}

std::expected<int, StarException> Star::edit(Logger& logger, int argc, char* argv[],
    std::shared_ptr<::Star::JsonPath::Path> const& path, Options const& options, List<Input> const& inputs) {
  QApplication app(argc, argv);
  StringList files;
  for (Input const& input : inputs) {
    if (std::holds_alternative<FindInput>(input)) {
      auto found = findFiles(logger, std::get<FindInput>(input));
      if (!found)
        return std::unexpected(std::move(found.error()));
      files.appendAll(std::move(*found));
    } else {
      files.append(std::get<FileInput>(input).filename);
    }
  }
  if (files.empty())
    return 0;
  JsonEditor editor(path, options, files);
  editor.show();
  return app.exec();
}


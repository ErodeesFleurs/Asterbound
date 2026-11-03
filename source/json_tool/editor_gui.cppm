module;
#include <QErrorMessage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTextDocument>

import star.json_tool;
import std;
import star.string;
import star.list;
import star.formatted_json;
import star.json_path;
import star.logging;
import star.format;

namespace Star {

class JsonEditor : public QMainWindow {
public:
  explicit JsonEditor(std::shared_ptr<JsonPath::Path> const& path, Options const& options, List<String> const& files);

private:
  void next();
  void back();
  void updatePreview(QString const& valueStr);

  // Returns false if the change can't be made or the edit is invalid Json
  bool saveChanges();

  void displayCurrentFile();
  void updateValueEditor();
  void updateImagePreview();

  std::unique_ptr<QLabel> m_statusLabel;
  std::unique_ptr<QLabel> m_pathLabel;
  std::unique_ptr<QLabel> m_imageLabel;
  std::unique_ptr<QTextDocument> m_jsonDocument;
  std::unique_ptr<QTextEdit> m_jsonPreview;
  std::unique_ptr<QLineEdit> m_valueEditor;
  std::unique_ptr<QErrorMessage> m_errorDialog;
  std::unique_ptr<QPushButton> m_backButton;
  std::unique_ptr<QPushButton> m_nextButton;

  std::shared_ptr<JsonPath::Path> m_path;
  std::shared_ptr<JsonInputFormat> m_editFormat;
  Options m_options;
  List<String> m_files;
  std::size_t m_fileIndex;
  FormattedJson m_currentJson;
};

[[nodiscard]] std::expected<int, StarException> edit(Logger& logger, int argc, char* argv[], std::shared_ptr<JsonPath::Path> const& path, Options const& options, List<Input> const& inputs);

}


export module star.json_editor;

export namespace Star {
  using ::Star::JsonEditor;
  using ::Star::edit;
}

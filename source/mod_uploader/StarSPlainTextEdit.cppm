module;
#include <QPlainTextEdit>

import std;

namespace Star {

class SPlainTextEdit : public QPlainTextEdit {
  Q_OBJECT
public:
  SPlainTextEdit(std::optional<std::reference_wrapper<QWidget>> parent = std::nullopt);

signals:
  void editingFinished();

protected:
  void focusOutEvent(QFocusEvent* e);
};

}


export module star.s_plain_text_edit;

export namespace Star {
  using ::Star::SPlainTextEdit;
}

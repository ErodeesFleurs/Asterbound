#include <QPlainTextEdit>

import std;

import star.s_plain_text_edit;

namespace Star {

SPlainTextEdit::SPlainTextEdit(std::optional<std::reference_wrapper<QWidget>> parent)
    : QPlainTextEdit(parent ? &parent->get() : nullptr) {}

void SPlainTextEdit::focusOutEvent(QFocusEvent* e) {
  QPlainTextEdit::focusOutEvent(e);
  emit editingFinished();
}

}

// Generate with moc -i from StarSPlainTextEdit.cppm; compile in this scanned owner.
#include "moc_StarSPlainTextEdit.cpp"

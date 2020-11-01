/*  Copyright (C) 2014-2020 FastoGT. All right reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
        * Neither the name of FastoGT. nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "gui/widgets/fasto_scintilla.h"

#include <QKeyEvent>

#include <common/macros.h>

#include <common/qt/gui/shortcuts.h>

namespace {
int GetNumberOfDigits(int x) {
  if (x < 0) {
    return GetNumberOfDigits(-x) + 1;
  }

  if (x >= 10000) {
    if (x >= 10000000) {
      if (x >= 100000000) {
        if (x >= 1000000000) {
          return 10;
        }
        return 9;
      }
      return 8;
    }
    if (x >= 100000) {
      if (x >= 1000000) {
        return 7;
      }
      return 6;
    }
    return 5;
  }

  if (x >= 100) {
    if (x >= 1000) {
      return 4;
    }
    return 3;
  }

  if (x >= 10) {
    return 2;
  }

  return 1;
}

const QColor kCaretForegroundColor = QColor(Qt::black);
const QColor kSelectionBackgroundColor = QColor(Qt::blue);
const QColor kSelectionForegroundColor = QColor(Qt::white);

const QColor kMatchedBraceForegroundColor = QColor(190, 190, 190);
const QColor kMatchedBraceBackgroundColor = QColor(30, 36, 38);

const QColor kMarginsBackgroundColor = QColor(Qt::green);
const QColor kMarginsForegroundColor = QColor(Qt::white);
}  // namespace

namespace fastonosql {
namespace gui {

FastoScintilla::FastoScintilla(QWidget* parent) : QsciScintilla(parent), line_number_margin_width_(0) {
  setAutoIndent(true);
  setIndentationsUseTabs(false);
  setIndentationWidth(indentation_width);
  setUtf8(true);
  setMarginWidth(1, 0);

  setCaretForegroundColor(kCaretForegroundColor);

  setMatchedBraceForegroundColor(kMatchedBraceForegroundColor);
  setMatchedBraceBackgroundColor(kMatchedBraceBackgroundColor);

  setSelectionBackgroundColor(kSelectionBackgroundColor);
  setSelectionForegroundColor(kSelectionForegroundColor);

  setContentsMargins(0, 0, 0, 0);
  setViewportMargins(3, 3, 3, 3);
  setMarginLineNumbers(0, true);

  // Margins colors
  // line numbers margin
  setMarginsBackgroundColor(kMarginsBackgroundColor);
  setMarginsForegroundColor(kMarginsForegroundColor);

  SendScintilla(QsciScintilla::SCI_SETHSCROLLBAR, 0);

  setWrapMode(QsciScintilla::WrapNone);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  VERIFY(connect(this, &FastoScintilla::linesChanged, this, &FastoScintilla::updateLineNumbersMarginWidth));

  setAutoCompletionThreshold(1);
  setAutoCompletionCaseSensitivity(false);
  setAutoCompletionSource(QsciScintilla::AcsNone);
}

void FastoScintilla::setShowAutoCompletion(bool show) {
  setAutoCompletionSource(show ? QsciScintilla::AcsAPIs : QsciScintilla::AcsNone);
}

bool FastoScintilla::isShowAutoCompletion() const {
  AutoCompletionSource aut = autoCompletionSource();
  return aut == QsciScintilla::AcsAPIs;
}

void FastoScintilla::updateLineNumbersMarginWidth() {
  const int number_of_digits = GetNumberOfDigits(lines());

  int tw = textWidth(QsciScintilla::STYLE_LINENUMBER, "0");
  line_number_margin_width_ = number_of_digits * tw + row_number_width;

  // If line numbers margin already displayed, update its width
  if (lineNumberMarginWidth()) {
    setMarginWidth(0, line_number_margin_width_);
  }
}

void FastoScintilla::keyPressEvent(QKeyEvent* key_event) {
  if (key_event->key() == Qt::Key_F11) {
    key_event->ignore();
    toggleLinesNumbers();
    return;
  }

  if (isShowAutoCompletion()) {
    if (common::qt::gui::isAutoCompleteShortcut(key_event)) {
      showAutocompletion();
      return;
    } else if (common::qt::gui::isHideAutoCompleteShortcut(key_event)) {
      hideAutocompletion();
      return;
    }
  }

  QsciScintilla::keyPressEvent(key_event);
}

int FastoScintilla::lineNumberMarginWidth() const {
  return marginWidth(0);
}

int FastoScintilla::textWidth(int style, const QString& text) {
  const QByteArray utf8 = text.toUtf8();
  const char* byte_array = utf8.constData();
  return SendScintilla(QsciScintilla::SCI_TEXTWIDTH, style, byte_array);
}

void FastoScintilla::toggleLinesNumbers() {
  updateLineNumbersMarginWidth();
  if (!lineNumberMarginWidth()) {
    setMarginWidth(0, line_number_margin_width_);
  } else {
    setMarginWidth(0, 0);
  }
}

void FastoScintilla::showAutocompletion() {
  if (isShowAutoCompletion()) {
    autoCompleteFromAPIs();
  }
}

void FastoScintilla::hideAutocompletion() {
  if (isShowAutoCompletion()) {
    cancelList();
  }
}

}  // namespace gui
}  // namespace fastonosql

#include "text_entry.h"

namespace {
constexpr const char *kAlphanumericCharset =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
constexpr const char *kHexCharset = "0123456789ABCDEF";
}  // namespace

TextEntryWidget::TextEntryWidget(CharSet charSet, size_t maxLength) {
  if (charSet == CharSet::kHex) {
    charset_ = kHexCharset;
    charsetLength_ = 16;
  } else {
    charset_ = kAlphanumericCharset;
    charsetLength_ = 37;
  }
  maxLength_ = maxLength > kMaxBufferLength ? kMaxBufferLength : maxLength;
}

void TextEntryWidget::scrollNext() {
  if (done_) return;
  cursor_ = (cursor_ + 1) % (charsetLength_ + 1);
}

void TextEntryWidget::scrollPrev() {
  if (done_) return;
  cursor_ = (cursor_ == 0) ? charsetLength_ : cursor_ - 1;
}

void TextEntryWidget::commitChar() {
  if (done_) return;

  if (isDoneSelected()) {
    done_ = true;
    return;
  }

  if (length_ >= maxLength_) {
    return;  // buffer full — only "done" is a valid path forward
  }

  buffer_[length_++] = charset_[cursor_];
  buffer_[length_] = '\0';
  cursor_ = 0;
}

void TextEntryWidget::backspace() {
  if (done_ || length_ == 0) return;
  buffer_[--length_] = '\0';
}

bool TextEntryWidget::isDoneSelected() const { return cursor_ == charsetLength_; }

char TextEntryWidget::currentChar() const { return charset_[cursor_]; }

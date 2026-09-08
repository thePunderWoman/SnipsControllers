#pragma once

#include <cstddef>

// Reusable one-character-at-a-time entry widget ("scroll wheel" style,
// like old feature-phone name entry): scroll to a character, commit it,
// repeat. A trailing "done" position past the last real character in the
// charset finishes entry. Pure logic — no display or button-mapping
// decisions live here; the menu screen that owns an instance decides how
// scroll/commit/backspace map to physical buttons and how to render the
// current highlight.
class TextEntryWidget {
 public:
  enum class CharSet { kAlphanumeric, kHex };

  explicit TextEntryWidget(CharSet charSet, size_t maxLength = 20);

  // Cycles the current slot's highlight forward/backward through the
  // charset, wrapping at both ends (including the trailing "done"
  // position). No-op once done().
  void scrollNext();
  void scrollPrev();

  // Commits the current highlight into the buffer and advances to a new
  // slot (highlight resets to the first charset character), unless the
  // current highlight is the "done" position, which finishes entry
  // instead — see done(). No-op once done(), and no-op if the buffer is
  // already at maxLength and the highlight isn't "done".
  void commitChar();

  // Removes the last committed character. No-op if empty or once done().
  void backspace();

  // True when the current highlight is the trailing "done" position —
  // the caller should show "DONE" rather than calling currentChar().
  bool isDoneSelected() const;

  // Valid only when !isDoneSelected().
  char currentChar() const;

  bool done() const { return done_; }
  const char *text() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  static constexpr size_t kMaxBufferLength = 20;

  const char *charset_;
  size_t charsetLength_;
  size_t cursor_ = 0;  // 0..charsetLength_ inclusive; charsetLength_ = done
  char buffer_[kMaxBufferLength + 1] = {};
  size_t length_ = 0;
  size_t maxLength_;
  bool done_ = false;
};

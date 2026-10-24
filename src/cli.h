#pragma once

#include <QStringList>

// The commands that need no window: primer tm, check, design, and skill.
namespace cli {

// True when the arguments name a command rather than the window.
bool isCommand(const QStringList &arguments);

// Runs the command and returns the exit code: 0 on success, 1 on any failed
// check or error, with errors on stderr.
int run(const QStringList &arguments);

} // namespace cli

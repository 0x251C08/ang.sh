// config.h — loads ~/.config/angsh/config.ang, theme.conf, plugins/*.ang.
// All config lines are plain angsh commands, so ricing needs no new language.
#pragma once
#include "env.h"

namespace ang {

void apply_defaults(ShellState& st);
void load_config(ShellState& st);  // silent; never fatal

}  // namespace ang

#pragma once

namespace toasty::layout {
    bool enabled();
    void setEnabled(bool on);
    void apply(bool on);
    void loadSaved();
}

#pragma once
#include <string_view>

namespace toasty::i18n {
    enum class Language { English, Indonesian };

    Language current();
    void setLanguage(Language lang);
    void loadSaved();
    std::string_view tr(std::string_view id);
}

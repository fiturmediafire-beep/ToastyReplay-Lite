#include "Lang.hpp"
#include <Geode/Geode.hpp>
#include <unordered_map>

using namespace geode::prelude;

namespace toasty::i18n {

static Language s_lang = Language::English;

static const std::unordered_map<std::string_view, std::string_view> s_id = {
    {"Main",                "Utama"},
    {"Macros",              "Makro"},
    {"Settings",            "Pengaturan"},
    {"Keybinds",            "Tombol"},
    {"About",               "Tentang"},
    {"Macro Controls",      "Kontrol Makro"},
    {"Record",              "Rekam"},
    {"Replay",              "Putar"},
    {"Idle",                "Siaga"},
    {"Safe Mode",           "Mode Aman"},
    {"Auto Safe Mode",      "Mode Aman Otomatis"},
    {"Layout Mode",         "Mode Layout"},
    {"Frame Stepper",       "Stepper Frame"},
    {"Noclip",              "Noclip"},
    {"Menu Scale",          "Skala Menu"},
    {"Accent Color",        "Warna Aksen"},
    {"Show Notifications",  "Tampilkan Notifikasi"},
    {"Auto Save Macros",    "Simpan Makro Otomatis"},
    {"Language",            "Bahasa"},
    {"Open Menu",           "Buka Menu"},
    {"Speedhack",           "Speedhack"},
    {"Frame Step",          "Step Frame"},
    {"Toggle Stepper",      "Aktifkan Stepper"},
    {"Show Trajectory",     "Tampilkan Lintasan"},
};

Language current() { return s_lang; }

void loadSaved() {
    int v = Mod::get()->getSavedValue<int>("language", 0);
    s_lang = (v == 1) ? Language::Indonesian : Language::English;
}

void setLanguage(Language lang) {
    s_lang = lang;
    Mod::get()->setSavedValue<int>("language", static_cast<int>(lang));
}

std::string_view tr(std::string_view id) {
    if (s_lang == Language::English) return id;
    auto it = s_id.find(id);
    if (it != s_id.end()) return it->second;
    return id;
}

} // namespace toasty::i18n

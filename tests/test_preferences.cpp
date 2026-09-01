#include <fstream>

#include "settings/preferences.hpp"
#include "test_suites.hpp"
#include "ui/theme.hpp"

namespace {
void preferencesRoundTrip() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "nested" / "preferences";
    require(savePreferences(path, {ThemeMode::Light}), "preferences write must succeed");
    require(loadPreferences(path).theme == ThemeMode::Light, "theme must round-trip");
}

void invalidThemeFallsBackToDark() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "preferences";
    std::ofstream(path) << "theme system\n";
    require(loadPreferences(path).theme == ThemeMode::Dark,
            "unsupported themes must fall back to Dark");
}

void themesResolveToDifferentPalettes() {
    const auto dark = resolveTheme(ThemeMode::Dark);
    const auto light = resolveTheme(ThemeMode::Light);
    require(dark.background.r != light.background.r && dark.text.r != light.text.r,
            "Dark and Light must resolve to distinct palettes");
}
}  // namespace

void addPreferencesTests(TestCases& tests) {
    tests.emplace_back("preferences round-trip", preferencesRoundTrip);
    tests.emplace_back("preferences validation", invalidThemeFallsBackToDark);
    tests.emplace_back("theme palettes", themesResolveToDifferentPalettes);
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "ctrl-handle-manager.h"

#include <sigc++/signal.h>
#include <glib/gi18n.h>
#include <glibmm/main.h>
#include <giomm/file.h>
#include <giomm/filemonitor.h>

#include "ctrl-handle-styling.h"

#include "helper/auto-connection.h"
#include "io/resource.h"
#include "preferences.h"

namespace Inkscape::Handles {
namespace {

class ManagerImpl final : public Manager
{
public:
    ManagerImpl();

private:
    // Most recent css (shared between all CanvasItemContexts)
    std::shared_ptr<Css const> css;

    // For file system monitoring.
    Glib::RefPtr<Gio::FileMonitor> monitor;
    auto_connection timeout;

    // Emitted when the css changes.
    sigc::signal<void()> signal_css_updated;

    void updateCss();

    friend Manager;
};

ManagerImpl::ManagerImpl()
{
    current_theme = Inkscape::Preferences::get()->getIntLimited("/handles/color-scheme-index", 0, 0, get_handle_themes().size() - 1);

    // Set the initial css.
    updateCss();

    // Monitor the user css path for changes. We use a timeout to compress multiple events into one.
    auto const path = IO::Resource::get_path_string(IO::Resource::USER, IO::Resource::UIS, "node-handles.css");
    auto file = Gio::File::create_for_path(path);
    monitor = file->monitor_file();
    monitor->signal_changed().connect([this] (Glib::RefPtr<Gio::File> const &, Glib::RefPtr<Gio::File> const &, Gio::FileMonitor::Event) {
        if (timeout) return;
        timeout = Glib::signal_timeout().connect([this] {
            updateCss();
            signal_css_updated.emit();
            return false;
        }, 200);
    });
}

void ManagerImpl::updateCss()
{
    auto& filename = get_handle_themes().at(current_theme).file_name;
    css = std::make_shared<Css const>(parse_css(filename));
}

} // namespace

Manager &Manager::get()
{
    static ManagerImpl instance;
    return instance;
}

std::shared_ptr<Css const> Manager::getCss() const
{
    auto &d = static_cast<ManagerImpl const &>(*this);
    return d.css;
}

sigc::connection Manager::connectCssUpdated(sigc::slot<void()> &&slot)
{
    auto &d = static_cast<ManagerImpl &>(*this);
    return d.signal_css_updated.connect(std::move(slot));
}

// predefined handle color themes
const std::vector<ColorTheme>& Manager::get_handle_themes() const {
#define translation_context "Handle color scheme name"
    static std::vector<ColorTheme> themes = {
        // default blue scheme
        {"handle-theme-azure.css",    C_(translation_context, "Azure"), true, 0x2a7fff},
        // red scheme
        {"handle-theme-crimson.css",  C_(translation_context, "Crimson"), true, 0xff1a5e},
        // green scheme
        {"handle-theme-spruce.css",   C_(translation_context, "Spruce"), true, 0x05ca85},
        // purple scheme
        {"handle-theme-violet.css",   C_(translation_context, "Violet"), true, 0xbb61f3},
        // yellow scheme
        {"handle-theme-gold.css",     C_(translation_context, "Gold"), true, 0xebca00},
        // gray scheme
        {"handle-theme-steel.css",    C_(translation_context, "Steel"), true, 0x9db4d8},
        // a "negative" version
        {"handle-theme-negative.css", C_(translation_context, "Negative"), false, 0xa0a0b0},
        // reserved for user custom style
        {"handle-theme-custom.css",   C_(translation_context, "Custom"), true, 0x808080},
    };
    return themes;
#undef translation_context
}

void Manager::select_theme(int index) {
    auto& themes = get_handle_themes();
    if (static_cast<size_t>(index) >= themes.size()) {
        g_warning("Invalid handle color theme index, css not loaded.");
        return;
    }
    current_theme = index;
    Inkscape::Preferences::get()->setInt("/handles/color-scheme-index", index);
    auto &d = static_cast<ManagerImpl&>(*this);
    d.updateCss();
    d.signal_css_updated.emit();
}

} // namespace Inkscape::Handles

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

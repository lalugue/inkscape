// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for Live Path Effects (LPE)
 */
/* Authors:
 *   Jabiertxof
 *   Adam Belis (UX/Design)
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef LIVEPATHEFFECTEDITOR_H
#define LIVEPATHEFFECTEDITOR_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <glibmm/refptr.h>

#include "preferences.h"

#include "live_effects/effect-enum.h"
#include "object/sp-lpe-item.h"       // PathEffectList
#include "ui/dialog/dialog-base.h"
#include "ui/widget/completion-popup.h"

namespace Gtk {
class Box;
class Builder;
class Button;
class Expander;
class Label;
class ListBox;
class ListStore;
class Widget;
} // namespace Gtk

namespace Inkscape {
class Selection;
} // namespace Inkscape

namespace Inkscape::LivePathEffect {
class Effect;
class LPEObjectReference;
} // namespace Inkscape::LivePathEffect

class SPLPEItem;

namespace Inkscape::UI::Dialog {

/*
 * @brief The LivePathEffectEditor class
 */
class LivePathEffectEditor final : public DialogBase
{
public:
    LivePathEffectEditor();
    ~LivePathEffectEditor() final;

    LivePathEffectEditor(LivePathEffectEditor const &d) = delete;
    LivePathEffectEditor operator=(LivePathEffectEditor const &d) = delete;

    static LivePathEffectEditor &getInstance() { return *new LivePathEffectEditor(); }
    void move_list(gint origin, gint dest);

    std::vector<std::pair<Gtk::Expander *, std::shared_ptr<Inkscape::LivePathEffect::LPEObjectReference> > > _LPEExpanders;
    void showParams(std::pair<Gtk::Expander *, std::shared_ptr<Inkscape::LivePathEffect::LPEObjectReference> > expanderdata, bool changed);
    bool updating = false;
    SPLPEItem *current_lpeitem = nullptr;
    std::pair<Gtk::Expander *, std::shared_ptr<Inkscape::LivePathEffect::LPEObjectReference> > current_lperef = std::make_pair(nullptr, nullptr);
    static const LivePathEffect::EnumEffectData<LivePathEffect::EffectType> *getActiveData();
    bool selection_changed_lock = false;
    bool dnd = false;

private:
    Glib::RefPtr<Gtk::Builder> _builder;

public:
    Gtk::ListBox& LPEListBox;
    gint dndx = 0;
    gint dndy = 0;

private:
    void add_lpes(Inkscape::UI::Widget::CompletionPopup& popup, bool symbolic);
    void clear_lpe_list();
    void selectionChanged (Inkscape::Selection *selection                ) final;
    void selectionModified(Inkscape::Selection *selection, unsigned flags) final;
    void onSelectionChanged(Inkscape::Selection *selection);
    void expanded_notify(Gtk::Expander *expander);
    void onAdd(Inkscape::LivePathEffect::EffectType etype);
    bool showWarning(std::string const &msg);
    void toggleVisible(Inkscape::LivePathEffect::Effect *lpe, Gtk::Button *visbutton);
    bool is_appliable(LivePathEffect::EffectType etypen, Glib::ustring item_type, bool has_clip, bool has_mask);
    void removeEffect(Gtk::Expander * expander);
    void effect_list_reload(SPLPEItem *lpeitem);
    void selection_info();
    void map_handler();
    void clearMenu();
    void setMenu();
    bool lpeFlatten(std::shared_ptr<Inkscape::LivePathEffect::LPEObjectReference> lperef);

    SPLPEItem * clonetolpeitem();
    Inkscape::UI::Widget::CompletionPopup _lpes_popup;
    Gtk::Box& _LPEContainer;
    Gtk::Box& _LPEAddContainer;
    Gtk::Label&_LPESelectionInfo;
    Gtk::ListBox&_LPEParentBox;
    Gtk::Box&_LPECurrentItem;
    PathEffectList effectlist;
    Glib::RefPtr<Gtk::ListStore> _LPEList;
    Glib::RefPtr<Gtk::ListStore> _LPEListFilter;
    const LivePathEffect::EnumEffectDataConverter<LivePathEffect::EffectType> &converter;
    Gtk::Widget *effectwidget = nullptr;
    Gtk::Widget *popupwidg = nullptr;
    GtkWidget *currentdrag = nullptr;
    bool _reload_menu = false;
    gint _buttons_width = 0;
    bool _freezeexpander = false;
    Glib::ustring _item_type;
    bool _has_clip;
    bool _has_mask;
};

} // namespace Inkscape::UI::Dialog

#endif // LIVEPATHEFFECTEDITOR_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

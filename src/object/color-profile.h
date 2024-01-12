// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SPObject of the color-profile object found a direct child of defs.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_COLOR_PROFILE_H
#define SEEN_COLOR_PROFILE_H

#include <lcms2.h> // XXX Remove during refactoring!

#include "color.h"                 // XXX Remove during refactoring!
#include "color/cms-color-types.h" // XXX Remove as well
#include "colors/cms/system.h"
#include "colors/spaces/enum.h" // RenderingIntent
#include "sp-object.h"

namespace Inkscape {

class URI;

enum class ColorProfileStorage
{
    HREF_DATA,
    HREF_FILE,
    LOCAL_ID,
};

class ColorProfile final : public SPObject
{
public:
    ColorProfile() = default;
    ~ColorProfile() override = default;
    int tag() const override { return tag_of<decltype(*this)>; }

    static ColorProfile *createFromProfile(SPDocument *doc, Colors::CMS::Profile const &profile,
                                           std::string const &name, ColorProfileStorage storage);

    std::string getName() const { return _name; }
    std::string getLocalProfileId() const { return _local; }
    std::string getProfileData() const;
    Colors::RenderingIntent getRenderingIntent() const { return _intent; }

    // This is the only variable we expect inkscape to modify. Changing the icc
    // profile data or ID should instead involve creating a new ColorProfile element.
    void setRenderingIntent(Colors::RenderingIntent intent);

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void release() override;

    void set(SPAttr key, char const *value) override;

    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned int flags) override;

private:
    std::string _name;
    std::string _local;
    Colors::RenderingIntent _intent;

    std::unique_ptr<Inkscape::URI> _uri;

public:
    // XXX Compatability code which will be removed in the main refactoring step, never populated
    char *name = nullptr;
    char *href = nullptr;
    unsigned int rendering_intent;

    ColorSpaceSig getColorSpace() const { return {0}; };
    ColorProfileClassSig getProfileClass() const { return {0}; }
    cmsHTRANSFORM getTransfToSRGB8() { return nullptr; }
    cmsHTRANSFORM getTransfFromSRGB8() { return nullptr; }
    cmsHTRANSFORM getTransfGamutCheck() { return nullptr; };
    bool GamutCheck(SPColor color) { return false; }
    int getChannelCount() const { return 4; }
    bool isPrintColorSpace() { return false; }
    cmsHPROFILE getHandle() { return nullptr; }
};

// XXX Delete!!!
enum
{
    RENDERING_INTENT_UNKNOWN = 0,
    RENDERING_INTENT_AUTO = 1,
    RENDERING_INTENT_PERCEPTUAL = 2,
    RENDERING_INTENT_RELATIVE_COLORIMETRIC = 3,
    RENDERING_INTENT_SATURATION = 4,
    RENDERING_INTENT_ABSOLUTE_COLORIMETRIC = 5
};

} // namespace Inkscape

#endif // !SEEN_COLOR_PROFILE_H

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

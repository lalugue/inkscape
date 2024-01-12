// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Jon A. Cruz <jon@joncruz.org>
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2013-2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_COMPONENTS_H
#define SEEN_COLORS_COMPONENTS_H

#include <lcms2.h>
#include <map>
#include <string>
#include <vector>

namespace Inkscape::Colors::Space {

enum class Type;

struct Component
{
    Component(Type type, unsigned int index, std::string id, std::string name, std::string tip, unsigned scale);

    Type type;
    unsigned int index;
    std::string id;
    std::string name;
    std::string tip;
    unsigned scale;

    double normalize(double value) const;
};

class Components
{
public:
    Components() = default;

    static Components const &get(Type type, bool alpha = false);

    std::vector<Component>::const_iterator begin() const { return std::begin(_components); }
    std::vector<Component>::const_iterator end() const { return std::end(_components); }
    Component const &operator[](const unsigned int index) const { return _components[index]; }

    Type getType() const { return _type; }
    unsigned size() const { return _components.size(); }

    void add(std::string id, std::string name, std::string tip, unsigned scale);
    void setType(Type type) { _type = type; }

private:
    Type _type;
    std::vector<Component> _components;
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_COMPONENTS_H

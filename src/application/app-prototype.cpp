/**
 * \brief  Base class for different application modes
 *
 * Author:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *
 * Copyright (C) 2005 Bryce Harrington
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#include "app-prototype.h"

namespace Inkscape {
namespace NSApplication {

AppPrototype::AppPrototype() 
{
}

AppPrototype::AppPrototype(int argc, const char **argv) 
{
}

AppPrototype::~AppPrototype()
{
}


} // namespace NSApplication
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:75
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :

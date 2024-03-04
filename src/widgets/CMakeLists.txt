# SPDX-License-Identifier: GPL-2.0-or-later

set(widgets_SRC
	paintdef.cpp
	sp-attribute-widget.cpp
	spw-utilities.cpp

	# -------
	# Headers
	paintdef.h
	sp-attribute-widget.h
	spw-utilities.h
	widget-sizes.h
)

add_inkscape_source("${widgets_SRC}")

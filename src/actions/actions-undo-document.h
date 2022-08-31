// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Undo/Redo tied to document.
 *
 * Authors:
 *   Tavmjong Bah
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INK_ACTIONS_DOCUMENT_UNDO_H
#define INK_ACTIONS_DOCUMENT_UNDO_H

class SPDocument;
class InkscapeApplication;

void add_actions_undo_document(SPDocument* document);
void add_actions_undo_app(InkscapeApplication* app);

#endif // INK_ACTIONS_DOCUMENT_UNDO_H

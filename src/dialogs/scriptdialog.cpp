/*
 * This dialog is for launching scripts whose main purpose if
 * the scripting of Inkscape itself.
 *
 * Authors:
 *   Bob Jamison
 *   Other dudes from The Inkscape Organization
 *
 * Copyright (C) 2004 The Inkscape Organization
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "scriptdialog.h"

#include <gtkmm.h>
#include <glibmm.h>


#include <dialogs/dialog-events.h>
#include "helper/sp-intl.h"

namespace Inkscape {
namespace UI {
namespace Dialogs {


//#########################################################################
//## I M P L E M E N T A T I O N
//#########################################################################

/**
 * A script editor/executor
 */
class ScriptDialogImpl : public ScriptDialog, public Gtk::Dialog
{

    public:
    

    /**
     * Constructor
     */
    ScriptDialogImpl();

    /**
     * Destructor
     */
    ~ScriptDialogImpl();


    /**
     * Show the dialog
     */
    void show();

    /**
     * Do not show the dialog
     */
    void hide();

    /**
     * Clear the text
     */
    void clear();



    private:


    Gtk::MenuBar menuBar;

    Gtk::Menu   fileMenu;

    Gtk::ScrolledWindow scriptTextScroll;

    Gtk::TextView scriptText;

 
};




//#########################################################################
//## E V E N T S
//#########################################################################

/**
 * Also a public method.  Remove all text from the dialog
 */
void ScriptDialogImpl::clear()
{
    Glib::RefPtr<Gtk::TextBuffer> buffer = scriptText.get_buffer();
    buffer->erase(buffer->begin(), buffer->end());
}


//#########################################################################
//## C O N S T R U C T O R    /    D E S T R U C T O R
//#########################################################################
/**
 * Constructor
 */
ScriptDialogImpl::ScriptDialogImpl()
{
    set_title(_("Script"));
    set_size_request(300, 400);

    Gtk::VBox *mainVBox = get_vbox();

    //## Add a menu for clear()
    menuBar.items().push_back( Gtk::Menu_Helpers::MenuElem(_("_File"), fileMenu) );
    fileMenu.items().push_back( Gtk::Menu_Helpers::MenuElem(_("_Clear"),
           sigc::mem_fun(*this, &ScriptDialogImpl::clear) ) );
    mainVBox->pack_start(menuBar, Gtk::PACK_SHRINK);
    

    //### Set up the text widget
    scriptText.set_editable(false);
    scriptTextScroll.add(scriptText);
    scriptTextScroll.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_ALWAYS);
    mainVBox->pack_start(scriptTextScroll);

    show_all_children();

}

/**
 * Factory method.  Use this to create a new ScriptDialog
 */
ScriptDialog *ScriptDialog::create()
{
    ScriptDialog *dialog = new ScriptDialogImpl();
    return dialog;
}


/**
 * Constructor
 */
ScriptDialogImpl::~ScriptDialogImpl()
{


}


//#########################################################################
//## M E T H O D S
//#########################################################################

void ScriptDialogImpl::show()
{
    //call super()
    Gtk::Dialog::show();
    //sp_transientize((GtkWidget *)gobj());  //Make transient
    raise();
    Gtk::Dialog::present();
}



void ScriptDialogImpl::hide()
{
    //call super()
    Gtk::Dialog::hide();
}



/* static instance, to reduce dependencies */
static ScriptDialog *scriptDialogInstance = NULL;

ScriptDialog *ScriptDialog::getInstance()
{
    if ( !scriptDialogInstance )
        {
        scriptDialogInstance = new ScriptDialogImpl();
        }
    return scriptDialogInstance;
}



void ScriptDialog::showInstance()
{
    ScriptDialog *scriptDialog = getInstance();
    scriptDialog->show();
}






} //namespace Dialogs
} //namespace UI
} //namespace Inkscape

//#########################################################################
//## E N D    O F    F I L E
//#########################################################################




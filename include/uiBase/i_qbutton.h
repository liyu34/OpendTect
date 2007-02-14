#ifndef i_qbutton_h
#define i_qbutton_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Lammertink
 Date:          26/04/2000
 RCS:           $Id: i_qbutton.h,v 1.12 2007-02-14 12:38:00 cvsnanne Exp $
________________________________________________________________________

-*/

#include <qobject.h>
#include <uibutton.h>

#ifdef USEQT3
# define mButton QButton
# include <qbutton.h>
#else
# define mButton QAbstractButton
# include <QAbstractButton>
#endif

//! Help class, because templates can not use signals/slots
/*!
    Relays QT button signals to the notifyHandler of a uiButton object.
*/
class i_ButMessenger : public QObject 
{ 
    Q_OBJECT
    friend class                uiButton;
public:
				i_ButMessenger( mButton*  sender,
                                       	  	uiButtonBody* receiver )
                                : _receiver( receiver )
                                , _sender( sender )
				{
				    connect( _sender, SIGNAL( clicked() ), 
					     this,   SLOT( clicked() ) );
				    connect( _sender, SIGNAL( pressed() ), 
					     this,   SLOT( pressed() ) );
				    connect( _sender, SIGNAL( released() ), 
					     this,   SLOT( released() ) );
				    connect( _sender, SIGNAL(toggled(bool)), 
					     this,   SLOT(toggled(bool)) );
				}

private:

    uiButtonBody*		_receiver;
    mButton*			_sender;

public slots:

    void toggled( bool ) 	
		{ _receiver->notifyHandler( uiButtonBody::toggled ); }
    void clicked() 		
		{ _receiver->notifyHandler( uiButtonBody::clicked ); }
    void pressed() 		
		{ _receiver->notifyHandler( uiButtonBody::pressed ); }
    void released()		
		{ _receiver->notifyHandler( uiButtonBody::released); }
};

#endif

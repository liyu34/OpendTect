#ifndef uigeninput_h
#define uigeninput_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Bril
 Date:          Oct 2000
 RCS:           $Id: uigeninput.h,v 1.26 2004-02-19 14:21:17 nanne Exp $
________________________________________________________________________

-*/

#include "uigroup.h"
#include "datainpspec.h"
#include "position.h"

class uiLineEdit;
class uiLabel;
class uiCheckBox;
class uiPushButton;

class uiInputFld;
class DataInpSpec;

class FieldIdx;
class UserInputObj;

/*! \brief General Input Element

The General Input line consists of a number of input elements, 
optionally accompanied with either a label or a checkbox.

The uiGeninput is constructed with a number of DataInpSpec's, which determine
what kind of input field is going to be under the hood.

The usage of the DataInpSpec keeps the constructor for uiGenInput very simple
and also it's most common use. For example, to define a uiGenInput that inputs
a string, you'd do something like

\code
uiGenInput* xx = new uiGenInput( this,"Input name", StringInpSpec("Initial"));
\endcode

For more complicated inputs, you'd have to explicitely construct a
DataInpSpec sub-class, and fill in it's details, like allowed range, etc.

When passed to the constructor of a uiGenInput, the DataInpSpec objects
are copied. You can use uiGenInput::getInpSpec to get a hold of them, but
only if the uiGenInput has not been finalised yet.


*/

class uiGenInput : public uiGroup
{
public:
			uiGenInput( uiParent* p, const char* disptxt
				  , const char* inputStr=0 ); 

			uiGenInput( uiParent* p, const char* disptxt,
			    const DataInpSpec& );

			uiGenInput( uiParent* p, const char* disptxt,
			    const DataInpSpec& ,const DataInpSpec& );

			uiGenInput( uiParent* p, const char* disptxt,
			    const DataInpSpec&, const DataInpSpec&,
			    const DataInpSpec& );

    virtual		~uiGenInput();

    void		addInput( const DataInpSpec& );

/*! \brief gives access to the actual spec of an input field.

When the uiGenInput is being finalised, the actual input fields are 
constructed. The fields make a clone of their input spec,
which are used for range checking, etc.

The purpose of this is to be able to store the actual current spec's, 
instead of the intial specs, which might have been updated with newSpec(). 
For example, limits might have been set/changed.

\sa  bool newSpec(DataInpSpec* nw, int nr)
 
*/
    const DataInpSpec*	spec( int nr ) const;

/*! \brief makes it possible to change the actual spec for an input field.

Returns true, if changes are accepted.

*/
    bool		newSpec(const DataInpSpec& nw, int nr);

    //! update spec() from current values on screen.
    void		updateSpecs();

//! checks if inputs are valid, f.e. within specified range
    bool		isValid( int nr=0 ) const;
    bool		isUndef( int nr=0 ) const;

    const char*		text(int nr=0, const char* undefVal="") const;

    int			getIntValue( int nr=0, int undefVal=mUndefIntVal) const;
    double		getValue( int nr=0, double undefVal=mUndefValue ) const;
    float		getfValue( int nr=0, float undefVal=mUndefValue ) const;
    bool		getBoolValue( int nr=0, bool undefVal=false ) const;

    inline Interval<int> getIInterval(int nr=0) const
			{ return Interval<int>( getIntValue(nr*2),
						getIntValue(nr*2+1) ); }
    inline Interval<float> getFInterval(int nr=0) const
			{ return Interval<float>( getfValue(nr*2),
						  getfValue(nr*2+1) ); }
    inline Interval<double> getDInterval(int nr=0) const
			{ return Interval<double>( getValue(nr*2),
						   getValue(nr*2+1) ); }
    inline StepInterval<int> getIStepInterval(int nr=0) const
			{ return StepInterval<int>( getIntValue(nr*3),
						    getIntValue(nr*3+1),
						    getIntValue(nr*3+2) ); }
    inline StepInterval<float> getFStepInterval(int nr=0) const
			{ return StepInterval<float>( getfValue(nr*3),
						      getfValue(nr*3+1),
						      getfValue(nr*3+2) ); }
    inline StepInterval<double> getDStepInterval(int nr=0) const
			{ return StepInterval<double>( getValue(nr*3),
						       getValue(nr*3+1),
						       getValue(nr*3+2) ); }
    inline Coord	getCoord(int nr=0) const
			{ return Coord(getValue(nr*2),getValue(nr*2+1)); }
    inline BinID	getBinID(int nr=0) const
			{ return BinID(getIntValue(nr*2),getIntValue(nr*2+1)); }

    void		setText(const char*,int nr=0);
    void		setValue(int,int nr=0);
    void		setValue(double,int nr=0);
    void		setValue(float,int nr=0);
    void		setValue(bool,int nr=0);

    inline void		setTexts( const char* v1, const char* v2 )
			{ setText(v1,0); setText(v2,1); }
    inline void		setValues( int v1, int v2 )
			{ setValue(v1,0); setValue(v2,1); }
    inline void		setValues( double v1, double v2 )
			{ setValue(v1,0); setValue(v2,1); }
    inline void		setValues( float v1, float v2 )
			{ setValue(v1,0); setValue(v2,1); }
    inline void		setValues( bool v1, bool v2 )
			{ setValue(v1,0); setValue(v2,1); }
    inline void		setValue( const Coord& c )
			{ setValue(c.x,0); setValue(c.y,1); }
    inline void		setValue( const BinID& b )
			{ setValue(b.inl,0); setValue(b.crl,1); }
    inline void		setValue( const Interval<int>& i )
			{ setValue(i.start,0); setValue(i.stop,1); }
    inline void		setValue( const Interval<float>& i )
			{ setValue(i.start,0); setValue(i.stop,1); }
    inline void		setValue( const Interval<double>& i )
			{ setValue(i.start,0); setValue(i.stop,1); }
    inline void		setValue( const StepInterval<int>& i )
			{ setValue(i.start,0); setValue(i.stop,1);
			  setValue(i.step,2); }
    inline void		setValue( const StepInterval<float>& i )
			{ setValue(i.start,0); setValue(i.stop,1);
			  setValue(i.step,2); }
    inline void		setValue( const StepInterval<double>& i )
			{ setValue(i.start,0); setValue(i.stop,1);
			  setValue(i.step,2); }

    void		displayField(bool yn=true,int elemnr=-1,int fldnr=-1);
    void		setReadOnly( bool yn=true,int nr=-1);
    void		setSensitive(bool yn=true,int elemnr=-1,int fldnr=-1);
    void		clear(int nr=-1);

			//! returns 0 if not finalised.
    UserInputObj*	element(int idx); 
    int			nElements() const; 
    void		setElemSzPol( uiObject::SzPolicy p )	{ elemszpol=p; }
    uiObject::SzPolicy	elemSzPol() const		{ return elemszpol; }


    virtual const char*	titleText();
    virtual void	setTitleText(const char*);

    void 		setChecked( bool yn );
    bool		isChecked();
    bool		isCheckable()
    			{ return cbox ? true : false; }

    virtual bool 	isSingleLine() const		{ return true; }

    void		setWithCheck( bool yn=true )	{ withchk = yn; }
    void		setWithSelect( bool yn=true ) 
			{ selText = yn ? "Select ..." : "" ; }

    Notifier<uiGenInput> checked;
    Notifier<uiGenInput> valuechanging;
    Notifier<uiGenInput> valuechanged;

protected:

    ObjectSet<uiInputFld>	flds;
    TypeSet<FieldIdx>&		idxes;

    bool		finalised;

    BufferString	selText;
    bool		withchk;
    bool		withclr;

    uiLabel*		labl;
    uiCheckBox*		cbox;
    uiPushButton*	selbut;
    uiPushButton*	clrbut;

                        //! "Select ..." is pressed. Calls virtual doSelect
    void		doSelect_(CallBacker*);
                        //! "Select ..." is pressed. Called by doSelect_
    virtual void	doSelect(CallBacker*)   {}
    void		doClear(CallBacker*);

    void 		checkBoxSel(CallBacker*);

			//! DataInpField factory
    uiInputFld& 	createInpFld(const DataInpSpec&);
    void		doFinalise();
    inline DataInpSpec*	spec( int nr )
			{
			    return const_cast<DataInpSpec*>
				( ((const uiGenInput*)this)->spec(nr) );
			}

private:

    bool		checked_;

    bool		rdonly_;
    bool		rdonlyset_;

    ObjectSet<DataInpSpec> inputs;

    uiObject::SzPolicy	elemszpol;
};


#endif

/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   DK, David Kirkby,    UC Irvine,         dkirkby@uci.edu                 *
 *                                                                           *
 * Copyright (c) 2000-2005, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// BEGIN_HTML
// Class RooTrace controls the memory tracing hooks in all RooFit
// objects. When tracing is active, a table of live RooFit objects
// is kept that can be queried at any time. In verbose mode, messages
// are printed in addition at the construction and destruction of
// each object.
// END_HTML
//

#include "RooFit.h"

#include "RooTrace.h"
#include "RooAbsArg.h"
#include "Riostream.h"
#include "RooMsgService.h"

#include <iomanip>




using namespace std;

ClassImp(RooTrace)
;


Bool_t RooTrace::_active(kFALSE) ;
Bool_t RooTrace::_verbose(kFALSE) ;
RooLinkedList RooTrace::_list ;
RooLinkedList RooTrace::_markList ;
map<TClass*,int> RooTrace::_objectCount ;
map<string,int> RooTrace::_specialCount ;
map<string,int> RooTrace::_specialSize ;


//_____________________________________________________________________________
void RooTrace::create(const TObject* obj) 
{ 
  // Register creation of object 'obj' 

  if (_active) create3(obj) ; 
}


//_____________________________________________________________________________
void RooTrace::destroy(const TObject* obj) 
{ 
  // Register deletion of object 'obj'

  if (_active) destroy3(obj) ; 
}


//_____________________________________________________________________________
void RooTrace::createSpecial(const char* name, int size) 
{
  if (_active) {
    _specialCount[name]++ ;
    _specialSize[name] = size ;
  }
}


//_____________________________________________________________________________
void RooTrace::destroySpecial(const char* name) 
{
  if (_active) {
    _specialCount[name]++ ;
  }
}



//_____________________________________________________________________________
void RooTrace::active(Bool_t flag) 
{ 
  // If flag is true, memory tracing is activated

  _active = flag ; 
}


//_____________________________________________________________________________
void RooTrace::verbose(Bool_t flag) 
{ 
  // If flag is true, a message will be printed at each
  // object creation or deletion

  _verbose = flag ; 
}



//_____________________________________________________________________________
void RooTrace::create2(const TObject* obj) 
{
  // Back end function of create(), register creation of object 'obj' 
  
  _list.Add((RooAbsArg*)obj) ;
  if (_verbose) {
    cout << "RooTrace::create: object " << obj << " of type " << obj->ClassName() 
	 << " created " << endl ;
  }
}


  

//_____________________________________________________________________________
void RooTrace::destroy2(const TObject* obj) 
{
  // Back end function of destroy(), register deletion of object 'obj' 

  if (!_list.Remove((RooAbsArg*)obj)) {
  } else if (_verbose) {
    cout << "RooTrace::destroy: object " << obj << " of type " << obj->ClassName() 
	 << " destroyed [" << obj->GetTitle() << "]" << endl ;
  }
}



//_____________________________________________________________________________
void RooTrace::create3(const TObject* obj) 
{
  // Back end function of create(), register creation of object 'obj' 
  _objectCount[obj->IsA()]++ ;
}


  

//_____________________________________________________________________________
void RooTrace::destroy3(const TObject* obj) 
{
  // Back end function of destroy(), register deletion of object 'obj' 
  _objectCount[obj->IsA()]-- ;
}





//_____________________________________________________________________________
void RooTrace::mark()
{
  // Put marker in object list, that allows to dump contents of list
  // relative to this marker

  _markList = _list ;
}



//_____________________________________________________________________________
void RooTrace::dump() 
{
  // Dump contents of object registry to stdout
  dump(cout,kFALSE) ;
}


//_____________________________________________________________________________
void RooTrace::dump(ostream& os, Bool_t sinceMarked) 
{
  // Dump contents of object register to stream 'os'. If sinceMarked is
  // true, only object created after the last call to mark() are shown.

  os << "List of RooFit objects allocated while trace active:" << endl ;


  Int_t i, nMarked(0) ;
  for(i=0 ; i<_list.GetSize() ; i++) {
    if (!sinceMarked || _markList.IndexOf(_list.At(i)) == -1) {
      os << hex << setw(10) << _list.At(i) << dec << " : " << setw(20) << _list.At(i)->ClassName() << setw(0) << " - " << _list.At(i)->GetName() << endl ;
    } else {
      nMarked++ ;
    }
  }
  if (sinceMarked) os << nMarked << " marked objects suppressed" << endl ;
}



//_____________________________________________________________________________
void RooTrace::printObjectCounts() 
{
  Double_t total(0) ;
  for (map<TClass*,int>::iterator iter = _objectCount.begin() ; iter != _objectCount.end() ; ++iter) {
    Double_t tot= 1.0*(iter->first->Size()*iter->second)/(1024*1024) ;
    cout << " class " << iter->first->GetName() << " count = " << iter->second << " sizeof = " << iter->first->Size() << " total memory = " <<  Form("%5.1f",tot) << " Mb" << endl ;
    total+=tot ;
  }

  for (map<string,int>::iterator iter = _specialCount.begin() ; iter != _specialCount.end() ; ++iter) {
    int size = _specialSize[iter->first] ;
    Double_t tot=1.0*(size*iter->second)/(1024*1024) ;
    cout << " special " << iter->first << " count = " << iter->second << " sizeof = " << size  << " total memory = " <<  Form("%5.1f",tot) << " Mb" << endl ;
    total+=tot ;
  }
  cout << "grand total memory = " << total << endl ;

}


//_____________________________________________________________________________
void RooTrace::callgrind_zero() 
{
  // Utility function to trigger zeroing of callgrind counters.
  //
  // Note that this function does _not_ do anything, other than optionally printing this message
  // To trigger callgrind zero counter action, run callgrind with 
  // argument '--zero-before=RooTrace::callgrind_zero()' (include single quotes in cmdline)
  
  ooccoutD((TObject*)0,Tracing) << "RooTrace::callgrind_zero()" << endl ;
}

//_____________________________________________________________________________
void RooTrace::callgrind_dump() 
{
  // Utility function to trigger dumping of callgrind counters.
  //
  // Note that this function does _not_ do anything, other than optionally printing this message
  // To trigger callgrind dumping action, run callgrind with 
  // argument '--dump-before=RooTrace::callgrind_dump()' (include single quotes in cmdline)

  ooccoutD((TObject*)0,Tracing) << "RooTrace::callgrind_dump()" << endl ;
}

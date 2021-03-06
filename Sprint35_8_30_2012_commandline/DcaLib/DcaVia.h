#if !defined(__DcaVia_h__)
#define __DcaVia_h__

#pragma once

#include "Dca.h"
#include "DcaContainer.h"
#include "DcaData.h"

/////////////////////////////////////////////////////////////////////////////
// CViaList
/////////////////////////////////////////////////////////////////////////////
class CViaList  : public CTypedPtrList<CPtrList, DataStruct*>
{

};

/////////////////////////////////////////////////////////////////////////////
// CViaListMap
/////////////////////////////////////////////////////////////////////////////
class CViaListMap : public CTypedPtrMap<CMapStringToPtr,CString,CViaList*>
{

};
#endif /*__DcaVia_h__*/
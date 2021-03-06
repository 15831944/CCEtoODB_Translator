// $Header: /CAMCAD/5.0/DbUtil.cpp 95    3/26/07 11:19a Kurt Van Ness $

/*****************************************************************************/
/*
    Project CAMCAD
    Router Solutions Inc.
    Copyright � 1994-2000. All Rights Reserved.
*/

#include "stdafx.h"
#include "data.h"
#include "ccdoc.h"
#include "graph.h"
#include "EnumIterator.h"

char *insertion_process[] =
{
   "No Assembly",
   "Manual Assembly",
   "Radial Assembly",
   "Axial Assembly",
};

// defined from GenCAM spec 
char *pin_function[] =
{
   "Driver",
   "Receiver",
   "BiDirectional",
   "AnalogIn",
   "AnalogOut",
   "NClosed",
   "Power",
   "Ground",
   "Analog",
   "Digital",
   "Inactive",
   "Anode",
   "Cathode",
   "Base",  // for transitors
   "Collector",
   "Emitter",
   "Source",
   "Drain",
   "Gate",
   "Wiper",
   "Case",
   "Clock",
   "Enable"
   "Disable",
   "TDI",
   "TDO",
   "TMS",
   "TCK",
   "TRST",
   "INTNC",
};

//char *inserttypes[] = 
//{
//   "Unknown",
//   "Via",
//   "Pin",
//   "PCB Component",
//   "Mech Component",
//   "Generic Component",
//   "PCB",
//   "Fiducial",
//   "Tooling",
//   "Test Point",        // generated by CAd systems and a free/test pad
//   "Free Pad",
//   "Logic Symbol",         // "Gate" <== this was the old name as of 27-Mar-00 WS
//   "Port Instance",
//   "Drill Hole",
//   "Mechanical Pin",
//   "Test Probe",
//   "Drill Symbol",      // here was panel insert, which is now elimitated. 2-Feb-99 WS
//   "Centroid",
//   "Clear Pad",
//   "Relief Pad",
//   "Generic Obstacle",
//   "DRC Marker",
//   "Test Access Point", // generated by Test_Access_Analysis
//   "Test Pad",
//   "Junction Point",
//   "Gluepoint",
//   "Reject Mark",
//   "XOut",
//   "Hierarchy Symbol",     // added 05/02/03
//   "Sheet Connector",      // added 05/02/03
//   "TieDot",               // added 05/02/03
//   "Ripper",               // added 05/02/03
//   "Ground",               // added 05/02/03
//   "Terminator",           // added 05/02/03
//   "Aperture",             // added 05/30/03
//   "RealPart",             // added 07/25/03
//   "Pad",                  // added 06/06/03
//   "Package",              // added 06/06/03
//   "Package Pin",          // added 07/25/03
//   "Stencil Hole",         // added 10/07/03
//};

char *blocktypes[] = 
{
   "Unknown",
   "PCB Design",
   "PAD Shape",            
   "(3) Not Used",      // this is not used in CAMCAD anymore  
   "PAD Stack",            
   "PCB Comp", 
   "Mech Comp",       
   "Gen. Comp",   
   "PCB Panel",               
   "Drawing",
   "Fiducial",
   "Tooling",
   "Test Point",        // a single pin component 
   "Dimension",
   "Library",           // EDIF library collection
   "Local Insert",      // ????
   "Tool Graphic",      // how a drill is plotted in a drill drawing
   "Schematic Sheet",
   "Schematic Symbol",  // this was "Schematic Gate" as of 27-Mar-00 WS
   "Gate Port",
   "Drill Hole",
   "Redlining",
   "Test Probe",
   "Centroid",
   "DRC Marker",
   "Geometry Library",
   "Test Pad",          // defined like a padstack
   "Test Access Point",
   "Junction Point",
   "Gluepoint",
   "Reject Mark", 
   "XOut",
   "RealPart",
   "Package",
   "Package Pin",
   "Complex Drill Hole",
   "Composite Component"
};

char *shapes[] = 
{
   "Undefined",
   "Round",
   "Square",
   "Rectangle",
   "Target",
   "Thermal",
   "Complex",
   "Donut",
   "Octagon",
   "Oblong",
   "Blank",
};



// update in dbutuil.h
char *device_subclass[] =
{
   "NON_FUNCTIONAL_PINS",
   "TREAT_AS_TWO_PIN",
   "SIP_ISOLATED",
   "SIP_BUSSED",
   "DIP_ISOLATED",
   "DIP_BUSSED",
   "POT_1",
   "POT_2",
   "POT_3",
   "VARIABLE_CAPACITOR_1",
   "VARIABLE_CAPACITOR_2",
   "VARIABLE_CAPACITOR_3",
   "SOT23_COMMON_ANODE",
   "SOT23_COMMON_CATHODE",
   "SOT23_SINGLE_DIODE",
   "DUAL_DIODE",
   "QUAD_BRIDGE_RECTIFIER",
   "SMT_COMMON",
   "SMT_ISOLATED",
   "DIP_OSCILLATOR_4",
   "DIP_OSCILLATOR_8",
   "DIP_OSCILLATOR_14",
   "TRANSISTOR_ARRAY",
};


char *layergroups[] = 
{
/*00*/   "Unassigned",
/*01*/   "Top",
/*02*/   "Bottom",
/*03*/   "All",
/*04*/   "Inner",
/*05*/   "Outer",
/*06*/   "Planes",
/*07*/   "DRC",
/*08*/   "Misc",
};

// used for ATT_TEST
// used for ATT_SOLDERMASK
// used for ATT_VIATEST5DX
char *testaccesslayers[] = 
{
   "BOTH",
   "BOTTOM",
   "TOP",
   "NONE",
};

// this is assigned in camcad.rc.
// this is defined in ATT_TEST_NET_STATUS

char *netstatus[] =
{
   "Normal",
   "No Probes",
   "Critical",
   "Ground",
   "Power",
};

//_____________________________________________________________________________
InsertTypeTag insertTypeTag(int insertType) 
{ 
   return intToInsertTypeTag(insertType); 
}

InsertTypeTag insertTypeTag(short insertType) 
{ 
   return intToInsertTypeTag(insertType); 
}

CString insertTypeToString(int insertType)
{
   return insertTypeToString(intToInsertTypeTag(insertType));
}

//_____________________________________________________________________________


/******************************************************************************
* NormalizeAngle
*/
double NormalizeAngle(double angle)
{
   while (angle < 0)
      angle += 2.0*PI;

   while (angle >= 2.0*PI)
      angle -= 2.0*PI;

   return angle;
}


#ifndef C_GLUE_H
#define C_GLUE_H

#include <pcp/pmapi.h>

int getPmUnitsDimSpace(pmUnits units);
int getPmUnitsDimTime(pmUnits units);
int getPmUnitsDimCount(pmUnits units);
unsigned int getPmUnitsScaleSpace(pmUnits units);
unsigned int getPmUnitsScaleTime(pmUnits units);
int getPmUnitsScaleCount(pmUnits units);
pmValueSet* getPmValueSetFromPmResult(int index, pmResult *pm_result);
pmValue getDuplicatedPmValueFromPmValueSet(int index, pmValueSet *pm_value_set);
void freePmValue(pmValue pm_value, int valfmt);
__int32_t getInt32FromPmAtomValue(pmAtomValue atom);
__uint32_t getUInt32FromPmAtomValue(pmAtomValue atom);
__int64_t getInt64FromPmAtomValue(pmAtomValue atom);
__uint64_t getUInt64FromPmAtomValue(pmAtomValue atom);
float getFloatFromPmAtomValue(pmAtomValue atom);
double getDoubleFromPmAtomValue(pmAtomValue atom);
char *getStringFromPmAtomValue(pmAtomValue atom);
void freeStringFromPmAtomValue(pmAtomValue atom);
void freePmValueBlockFromPmAtomValue(pmAtomValue atom);

// PMNS callback glue
typedef void(*pmTraversePMNSCGoCallbackType)(const char *name);
void pmTraversePMNSCGoCallback(const char *name);
void goPmTraversePMNSCGoCallback(char *name);

#endif
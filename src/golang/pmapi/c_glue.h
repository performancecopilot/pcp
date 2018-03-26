//Copyright (c) 2018 Ryan Doyle
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#ifndef C_GLUE_H
#define C_GLUE_H

#include <pcp/pmapi.h>

struct pm_value_duplicate_result {
    int success;
    pmValue value;
};

int getPmUnitsDimSpace(pmUnits units);
int getPmUnitsDimTime(pmUnits units);
int getPmUnitsDimCount(pmUnits units);
unsigned int getPmUnitsScaleSpace(pmUnits units);
unsigned int getPmUnitsScaleTime(pmUnits units);
int getPmUnitsScaleCount(pmUnits units);
pmValueSet* getPmValueSetFromPmResult(int index, pmResult *pm_result);
struct pm_value_duplicate_result getDuplicatedPmValueFromPmValueSet(int index, pmValueSet *pm_value_set, int test_memory_pressure);
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
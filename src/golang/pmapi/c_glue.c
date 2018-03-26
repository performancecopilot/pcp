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

#include "c_glue.h"
// cgo does not support packed pmUnit struct. Define some helper functions
// to get the underlying data out of the struct
int getPmUnitsDimSpace(pmUnits units) {
	return units.dimSpace;
}
int getPmUnitsDimTime(pmUnits units) {
	return units.dimTime;
}
int getPmUnitsDimCount(pmUnits units) {
	return units.dimCount;
}

unsigned int getPmUnitsScaleSpace(pmUnits units) {
	return units.scaleSpace;
}
unsigned int getPmUnitsScaleTime(pmUnits units) {
	return units.scaleTime;
}
int getPmUnitsScaleCount(pmUnits units) {
	return units.scaleCount;
}

pmValueSet* getPmValueSetFromPmResult(int index, pmResult *pm_result) {
	return pm_result->vset[index];
}

// See comment in PmFetch() for an explanation of why we do this
struct pm_value_duplicate_result getDuplicatedPmValueFromPmValueSet(int index, pmValueSet *pm_value_set, int test_memory_pressure) {
    struct pm_value_duplicate_result result;
	pmValue pm_value = pm_value_set->vlist[index];

	if(test_memory_pressure) {
	    result.success = 0;
	    return result;
	}

	if(pm_value_set->valfmt == PM_VAL_DPTR) {
		pmValueBlock *orig_vblock = pm_value.value.pval;
		pmValueBlock *new_vblock = (pmValueBlock*)malloc(orig_vblock->vlen);
		if (new_vblock == NULL) {
		    result.success = 0;
		    return result;
		}
		memcpy(new_vblock, orig_vblock, orig_vblock->vlen);
		pm_value.value.pval = new_vblock;
	}

    result.value = pm_value;
    result.success = 1;

	return result;
}

void freePmValue(pmValue pm_value, int valfmt) {
	if(valfmt == PM_VAL_DPTR) {
		free(pm_value.value.pval);
	}
}

__int32_t getInt32FromPmAtomValue(pmAtomValue atom) {
	return atom.l;
}

__uint32_t getUInt32FromPmAtomValue(pmAtomValue atom) {
	return atom.ul;
}

__int64_t getInt64FromPmAtomValue(pmAtomValue atom) {
	return atom.ll;
}

__uint64_t getUInt64FromPmAtomValue(pmAtomValue atom) {
	return atom.ull;
}

float getFloatFromPmAtomValue(pmAtomValue atom) {
	return atom.f;
}

double getDoubleFromPmAtomValue(pmAtomValue atom) {
	return atom.d;
}

char *getStringFromPmAtomValue(pmAtomValue atom) {
	return atom.cp;
}

void freeStringFromPmAtomValue(pmAtomValue atom) {
	free(atom.cp);
}

void freePmValueBlockFromPmAtomValue(pmAtomValue atom) {
	free(atom.vbp);
}

void pmTraversePMNSCGoCallback(const char *name) {
	goPmTraversePMNSCGoCallback((char*)name);
}

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
pmValue getDuplicatedPmValueFromPmValueSet(int index, pmValueSet *pm_value_set) {
	pmValue pm_value = pm_value_set->vlist[index];

	if(pm_value_set->valfmt == PM_VAL_DPTR) {
		pmValueBlock *orig_vblock = pm_value.value.pval;
		pmValueBlock *new_vblock = (pmValueBlock*)malloc(orig_vblock->vlen);
		memcpy(new_vblock, orig_vblock, orig_vblock->vlen);
		pm_value.value.pval = new_vblock;
	}

	return pm_value;
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

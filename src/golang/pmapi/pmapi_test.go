//Copyright (c) 2016 Ryan Doyle
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

package pmapi

import (
	"testing"
	"time"
	"github.com/stretchr/testify/assert"
)

var sampleDoubleMillionPmID PmID = 121634844
var sampleMillisecondsPmID PmID = 121634819
var sampleColourInDom PmInDom = 121634817
var sampleStringHulloPmID PmID = 121634847

func TestPmapiContext_PmGetContextHostname(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "localhost")
	hostname, _ := c.PmGetContextHostname()

	assert.NotEmpty(t, hostname)
}

func TestPmapiContext_PmLookupNameForASingleName(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "localhost")
	pmids, _ := c.PmLookupName("sample.double.million")

	assert.Equal(t, sampleDoubleMillionPmID, pmids[0])
}

func TestPmapiContext_PmLookupNameReturnsAnErrorForUnknownNames(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "localhost")
	_, err := c.PmLookupName("not.a.name")

	assert.Error(t, err)
}

func TestPmapiContext_PmLookupNameForMultipleNames(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "localhost")
	pmids, _ := c.PmLookupName("sample.double.million", "sample.milliseconds",)

	assert.Equal(t, sampleDoubleMillionPmID, pmids[0])
	assert.Equal(t, sampleMillisecondsPmID, pmids[1])
}

func TestPmapiContext_PmLookupDesc_HasTheCorrectPmID(t *testing.T) {
	pmdesc, _ := localContext().PmLookupDesc(sampleDoubleMillionPmID)

	assert.Equal(t, sampleDoubleMillionPmID, pmdesc.PmID)
}

func TestPmapiContext_PmLookupDesc_HasTheCorrectType(t *testing.T) {
	pmdesc, _ := localContext().PmLookupDesc(sampleDoubleMillionPmID)

	assert.Equal(t, PmTypeDouble, pmdesc.Type)
}

func TestPmapiContext_PmLookupDesc_HasTheCorrectInDom(t *testing.T) {
	pmdesc, _ := localContext().PmLookupDesc(sampleDoubleMillionPmID)

	assert.Equal(t, PmInDomNull, pmdesc.InDom)
}

func TestPmapiContext_PmLookupDesc_HasTheCorrectSemantics(t *testing.T) {
	pmdesc, _ := localContext().PmLookupDesc(sampleDoubleMillionPmID)

	assert.Equal(t, PmSemInstant, pmdesc.Sem)
}

func TestPmapiContext_PmLookupDesc_HasTheCorrectUnits(t *testing.T) {
	pmdesc, _ := localContext().PmLookupDesc(sampleMillisecondsPmID)
	expected_units := PmUnits{
		DimTime: 1,
		ScaleTime: PmTimeMSec,
	}

	assert.Equal(t, expected_units, pmdesc.Units)
}

func TestPmapiContext_PmLookupInDom_ReturnsTheInstanceNameMapping(t *testing.T) {
	indom, _ := localContext().PmGetInDom(sampleColourInDom)

	assert.Equal(t, "red", indom[0])
	assert.Equal(t, "green", indom[1])
	assert.Equal(t, "blue", indom[2])
}

func TestPmapiContext_PmGetInDom_ReturnsANilErrorForValidInDoms(t *testing.T) {
	_, err := localContext().PmGetInDom(sampleColourInDom)

	assert.NoError(t, err)
}

func TestPmapiContext_PmGetInDom_ReturnsAnErrorForIncorrectInDoms(t *testing.T) {
	_, err := localContext().PmGetInDom(PmInDom(123))

	assert.Error(t, err)
}

func TestPmapiContext_PmFetch_returnsAPmResultWithATimestamp(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assertWithinDuration(t, pm_result.Timestamp, time.Now(), time.Second)
}

func TestPmapiContext_PmFetch_returnsAPmResultWithTheNumberOfPMIDs(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assert.Equal(t, 1, pm_result.NumPmID)
}

func TestPmapiContext_PmFetch_returnsAVSet_withAPmID(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assert.Equal(t, sampleDoubleMillionPmID, pm_result.VSet[0].PmID)
}

func TestPmapiContext_PmFetch_returnsAVSet_withNumval(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assert.Equal(t, 1, pm_result.VSet[0].NumVal)
}

func TestPmapiContext_PmFetch_returnsAVSet_withValFmt(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assert.Equal(t, PmValDptr, pm_result.VSet[0].ValFmt)
}

func TestPmapiContext_PmFetch_returnsAVSet_withVlist_withAPmValue_withAnInst(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	assert.Equal(t, -1, pm_result.VSet[0].VList[0].Inst)
}

func TestPmapiContext_PmFetch_supportsMultiplePMIDs(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID, sampleStringHulloPmID)

	assert.Len(t, pm_result.VSet[0].VList, 1)
	assert.Len(t, pm_result.VSet[1].VList, 1)
}

func TestPmExtractValue_forADoubleValue(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleDoubleMillionPmID)

	value := pm_result.VSet[0].VList[0]

	atom, _ := PmExtractValue(PmValDptr, PmTypeDouble, value)

	assert.Equal(t, 1000000.0, atom.Double)
}

func TestPmExtractValue_forAStringValue(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleStringHulloPmID)

	value := pm_result.VSet[0].VList[0]

	atom, _ := PmExtractValue(PmValDptr, PmTypeString, value)

	assert.Equal(t, "hullo world!", atom.String)
}

func TestPmExtractValue_returnsAnErrorWhenTryingToExtractTheWrongType(t *testing.T) {
	pm_result, _ := localContext().PmFetch(sampleStringHulloPmID)

	value := pm_result.VSet[0].VList[0]

	_, err := PmExtractValue(PmValDptr, PmType64, value)

	assert.Error(t, err)
}

func TestPmNewContext_withAnInvalidHostHasANilContext(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "not-a-host")

	assert.Nil(t, c)
}

func TestPmNewContext_withAnInvalidHostHasAnError(t *testing.T) {
	_, err := PmNewContext(PmContextHost, "not-a-host")

	assert.Error(t, err)
}

func TestPmNewContext_hasAValidContext(t *testing.T) {
	c, _ := PmNewContext(PmContextHost, "localhost")

	assert.NotNil(t, c)
}

func TestPmNewContext_hasANilError(t *testing.T) {
	_, err := PmNewContext(PmContextHost, "localhost")

	assert.NoError(t, err)
}

func TestPmNewContext_supportsALocalContext(t *testing.T) {
	c, _ := PmNewContext(PmContextLocal, "")

	assert.NotNil(t, c)
}

func assertWithinDuration(t *testing.T, time1 time.Time, time2 time.Time, duration time.Duration) {
	rounded1 := time1.Round(duration)
	rounded2 := time2.Round(duration)
	if(!rounded1.Equal(rounded2)) {
		t.Errorf("Expected time: %v and time: %v to be within %v of each other", time1, time2, duration)
	}
}

func localContext() *PmapiContext {
	c, _ := PmNewContext(PmContextHost, "localhost")
	return c
}
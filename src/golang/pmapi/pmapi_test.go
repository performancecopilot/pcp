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
	"sync"
)

var sampleDoubleMillionPmID PmID = 121634844
var sampleMillisecondsPmID PmID = 121634819
var sampleColourInDom PmInDom = 121634817
var sampleStringHulloPmID PmID = 121634847
var sampleFloatTenAlsoSampleDupnamesTwoFloatTen PmID = 121634832
var notARealPmID PmID = 123

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

func TestPmapiContext_PmGetChildren_returnsTheChildrenFromNonLeafNode(t *testing.T) {
	children, _ := localContext().PmGetChildren("sample.many")

	assert.Equal(t, []string{"count", "int"}, children)
}

func TestPmapiContext_PmGetChildren_returnsNoErrorForANonLeafNode(t *testing.T) {
	_, err := localContext().PmGetChildren("sample.many")

	assert.NoError(t, err)
}

func TestPmapiContext_PmGetChildren_returnsAnEmptySliceForALeafNode(t *testing.T) {
	children, _ := localContext().PmGetChildren("sample.many.count")

	assert.Empty(t, children)
}

func TestPmapiContext_PmGetChildren_returnsNoErrorForALeafNode(t *testing.T) {
	_, err := localContext().PmGetChildren("sample.many.count")

	assert.NoError(t, err)
}

func TestPmapiContext_PmGetChildren_returnsAnErrorForUnknownMetrics(t *testing.T) {
	_, err := localContext().PmGetChildren("not.a.metric")

	assert.Error(t, err)
}

func TestPmapiContext_PmGetChildrenStatus_returnsChildrenFromNonLeafNode(t *testing.T) {
	children, _ := localContext().PmGetChildrenStatus("sample.ulong")

	assert.Equal(t, []PMNSNode{
		{name: "one", 		leaf:PmLeafStatus},
		{name: "ten", 		leaf:PmLeafStatus},
		{name: "hundred", 	leaf:PmLeafStatus},
		{name: "million", 	leaf:PmLeafStatus},
		{name: "write_me", 	leaf:PmLeafStatus},
		{name: "bin", 		leaf:PmLeafStatus},
		{name: "bin_ctr", 	leaf:PmLeafStatus},
		{name: "count", 	leaf:PmNonLeafStatus},
	}, children)
}

func TestPmapiContext_PmGetChildrenStatus_returnsNoErrorForANonLeafNode(t *testing.T) {
	_, err := localContext().PmGetChildrenStatus("sample.many")

	assert.NoError(t, err)
}

func TestPmapiContext_PmGetChildrenStatus_returnsAnEmptySliceForALeafNode(t *testing.T) {
	children, _ := localContext().PmGetChildrenStatus("sample.many.count")

	assert.Empty(t, children)
}

func TestPmapiContext_PmGetChildrenStatus_returnsNoErrorForALeafNode(t *testing.T) {
	_, err := localContext().PmGetChildrenStatus("sample.many.count")

	assert.NoError(t, err)
}

func TestPmapiContext_PmGetChildrenStatus_returnsAnErrorForUnknownMetrics(t *testing.T) {
	_, err := localContext().PmGetChildrenStatus("not.a.metric")

	assert.Error(t, err)
}

func TestPmapiContext_PmNameID_returnsTheNameForAValidPmID(t *testing.T) {
	name, _ := localContext().PmNameID(sampleDoubleMillionPmID)

	assert.Equal(t, "sample.double.million", name)
}

func TestPmapiContext_PmNameID_returnsNoErrorForAValidPmID(t *testing.T) {
	_, err := localContext().PmNameID(sampleDoubleMillionPmID)

	assert.NoError(t, err)
}

func TestPmapiContext_PmNameID_returnsAnErrorIfThePmIDIsInvalid(t *testing.T) {
	_, err := localContext().PmNameID(notARealPmID)

	assert.Error(t, err)
}

func TestPmapiContext_PmNameID_returnsAnEmptyNameIfThePmIDIsInvalid(t *testing.T) {
	name, _ := localContext().PmNameID(notARealPmID)

	assert.Empty(t, name)
}

func TestPmapiContext_PmNameAll_returnsTheNamesForAValidPmID(t *testing.T) {
	name, _ := localContext().PmNameAll(sampleDoubleMillionPmID)

	assert.Equal(t, []string{"sample.double.million"}, name)
}

func TestPmapiContext_PmNameAll_returnsAllTheNamesForAValidPmID(t *testing.T) {
	name, _ := localContext().PmNameAll(sampleFloatTenAlsoSampleDupnamesTwoFloatTen)

	assert.Equal(t, []string{"sample.dupnames.two.float.ten", "sample.float.ten"}, name)
}

func TestPmapiContext_PmNameAll_returnsNoErrorForAValidPmID(t *testing.T) {
	_, err := localContext().PmNameAll(sampleDoubleMillionPmID)

	assert.NoError(t, err)
}

func TestPmapiContext_PmNameAll_returnsAnErrorIfThePmIDIsInvalid(t *testing.T) {
	_, err := localContext().PmNameAll(notARealPmID)

	assert.Error(t, err)
}

func TestPmapiContext_PmNameAll_returnsEmptyIfThePmIDIsInvalid(t *testing.T) {
	name, _ := localContext().PmNameID(notARealPmID)

	assert.Empty(t, name)
}

func TestPmapiContext_PmTraversePMNS_traversesOverTheMetricNameSpace(t *testing.T) {
	var metrics []string
	localContext().PmTraversePMNS("sample.many", func(metric string) {
		metrics = append(metrics, metric)
	})

	assert.Equal(t, []string{"sample.many.count", "sample.many.int"}, metrics)
}

func TestPmapiContext_PmTraversePMNS_returnsTheNumberOfNamesTraversed(t *testing.T) {
	var metrics []string
	num, _ := localContext().PmTraversePMNS("sample.many", func(metric string) {
		metrics = append(metrics, metric)
	})

	assert.Equal(t, 2, num)
}

func TestPmapiContext_PmTraversePMNS_returnsNoErrorForAValidMetricRoot(t *testing.T) {
	var metrics []string
	_, err := localContext().PmTraversePMNS("sample.many", func(metric string) {
		metrics = append(metrics, metric)
	})

	assert.NoError(t, err)
}

func TestPmapiContext_PmTraversePMNS_returnsAnErrorForAnInvalidRootName(t *testing.T) {
	_, err := localContext().PmTraversePMNS("not-a-metric", func(metric string) {})

	assert.Error(t, err)
}

func TestPmapiContext_PmTraversePMNS_returnsAnErrorForAnInvalidCallbackFunction(t *testing.T) {
	_, err := localContext().PmTraversePMNS("sample.many", nil)

	assert.Error(t, err)
}

func BenchmarkPmapiContext_PmFetch(b *testing.B) {
	context, _ := PmNewContext(PmContextHost, "localhost")

	for i := 0; i < b.N; i++ {
		concurrentlyFetch(10, context)
	}
}

func BenchmarkPmapiContextUnsafe_PmFetch(b *testing.B) {
	context, _ := PmNewContextUnsafe(PmContextHost, "localhost")

	for i := 0; i < b.N; i++ {
		concurrentlyFetch(10, context)
	}
}

func BenchmarkPmapiContext_PmTraversePMNS(b *testing.B) {
	context := localContext()

	for i := 0; i < b.N; i++ {
		context.PmTraversePMNS("", func(name string) {})
	}
}

func concurrentlyFetch(concurrency int, context PMAPI) {
	wg := sync.WaitGroup{}
	wg.Add(concurrency)
	for i := 0; i < concurrency; i++ {
		go func() {
			context.PmFetch(sampleDoubleMillionPmID)
			wg.Done()
		}()
	}
	wg.Wait()
}

func assertWithinDuration(t *testing.T, time1 time.Time, time2 time.Time, duration time.Duration) {
	rounded1 := time1.Round(duration)
	rounded2 := time2.Round(duration)
	if(!rounded1.Equal(rounded2)) {
		t.Errorf("Expected time: %v and time: %v to be within %v of each other", time1, time2, duration)
	}
}

func localContext() PMAPI {
	c, _ := PmNewContext(PmContextHost, "localhost")
	return c
}
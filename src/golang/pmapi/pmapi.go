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
//#cgo CFLAGS: -I .
//#cgo LDFLAGS: -L . -lpcp
//#include <pcp/pmapi.h>
//#include "c_glue.h"
import "C"
import (
	"unsafe"
	"errors"
	"runtime"
	"time"
	"sync"
)

type PMAPI interface {
	PmLookupName(names ...string) ([]PmID, error)
	PmGetChildren(name string) ([]string, error)
	PmGetChildrenStatus(name string) ([]PMNSNode, error)
	PmNameID(pmid PmID) (string, error)
	PmFetch(pmids ...PmID) (*PmResult, error)
	PmLookupDesc(pmid PmID) (PmDesc, error)
	PmExtractValue(value_format int, pm_type int, pm_value *PmValue) (PmAtomValue, error)
	PmGetInDom(indom PmInDom) (map[int]string, error)
	PmNameAll(pmid PmID) ([]string, error)
	PmTraversePMNS(name string, traverse_callback func(string)) (int, error)
}

type PMNSNode struct {
	name string
	leaf int
}

type PmapiContext struct {
	context int
	contextLocker sync.Locker
}

type PmDesc struct {
	PmID PmID
	Type int
	InDom PmInDom
	Sem int
	Units PmUnits
}

type PmUnits struct {
	DimSpace int
	DimTime int
	DimCount int
	ScaleSpace uint
	ScaleTime uint
	ScaleCount int
}

type PmResult struct {
	Timestamp time.Time
	NumPmID	int
	VSet []*PmValueSet
}

type PmValueSet struct {
	PmID	PmID
	NumVal	int
	ValFmt	int
	VList	[]*PmValue
}

type PmValue struct {
	Inst int
	cPmValue C.pmValue
	valfmt C.int
}

type PmAtomValue struct {
	Int32 int32
	UInt32 uint32
	Int64 int64
	UInt64 uint64
	Float float32
	Double float64
	String string
}

type PmContextType int
type PmID uint32
type PmInDom uint32

var contextLock = sync.Mutex{}

/* Naive way to implement PMNS callback. This would need to be refactored if PmTraversePMNS was heavily used with
multiple contexts */
var currentPMNSCallback func(metric string)
var currentPMNSCallbackLock = sync.Mutex{}

const (
	PmContextHost = PmContextType(int(C.PM_CONTEXT_HOST))
	PmContextArchive = PmContextType(int(C.PM_CONTEXT_ARCHIVE))
	PmContextLocal = PmContextType(int(C.PM_CONTEXT_LOCAL))
	PmContextUndef = PmContextType(int(C.PM_CONTEXT_UNDEF))
	PmInDomNull = PmInDom(C.PM_INDOM_NULL)
	PmInNull = int(C.PM_IN_NULL)

	PmSpaceByte = uint(C.PM_SPACE_BYTE)
	PmSpaceKByte = uint(C.PM_SPACE_KBYTE)
	PmSpaceMByte = uint(C.PM_SPACE_MBYTE)
	PmSpaceGByte = uint(C.PM_SPACE_GBYTE)
	PmSpaceTByte = uint(C.PM_SPACE_TBYTE)
	PmSpacePByte = uint(C.PM_SPACE_PBYTE)
	PmSpaceEByte = uint(C.PM_SPACE_EBYTE)

	PmTimeNSec = uint(C.PM_TIME_NSEC)
	PmTimeUSec = uint(C.PM_TIME_USEC)
	PmTimeMSec = uint(C.PM_TIME_MSEC)
	PmTimeSec = uint(C.PM_TIME_SEC)
	PmTimeMin = uint(C.PM_TIME_MIN)
	PmTimeHour = uint(C.PM_TIME_HOUR)

	PmTypeNoSupport = int(C.PM_TYPE_NOSUPPORT)
	PmType32 = int(C.PM_TYPE_32)
	PmTypeU32 = int(C.PM_TYPE_U32)
	PmType64 = int(C.PM_TYPE_64)
	PmTypeU64 = int(C.PM_TYPE_U64)
	PmTypeFloat = int(C.PM_TYPE_FLOAT)
	PmTypeDouble = int(C.PM_TYPE_DOUBLE)
	PmTypeString = int(C.PM_TYPE_STRING)
	PmTypeAggregate= int(C.PM_TYPE_AGGREGATE)
	PmTypeAggregateStatic = int(C.PM_TYPE_AGGREGATE_STATIC)
	PmTypeEvent = int(C.PM_TYPE_EVENT)
	PmTypeHighResEvent = int(C.PM_TYPE_HIGHRES_EVENT)
	PmTypeUnknown = int(C.PM_TYPE_UNKNOWN)

	PmSemCounter = int(C.PM_SEM_COUNTER)
	PmSemInstant = int(C.PM_SEM_INSTANT)
	PmSemDiscrete = int(C.PM_SEM_DISCRETE)

	PmValInsitu = int(C.PM_VAL_INSITU)
	PmValDptr = int(C.PM_VAL_DPTR)
	PmValSptr = int(C.PM_VAL_SPTR)

	PmLeafStatus = int(C.PMNS_LEAF_STATUS)
	PmNonLeafStatus = int(C.PMNS_NONLEAF_STATUS)
)

/* Higher-performance version of PmNewContext that's safe if it's the only context in the process */
func PmNewContextUnsafe(context_type PmContextType, host_or_archive string) (*PmapiContext, error) {
	return pmNewContext(context_type, host_or_archive, noopLocker{})
}

func PmNewContext(context_type PmContextType, host_or_archive string) (*PmapiContext, error) {
	return pmNewContext(context_type, host_or_archive, &contextLock)
}

func pmNewContext(context_type PmContextType, host_or_archive string, locker sync.Locker) (*PmapiContext, error) {
	host_or_archive_ptr := C.CString(host_or_archive)
	defer C.free(unsafe.Pointer(host_or_archive_ptr))

	context_id := int(C.pmNewContext(C.int(context_type), host_or_archive_ptr))
	if (context_id < 0) {
		return nil, newPmError(context_id)
	}

	context := &PmapiContext{
		context: context_id,
		contextLocker: locker,
	}

	runtime.SetFinalizer(context, func(c *PmapiContext) {
		C.pmDestroyContext(C.int(c.context))
	})

	return context, nil
}

func (c *PmapiContext) PmGetContextHostname() (string, error) {
	string_buffer := make([]C.char, C.MAXHOSTNAMELEN)
	raw_char_ptr := (*C.char)(unsafe.Pointer(&string_buffer[0]))

	_, err := c.withinContext(func() int {
		C.pmGetContextHostName_r(C.int(c.context), raw_char_ptr, C.MAXHOSTNAMELEN)
		/* pmGetContextHostName_r does not return a status code so fake one to satisfy
		 the function definition. It's a bit hacky but worth the consistency IMO */
		return 0
	})
	if(err != nil) {
		return "", err
	}

	return C.GoString(raw_char_ptr), nil
}

func (c *PmapiContext) PmLookupName(names ...string) ([]PmID, error) {
	number_of_names := len(names)
	c_pmids := make([]C.pmID, number_of_names)
	c_names := make([]*C.char, number_of_names)

	/* Build c_names as copies of the original names */
	for i, name := range names {
		name_ptr := C.CString(name)
		c_names[i] = name_ptr
		defer C.free(unsafe.Pointer(name_ptr))
	}

	/* Do the actual lookup */
	_, err := c.withinContext(func() int {
		return int(C.pmLookupName(C.int(number_of_names), &c_names[0], &c_pmids[0]))
	})
	if(err != nil) {
		return nil, err
	}

	/* Collect up the C.pmIDs into Go PmID's. Originally when returning the slice that was passed
	into pmLookupName was resulting in bit length errors between Go's uint and C unsigned int */
	pmids := make([]PmID, number_of_names)
	for i, c_pmid := range c_pmids {
		pmids[i] = PmID(c_pmid)
	}

	return pmids, nil
}

func (c *PmapiContext) PmGetChildren(name string) ([]string, error) {
	name_ptr := C.CString(name)
	defer C.free(unsafe.Pointer(name_ptr))

	var children_ptr **C.char

	number_of_children, err := c.withinContext(func() int {
		return int(C.pmGetChildren(name_ptr, &children_ptr))
	})
	if(err != nil) {
		return nil, err
	}

	/* No children, return an empty list */
	if(number_of_children == 0) {
		return []string{}, nil
	}
	/* Only bother free-ing if we actually have children as specified in the programmers guide */
	defer C.free(unsafe.Pointer(children_ptr))

	children_ptr_slice := (*[1 << 30]*C.char)(unsafe.Pointer(children_ptr))

	/* Convert from C strings into golang  */
	children := make([]string, number_of_children)
	for i := 0; i < number_of_children; i++ {
		children[i] =  C.GoString(children_ptr_slice[i])
	}

	return children, nil

}

func (c *PmapiContext) PmGetChildrenStatus(name string) ([]PMNSNode, error) {
	name_ptr := C.CString(name)
	defer C.free(unsafe.Pointer(name_ptr))

	var children_ptr **C.char
	var children_status_ptr *C.int

	number_of_children, err := c.withinContext(func() int {
		return int(C.pmGetChildrenStatus(name_ptr, &children_ptr, &children_status_ptr))
	})
	if (err != nil) {
		return nil, err
	}

	/* No children, return an empty slice */
	if (number_of_children == 0) {
		return []PMNSNode{}, nil
	}

	/* Only bother free-ing if we actually have children. Specified for pmGetChildren() in the programmers guide so
	 assuming the same here seeing as pmGetChildren() delegates to pmGetChildrenStatus() anyway */
	defer C.free(unsafe.Pointer(children_ptr))
	defer C.free(unsafe.Pointer(children_status_ptr))

	children_ptr_slice := (*[1 << 30]*C.char)(unsafe.Pointer(children_ptr))
	children_status_ptr_slice := (*[1 << 30]C.int)(unsafe.Pointer(children_status_ptr))

	/* Convert from C strings into golang  */
	children := make([]PMNSNode, number_of_children)
	for i := 0; i < number_of_children; i++ {
		children[i] = PMNSNode{name: C.GoString(children_ptr_slice[i]), leaf: int(children_status_ptr_slice[i])}
	}

	return children, nil
}

func (c *PmapiContext) PmNameID(pmid PmID) (string, error) {

	var name_ptr *C.char

	_, err := c.withinContext(func() int {
		return int(C.pmNameID(C.pmID(pmid), &name_ptr))
	})
	if(err != nil) {
		return "", err
	}
	defer C.free(unsafe.Pointer(name_ptr))

	return C.GoString(name_ptr), nil
}

func (c *PmapiContext) PmNameAll(pmid PmID) ([]string, error) {

	var names_ptr **C.char

	name_count, err := c.withinContext(func() int {
		return int(C.pmNameAll(C.pmID(pmid), &names_ptr))
	})
	if(err != nil) {
		return nil, err
	}
	defer C.free(unsafe.Pointer(names_ptr))

	children_ptr_slice := (*[1 << 30]*C.char)(unsafe.Pointer(names_ptr))
	names := make([]string, name_count)
	for i := 0; i < name_count; i++ {
		names[i] = C.GoString(children_ptr_slice[i])
	}

	return names, nil
}

func (c *PmapiContext) PmTraversePMNS(name string, traverse_callback func(string)) (int, error) {
	if traverse_callback == nil {
		return 0, errors.New("traverse_callback cannot be nil")
	}

	name_ptr := C.CString(name)
	defer C.free(unsafe.Pointer(name_ptr))

	currentPMNSCallbackLock.Lock()
	defer currentPMNSCallbackLock.Unlock()

	currentPMNSCallback = traverse_callback
	defer func() { currentPMNSCallback  = nil }()

	metrics_found, err := c.withinContext(func() int {
		return int(C.pmTraversePMNS(name_ptr, (C.pmTraversePMNSCGoCallbackType)(unsafe.Pointer(C.pmTraversePMNSCGoCallback))))
	})

	if err != nil {
		return 0, err
	}

	return metrics_found, nil
}

//export goPmTraversePMNSCGoCallback
func goPmTraversePMNSCGoCallback(name *C.char) {
	currentPMNSCallback(C.GoString(name))
}

func (c *PmapiContext) PmLookupDesc(pmid PmID) (PmDesc, error) {
	c_pmdesc := C.pmDesc{}

	_, err := c.withinContext(func() int {
		return int(C.pmLookupDesc(C.pmID(pmid), &c_pmdesc))
	})
	if(err != nil) {
		return PmDesc{}, err
	}

	return PmDesc{
		PmID: PmID(c_pmdesc.pmid),
		Type: int(c_pmdesc._type),
		InDom: PmInDom(c_pmdesc.indom),
		Sem: int(c_pmdesc.sem),
		Units: PmUnits{
			DimSpace: int(C.getPmUnitsDimSpace(c_pmdesc.units)),
			DimTime: int(C.getPmUnitsDimTime(c_pmdesc.units)),
			DimCount: int(C.getPmUnitsDimCount(c_pmdesc.units)),
			ScaleSpace: uint(C.getPmUnitsScaleSpace(c_pmdesc.units)),
			ScaleTime: uint(C.getPmUnitsScaleTime(c_pmdesc.units)),
			ScaleCount: int(C.getPmUnitsScaleCount(c_pmdesc.units)),
		}}, nil

}

func (c *PmapiContext) PmGetInDom(indom PmInDom) (map[int]string, error) {
	var c_instance_ids *C.int
	var c_instance_names **C.char

	number_of_instances, err := c.withinContext(func() int {
		return int(C.pmGetInDom(C.pmInDom(indom), &c_instance_ids, &c_instance_names))
	})
	if(err != nil) {
		return nil, err
	}

	defer C.free(unsafe.Pointer(c_instance_ids))
	defer C.free(unsafe.Pointer(c_instance_names))

	/* Convert to a slice as we cannot do pointer arithmetic. As per
	   https://groups.google.com/forum/#!topic/golang-nuts/sV_f0VkjZTA */
	c_instance_ids_slice := (*[1 << 30]C.int)(unsafe.Pointer(c_instance_ids))
	c_instance_names_slice := (*[1 << 30]*C.char)(unsafe.Pointer(c_instance_names))

	indom_map := make(map[int]string)
	for i := 0; i < number_of_instances; i++ {
		indom_map[int(c_instance_ids_slice[i])] = C.GoString(c_instance_names_slice[i])
	}

	return indom_map, nil
}

func (c *PmapiContext) PmFetch(pmids ...PmID) (*PmResult, error) {
	number_of_pmids := len(pmids)

	var c_pm_result *C.pmResult
	c_pmids := (*C.pmID)(unsafe.Pointer(&pmids[0]))

	_, err := c.withinContext(func() int {
		return int(C.pmFetch(C.int(number_of_pmids), c_pmids, &c_pm_result))
	})
	if(err != nil) {
		return nil, err
	}
	/*
	Its safe to free the *pmResult here as we copy any result data with
	C.getDuplicatedPmValueFromPmValueSet. Originally PmValue's had a reference
	to the parent PmResult so the underlying *pmResult would not be free'ed
	until all references to the PmResult were gone. The only problem is that Go
	doesn't grantee finalizers running on objects that have cyclic references.

	It's a bit of extra copying of memory but its the only way to do it as far
	as I know without having to explicitly free the *pmResult (which is not safe
	if you have a reference to a PmValue as it's *pval will have already been freed)
	*/
	defer C.pmFreeResult(c_pm_result)

	return &PmResult{
		NumPmID:int(c_pm_result.numpmid),
		Timestamp:time.Unix(int64(c_pm_result.timestamp.tv_sec), int64(c_pm_result.timestamp.tv_usec) * 1000),
		VSet:vsetFromPmResult(c_pm_result),
	}, nil
}

func vsetFromPmResult(c_pm_result *C.pmResult) []*PmValueSet {
	number_of_pmids_from_pmresult := int(c_pm_result.numpmid)
	vset := make([]*PmValueSet, number_of_pmids_from_pmresult)

	for i := 0; i < number_of_pmids_from_pmresult; i++ {
		c_vset := C.getPmValueSetFromPmResult(C.int(i), c_pm_result)
		vset[i] = &PmValueSet{
			PmID:PmID(c_vset.pmid),
			NumVal:int(c_vset.numval),
			ValFmt:int(c_vset.valfmt),
			VList: vlistFromPmValueSet(c_vset),
		}
	}
	return vset
}

func vlistFromPmValueSet(c_vset *C.pmValueSet) []*PmValue {
	number_of_pm_values := int(c_vset.numval)
	if(number_of_pm_values <= 0) {
		return []*PmValue{}
	}
	vlist := make([]*PmValue, number_of_pm_values)

	for i := 0; i < number_of_pm_values; i++ {
		vlist[i] = newPmValue(i, c_vset)
	}

	return vlist
}

func newPmValue(index int, c_vset *C.pmValueSet) *PmValue {
	/* See comment in PmFetch() for an explanation */
	c_pm_value := C.getDuplicatedPmValueFromPmValueSet(C.int(index), c_vset)
	pm_value := &PmValue{
		Inst:int(c_pm_value.inst),
		cPmValue:c_pm_value,
		valfmt:c_vset.valfmt,
	}
	runtime.SetFinalizer(pm_value, func(pm_value *PmValue){
		C.freePmValue(pm_value.cPmValue, pm_value.valfmt)
	})
	return pm_value
}

func (c *PmapiContext) PmExtractValue(value_format int, pm_type int, pm_value *PmValue) (PmAtomValue, error) {
	return PmExtractValue(value_format, pm_type, pm_value)
}

func PmExtractValue(value_format int, pm_type int, pm_value *PmValue) (PmAtomValue, error) {
	var c_pm_atom_value C.pmAtomValue

	err := int(C.pmExtractValue(C.int(value_format), &pm_value.cPmValue, C.int(pm_type), &c_pm_atom_value, C.int(pm_type)))
	if(err < 0) {
		return PmAtomValue{}, newPmError(err)
	}

	switch pm_type {
	case PmType32:
		return PmAtomValue{Int32:int32(C.getInt32FromPmAtomValue(c_pm_atom_value))}, nil
	case PmTypeU32:
		return PmAtomValue{UInt32:uint32(C.getUInt32FromPmAtomValue(c_pm_atom_value))}, nil
	case PmType64:
		return PmAtomValue{Int64:int64(C.getInt64FromPmAtomValue(c_pm_atom_value))}, nil
	case PmTypeU64:
		return PmAtomValue{UInt64:uint64(C.getUInt64FromPmAtomValue(c_pm_atom_value))}, nil
	case PmTypeFloat:
		return PmAtomValue{Float:float32(C.getFloatFromPmAtomValue(c_pm_atom_value))}, nil
	case PmTypeDouble:
		return PmAtomValue{Double:float64(C.getDoubleFromPmAtomValue(c_pm_atom_value))}, nil
	case PmTypeString:
		str := PmAtomValue{String:C.GoString(C.getStringFromPmAtomValue(c_pm_atom_value))}
		C.freeStringFromPmAtomValue(c_pm_atom_value)
		return str, nil
	case PmTypeAggregate:
	case PmTypeEvent:
	case PmTypeHighResEvent:
		C.freePmValueBlockFromPmAtomValue(c_pm_atom_value)
		return PmAtomValue{}, errors.New("Unsupported type")
	}
	return PmAtomValue{}, errors.New("Unknown type")
}

func (c *PmapiContext) pmUseContext() error {
	err := int(C.pmUseContext(C.int(c.context)))
	if(err < 0) {
		return newPmError(err)
	}
	return nil
}

func (c *PmapiContext) withinContext(pmapiCall func() int) (int, error) {
	/* Synchronise calls within the pmapi context. With multiple instances of a pmapi context,
	its possible that the incorrect context is loaded without sync'ing calls. EG:

	thread1: pmUseContext(1)
	thread2: pmUseContext(2)
	thread1: pmFetch() <- called against the wrong context
	*/
	c.contextLocker.Lock()
	defer c.contextLocker.Unlock()

	err := c.pmUseContext()
	if(err != nil) {
		return -1, err
	}
	error_code_or_count_of_something := pmapiCall()
	if(error_code_or_count_of_something < 0 ) {
		return -1, newPmError(error_code_or_count_of_something)
	}
	return error_code_or_count_of_something, nil
}

func newPmError(err int) error {
	return errors.New(pmErrStr(err))
}

func pmErrStr(error_no int) string {
	string_buffer := make([]C.char, C.PM_MAXERRMSGLEN)
	raw_char_ptr := (*C.char)(unsafe.Pointer(&string_buffer[0]))

	C.pmErrStr_r(C.int(error_no), raw_char_ptr, C.PM_MAXERRMSGLEN)

	return C.GoString(raw_char_ptr)
}

func (c *PmapiContext) GetContextId() int {
	return c.context
}
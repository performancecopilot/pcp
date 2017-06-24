package pmapi

type noopLocker struct {}

func (_ noopLocker) Lock() {

}

func (_ noopLocker) Unlock() {

}
package main

// #cgo pkg-config: libavcodec libavutil libavformat libswscale gstreamer-codecparsers-1.0
// #cgo CFLAGS: -I${SRCDIR}/../../..
// #cgo CXXFLAGS: -I${SRCDIR}/../../..
// #cgo LDFLAGS: -lfMP4 -lmp4v2 -L${SRCDIR}/../../../build -L/usr/local/lib/
// #include <stdbool.h>
// #include <fMP4.h>
// #include "gomp4_callback.hpp"
import "C"

import (
	"errors"
	"unsafe"
)

type MP4 struct {
	handle C.fMP4Writer
}

func NewMP4() MP4 {
	var m MP4
	handle := C.CNewMP4()
	m.handle = handle

	return m
}

func (m MP4) WriteH264Sample(buf []byte, sample_size uint, is_key_frame bool, duration uint64) error {
	ret := C.fMP4_WriteH264Sample(m.handle,
		(*C.uchar)(unsafe.Pointer(&buf[0])),
		C.uint(sample_size),
		C._Bool(is_key_frame),
		C.ulonglong(duration))

	if !ret {
		return errors.New("Fail to write sample")
	}

	return nil
}

func (m MP4) Release() {
	C.fMP4_ReleaseWriter(m.handle)
}

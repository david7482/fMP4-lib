package main

// #include <stdio.h>
import "C"

import (
	"crypto/tls"
	"errors"
	"flag"
	"fmt"
	"golang.org/x/net/websocket"
	"net/http"
	"os"
	"unsafe"
)

var camera_ch = make(chan *websocket.Conn)
var client_ch = make(chan *websocket.Conn)
var err_ch = make(chan error, 1)
var frame_ch chan []byte

//export GoMP4Callback
func GoMP4Callback(buf *C.uchar, size C.int) C.int {
	gobuf := C.GoBytes(unsafe.Pointer(buf), size)
	fmt.Printf("Got buffer %d\n", len(gobuf))

	frame_ch <- gobuf

	return size
}

func write_frame(writer *websocket.Conn) {
	for {
		s := <-frame_ch
		fmt.Printf("Write buf: %d\n", len(s))

		if len(s) == 0 {
			return
		}

		// Note: We must use websocket.Message to send binary frames
		// The websocket.Conn.Write can't achieve that
		msg := websocket.Message
		err := msg.Send(writer, s)
		if err != nil {
			fmt.Println(err)
		}
	}
}

func conv2int(buf []byte) int {
	var tmp int = 0

	tmp |= int(buf[0])
	tmp |= int(buf[1]) << 8
	tmp |= int(buf[2]) << 16
	tmp |= int(buf[3]) << 24

	return tmp
}

func read_buffer(reader *websocket.Conn) (bool, int, []byte, error) {
	var msg []byte

	err := websocket.Message.Receive(reader, &msg)
	if err != nil {
		fmt.Println(err)
		return false, 0, nil, err
	}

	if len(msg) <= 9 {
		fmt.Printf("The msg is too small: %d\n", len(msg))
		return false, 0, nil, errors.New("Msg is too small")
	}

	hdr := msg[:9]

	is_key_frame := false
	if hdr[0] == 1 {
		is_key_frame = true
	}

	duration := conv2int(hdr[1:5])
	size := conv2int(hdr[5:9])

	if size != len(msg)-9 {
		fmt.Printf("Size unmatch hdr: %d, msg: %d\n", size, len(msg)-9)
		return false, 0, nil, errors.New("Msg size unmatched")
	}

	return is_key_frame, duration, msg[9:], nil
}

func process(writer *websocket.Conn, reader *websocket.Conn) error {
	var mp4writer = NewMP4()
	defer mp4writer.Release()

	frame_ch = make(chan []byte, 1)

	go write_frame(writer)

	for {
		is_key_frame, duration, buf, err := read_buffer(reader)
		if err != nil {
			frame_ch <- make([]byte, 0)
			return err
		}

		size := len(buf)
		fmt.Printf("frame: %v, %d, %d\n", is_key_frame, duration, size)

		if duration == 0 {
			fmt.Println("Frame with 0 duration is found. Drop it")
			continue
		}

		// Do some prcocess
		err = mp4writer.WriteH264Sample(buf, uint(size), is_key_frame, uint64(duration))
		if err != nil {
			fmt.Println(err)
		}
	}

	return nil
}

func cameraHandler(ws *websocket.Conn) {
	//fmt.Printf("Camera connected from %s\n", ws.RemoteAddr().String())
	fmt.Printf("Camera connected\n")

	select {
	case camera_ch <- ws:
		fmt.Println("camera start")
		<-err_ch
	case w := <-client_ch:
		fmt.Println("camera --> client")
		err := process(w, ws)
		err_ch <- err
	}

	fmt.Println("camera quit")
}

func clientHandler(ws *websocket.Conn) {
	//fmt.Printf("Client connected from %s\n", ws.RemoteAddr().String())
	fmt.Printf("Client connected\n")

	select {
	case client_ch <- ws:
		fmt.Println("client start")
		<-err_ch
	case w := <-camera_ch:
		fmt.Println("client --> camera")
		err := process(ws, w)
		err_ch <- err
	}

	fmt.Println("client quit")
}

func runHttps(port int, cert_path, key_path string) error {
	cert, err := tls.LoadX509KeyPair(cert_path, key_path)
	if err != nil {
		fmt.Println(err)
		return errors.New(err.Error())
	}

	var config tls.Config
	config.Certificates = append(config.Certificates, cert)

	ln, err := tls.Listen("tcp", fmt.Sprintf(":%d", port), &config)
	if err != nil {
		fmt.Println(err)
		return errors.New(err.Error())
	}

	http.Serve(ln, nil)

	return nil
}

func runHttp(port int) error {
	http.ListenAndServe(fmt.Sprintf(":%d", port), nil)

	return nil
}

func main() {
	var is_ssl bool
	var cert_path, key_path string
	var port int

	flag.BoolVar(&is_ssl, "ssl", false, "Enable SSL")
	flag.StringVar(&cert_path, "cert", "", "Certificate path")
	flag.StringVar(&key_path, "key", "", "Key path")
	flag.IntVar(&port, "port", 8080, "Port")
	flag.Parse()

	if is_ssl {
		if cert_path == "" || key_path == "" {
			fmt.Println("Please specify certificate or key path")
			os.Exit(1)
		}

		fmt.Printf("Cert: %s\n", cert_path)
		fmt.Printf("Key: %s\n", key_path)
	}

	http.Handle("/camera", NoOrigHandler{cameraHandler})
	http.Handle("/client", NoOrigHandler{clientHandler})

	fmt.Printf("Server start on %d\n", port)
	if is_ssl {
		runHttps(port, cert_path, key_path)
	} else {
		runHttp(port)
	}
}

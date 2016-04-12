package main

import (
	"golang.org/x/net/websocket"
	"net/http"
)

type NoOrigHandler struct {
	websocket.Handler
}

func (h NoOrigHandler) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	s := websocket.Server{Handler: h.Handler}
	s.ServeHTTP(w, req)
}

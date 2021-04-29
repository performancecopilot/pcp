#!/bin/bash

(sleep 1; xdg-open "http://127.0.0.1:8000") &
python3 -m http.server --bind 127.0.0.1

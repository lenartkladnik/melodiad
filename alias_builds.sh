#!/bin/bash

function _build_template() {
  mkdir -p build
  cd build
  cmake --preset "$1" ..
  cd ..
  cmake --build "build/$1" --preset "$1"
}

function build_debug_l0() {
  _build_template "linux-debug"
}

function build_debug_l1() {
  _build_template "linux-debug-l1"
}

function build_debug() {
  build_debug_l1
}

function build_linux() {
  _build_template "linux-release"
}

function build_windows() {
  _build_template "windows-release"
}

function build_macos() {
  _build_template "macos-release"
}

function build_all() {
  _build_template "linux-release"
  _build_template "windows-release"
  _build_template "macos-release"
}

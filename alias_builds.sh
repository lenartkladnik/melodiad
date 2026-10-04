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

function build_all_archive() {
  v=$1
  if [[ -z $v ]]; then
    v="latest"
  fi
}

function build_all_no_archive() {
  v=$1
  if [[ -z $v ]]; then
    v="latest"
  fi
}

function build_all() {
  v=$1
  if [[ -z $v ]]; then
    v="latest"
  fi
}

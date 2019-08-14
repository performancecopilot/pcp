#!/usr/bin/env ruby
require 'socket'
require 'open3'
require 'inifile'

# TODO: swap ini files to test multiple configurations 

files = Dir.glob(["cases/**/*.rb"], 0)
sorted = files.sort
sorted.each{|s| 
  load s
}

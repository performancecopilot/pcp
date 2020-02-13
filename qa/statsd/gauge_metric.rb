#!/usr/bin/env ruby
require 'open3'
require 'colorize'
require 'socket'

# Test if gauge metric datagrams are processed properly

print "⧖ Checking gauge metric functionality..."
err_count = 0

ds = UDPSocket.new
ds.connect('localhost', 8125)

payloads = [  
  # ok
  0,
  1,
  10, 
  100,
  1000,
  10000,
  "+1",
  "+10",
  "+100",
  "+1000",
  "+10000",
  "-0.1",
  "-0.01",
  "-0.001",
  "-0.0001",
  "-0.00001",
  # thrown away
  Float::MAX,
  Float::MIN,
  "-1wqeqe",
  "-20weqe0",
  "-wqewqe20"
]

gauge_expected_result = 10000 + 11111 - 0.99999

payloads.each { |payload|
  ds.send("test_gauge:" + payload.to_s + "|g", 0)
  sleep(0.1)
} 

# Check received stat

stdout, stderr, status = Open3.capture3("pminfo statsd.test_gauge -f")
if !stderr.empty? && stdout.include?("value " + gauge_expected_result.to_s)
  err_count = err_count + 1
end

print "\r"

$stdout.flush
if err_count == 0
  puts "✔".green + " Gauge stat updating accordingly                 "
else 
  puts "✖".red + " Gauge stat wasn't updated as expected: " + err_count.to_s
end

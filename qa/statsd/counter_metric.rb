#!/usr/bin/env ruby
require 'open3'
require 'colorize'
require 'socket'

# Test if counter metric datagrams are processed properly

print "⧖ Checking counter metric functionality..."
err_count = 0

ds = UDPSocket.new
ds.connect('localhost', 8125)

payloads = [
  # thrown away
  -1,
  Float::MAX,
  "-1wqeqe",
  "-20weqe0",
  "-wqewqe20",
  # ok
  0,
  1,
  10, 
  100,
  1000,
  10000,
  +1,
  +10,
  +100,
  +1000,
  +10000,
  0.1,
  0.01,
  0.001,
  0.0001,
  0.00001
]

counter_expected_result = 22222.1111

payloads.each { |payload|
  ds.send("test_counter:" + payload.to_s + "|c", 0)
  sleep(0.1)
} 

# Check received stat

stdout, stderr, status = Open3.capture3("pminfo statsd.test_counter -f")
if !stderr.empty? && stdout.include?("value " + counter_expected_result.to_s)
  err_count = err_count + 1
end

print "\r"

$stdout.flush
if err_count == 0
  puts "✔".green + " Counter stat updating accordingly                 "
else 
  puts "✖".red + " Counter stat wasn't updated as expected: " + err_count.to_s
end

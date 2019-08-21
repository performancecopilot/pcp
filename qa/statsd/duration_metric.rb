#!/usr/bin/env ruby
require 'open3'
require 'colorize'
require 'socket'

# Test if duration metric datagrams are processed properly

print "⧖ Checking duration metric functionality..."
err_count = 0

ds = UDPSocket.new
ds.connect('localhost', 8125)

# Simple test
(1..100).each { |payload|
  ds.send("test_duration:" + payload.to_s + "|ms", 0)
  sleep(0.001)
}

# Check received stat
min = 1
max = 100
median = 50
average = 50.5
percentile90 = 90
percentile95 = 95
percentile99 = 99
count = 100
std_deviation = 28.86607004772212

stdout, stderr, status = Open3.capture3("pminfo statsd.test_duration -f")
duration_status = ""
if stderr.empty?
  lines = stdout.split "\n"
  if lines[2].include?("value " + min.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'min' is " + min.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'min' is not " + min.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[3].include?("value " + max.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'max' is " + max.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'max' is not " + max.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[4].include?("value " + median.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'median' is " + median.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'median' is not " + median.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[5].include?("value " + average.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'average' is " + average.to_s + "\n"
  else
    puts lines[5]
    duration_status << "- " + "✖".red + " statsd.test_duration 'average' is not " + average.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[6].include?("value " + percentile90.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'percentile90' is " + percentile90.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'percentile90' is not " + percentile90.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[7].include?("value " + percentile95.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'percentile95' is " + percentile95.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'percentile95' is not " + percentile95.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[8].include?("value " + percentile99.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'percentile99' is " + percentile99.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'percentile99' is not " + percentile99.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[9].include?("value " + count.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'count' is " + count.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'count' is not " + count.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[10].include?("value " + std_deviation.to_s)
    duration_status << "- " + "✔".green + " statsd.test_duration 'std_deviation' is " + std_deviation.to_s + "\n"
  else
    duration_status << "- " + "✖".red + " statsd.test_duration 'std_deviation' is not " + std_deviation.to_s + "\n"
    err_count = err_count + 1
  end
else
  err_count = err_count + 1
  duration_status = "- " + "✖".red + "Unable to get statsd.test_duration"
end

print "\r"

$stdout.flush
if err_count == 0
  puts "✔".green + " Duration stat updating accordingly                 "
else 
  puts "✖".red + " Duration stat wasn't updated as expected: " + err_count.to_s
end

puts duration_status

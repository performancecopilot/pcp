#!/usr/bin/env ruby
require 'open3'
require 'colorize'
require 'socket'

# Test if agent activates
# - fails when stderr is not empty

print "⧖ Checking if hardcoded stats update accordingly..."
err_count = 0

ds = UDPSocket.new
ds.connect('localhost', 8125)

# What we are going to send

payloads = [
  "stat_login:1|c",
  "stat_login:3|c",
  "stat_login:5|c",
  "stat_logout:4|c",
  "stat_logout:2|c",
  "stat_logout:2|c",
  "stat_tagged_counter_a,tagX=X:1|c",
  "stat_tagged_counter_a,tagY=Y:2|c",
  "stat_tagged_counter_a,tagZ=Z:3|c",
  "stat_tagged_counter_b:4|c",
  "stat_tagged_counter_b,tagX=X,tagW=W:5|c",
  "stat_success:0|g",
  "stat_success:+5|g",
  "stat_success:-12|g",
  "stat_error:0|g",
  "stat_error:+9|g",
  "stat_error:-0|g",
  "stat_tagged_gauge_a,tagX=X:1|g",
  "stat_tagged_gauge_a,tagY=Y:+2|g",
  "stat_tagged_gauge_a,tagY=Y:-1|g",
  "stat_tagged_gauge_a,tagZ=Z:-3|g",
  "stat_tagged_gauge_b:4|g",
  "stat_tagged_gauge_b,tagX=X,tagW=W:-5|g",
  "stat_cpu_wait:200|ms",
  "stat_cpu_wait:100|ms",
  "stat_cpu_wait:200|ms",
  "stat_cpu_busy:100|ms",
  "stat_cpu_busy:10|ms",
  "stat_cpu_busy:20|ms",
  "stat_cpu_wait,target=cpu0:10|ms",
  "stat_cpu_wait,target=cpu0:100|ms",
  "stat_cpu_wait,target=cpu0:1000|ms",
  "stat_cpu_wait,target=cpu1:20|ms",
  "stat_cpu_wait,target=cpu1:200|ms",
  "stat_cpu_wait,target=cpu1:2000|ms",

  "stat_login:1|g",
  "stat_login:3|g",
  "stat_login:5|g",
  "stat_logout:4|g",
  "stat_logout:2|g",
  "stat_logout:2|g",
  "stat_login:+0.5|g",
  "stat_login:-0.12|g",
  "stat_logout:0.128|g",
  "session_started:-1|c",
  "cache_cleared:-4|c",
  "cache_cleared:-1|c",
  "session_started:1wq|c",
  "cache_cleared:4ěš|c",
  "session_started:1_4w|c",
  "session_started:1|cx",
  "cache_cleared:4|cw",
  "cache_cleared:1|rc",
  "session_started:|c",
  ":20|c",
  "session_started:1wq|g",
  "cache_cleared:4ěš|g",
  "session_started:1_4w|g",
  "session_started:-we|g",
  "cache_cleared:-0ě2|g",
  "cache_cleared:-02x|g",
  "session_started:1|gx",
  "cache_cleared:4|gw",
  "cache_cleared:1|rg",
  "session_started:|g",
  "cache_cleared:|g",
  "session_duration:|ms",
  "cache_loopup:|ms",
  "session_duration:1wq|ms",
  "cache_cleared:4ěš|ms",
  "session_started:1_4w|ms",
  "session_started:-1|ms",
  "cache_cleared:-4|ms",
  "cache_cleared:-1|ms",
  "session_started:1|mss",
  "cache_cleared:4|msd",
  "cache_cleared:1|msa",
  "session_started:|ms",
  ":20|ms"
]

payloads.each { |payload|
  ds.send(payload, 0)
  sleep(0.1)
} 

# Expected ammounts

received_count = payloads.length
parsed_count = 50
aggregated_count = 35
dropped_count = 44
metrics_tracked_counter = 4
metrics_tracked_gauge = 4
metrics_tracked_duration = 2
metrics_tracked_total = 10

## Check recorded totals

## Check received stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.received -f")
received_status = ""
if stderr.empty? && stdout.include?("value " + received_count.to_s)
  received_status = "- " + "✔".green + " statsd.pmda.received is " + received_count.to_s
else
  received_status = "- " + "✖".red + " statsd.pmda.received is not " + received_count.to_s
  err_count = err_count + 1
end

## Check parsed stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.parsed -f")
parsed_status = ""
if stderr.empty? && stdout.include?("value " + parsed_count.to_s)
  parsed_status = "- " + "✔".green + " statsd.pmda.parsed is " + parsed_count.to_s
else
  parsed_status = "- " + "✖".red + " statsd.pmda.parsed is not " + parsed_count.to_s
  err_count = err_count + 1
end

## Check aggregated stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.aggregated -f")
aggregated_status = ""
if stderr.empty? && stdout.include?("value " + aggregated_count.to_s)
  aggregated_status = "- " + "✔".green + " statsd.pmda.aggregated is " + aggregated_count.to_s
else
  aggregated_status = "- " + "✖".red + " statsd.pmda.aggregated is not 0" + aggregated_count.to_s
  err_count = err_count + 1
end

## Check dropped stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.dropped -f")
dropped_status = ""
if stderr.empty? && stdout.include?("value " + dropped_count.to_s)
  dropped_status = "- " + "✔".green + " statsd.pmda.dropped is " + dropped_count.to_s
else
  dropped_status = "- " + "✖".red + " statsd.pmda.dropped is not " + dropped_count.to_s
  err_count = err_count + 1
end

## Check time spent aggregating stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.time_spent_aggregating -f")
time_spent_aggregating_status = ""
if stderr.empty? && !stdout.include?("value 0")
  time_spent_aggregating_status = "- " + "✔".green + " statsd.pmda.time_spent_aggregating is not 0"
else
  time_spent_aggregating_status = "- " + "✖".red + " statsd.pmda.time_spent_aggregating is 0 "
  err_count = err_count + 1
end

## Check time spent parsing stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.time_spent_parsing -f")
time_spent_parsing_status = ""
if stderr.empty? && !stdout.include?("value 0")
  time_spent_parsing_status = "- " + "✔".green + " statsd.pmda.time_spent_parsing is not 0"
else
  time_spent_parsing_status = "- " + "✖".red + " statsd.pmda.time_spent_parsing is 0 "
  err_count = err_count + 1
end

## Check tracked metrics stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.metrics_tracked -f");
metrics_tracked_status = ""
if stderr.empty?
  lines = stdout.split("\n")
  if lines[2].include?("value " + metrics_tracked_counter.to_s)
    metrics_tracked_status << "- " + "✔".green + " statsd.pmda.metrics_tracked 'counter' is " + metrics_tracked_counter.to_s + "\n"
  else
    metrics_tracked_status << "- " + "✖".red + " statsd.pmda.metrics_tracked 'counter' is not " + metrics_tracked_counter.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[3].include?("value " + metrics_tracked_gauge.to_s)
    metrics_tracked_status << "- " + "✔".green + " statsd.pmda.metrics_tracked 'gauge' is " + metrics_tracked_gauge.to_s + "\n"
  else
    metrics_tracked_status << "- " + "✖".red + " statsd.pmda.metrics_tracked 'gauge' is not " + metrics_tracked_gauge.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[4].include?("value " + metrics_tracked_duration.to_s)
    metrics_tracked_status << "- " + "✔".green + " statsd.pmda.metrics_tracked 'duration' is " + metrics_tracked_duration.to_s + "\n"
  else
    metrics_tracked_status << "- " + "✖".red + " statsd.pmda.metrics_tracked 'duration' is not " + metrics_tracked_duration.to_s + "\n"
    err_count = err_count + 1
  end
  if lines[5].include?("value " + metrics_tracked_total.to_s)
    metrics_tracked_status << "- " + "✔".green + " statsd.pmda.metrics_tracked 'total' is " + metrics_tracked_total.to_s + "\n"
  else
    metrics_tracked_status << "- " + "✖".red + " statsd.pmda.metrics_tracked 'total' is not " + metrics_tracked_total.to_s + "\n"
    err_count = err_count + 1
  end
else
  metrics_tracked_status = "- " + "✖".red + "Unable to get statsd.pmda.metrics_tracked"
  err_count = err_count + 1
end

print "\r"

$stdout.flush
if err_count == 0
  puts "✔".green + " Hardcoded stats update accordingly                 "
else 
  puts "✖".red + " Hardcoded stats are not updated_accordingly: " + err_count.to_s
end

puts received_status
puts parsed_status
puts aggregated_status
puts dropped_status
puts time_spent_aggregating_status
puts time_spent_parsing_status
puts metrics_tracked_status

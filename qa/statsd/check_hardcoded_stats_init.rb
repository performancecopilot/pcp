#!/usr/bin/env ruby
require 'open3'
require 'colorize'

# Checks if all hardcoded stats are initialized to 0
# - fails when they are not

print "⧖ Checking initial state for hardcoded stats..."
err_count = 0

## Check received stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.received -f")
received_status = ""
if stderr.empty? && stdout.include?("value 0")
  received_status = "- " + "✔".green + " statsd.pmda.received is 0"
else
  received_status = "- " + "✖".red + " statsd.pmda.received is not 0"
  err_count = err_count + 1
end

## Check parsed stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.parsed -f")
parsed_status = ""
if stderr.empty? && stdout.include?("value 0")
  parsed_status = "- " + "✔".green + " statsd.pmda.parsed is 0"
else
  parsed_status = "- " + "✖".red + " statsd.pmda.parsed is not 0"
  err_count = err_count + 1
end

## Check aggregated stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.aggregated -f")
aggregated_status = ""
if stderr.empty? && stdout.include?("value 0")
  aggregated_status = "- " + "✔".green + " statsd.pmda.aggregated is 0"
else
  aggregated_status = "- " + "✖".red + " statsd.pmda.aggregated is not 0"
  err_count = err_count + 1
end

## Check dropped stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.dropped -f")
dropped_status = ""
if stderr.empty? && stdout.include?("value 0")
  dropped_status = "- " + "✔".green + " statsd.pmda.dropped is 0"
else
  dropped_status = "- " + "✖".red + " statsd.pmda.dropped is not 0"
  err_count = err_count + 1
end

## Check time spent aggregating stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.time_spent_aggregating -f")
time_spent_aggregating = ""
if stderr.empty? && stdout.include?("value 0")
  time_spent_aggregating = "- " + "✔".green + " statsd.pmda.time_spent_aggregating is 0"
else
  time_spent_aggregating = "- " + "✖".red + " statsd.pmda.time_spent_aggregating is not 0"
  err_count = err_count + 1
end

## Check time spent parsing stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.time_spent_parsing -f")
time_spent_parsing = ""
if stderr.empty? && stdout.include?("value 0")
  time_spent_parsing = "- " + "✔".green + " statsd.pmda.time_spent_parsing is 0"
else
  time_spent_parsing = "- " + "✖".red + " statsd.pmda.time_spent_parsing is not 0"
  err_count = err_count + 1
end

## Check tracked metrics stat

stdout, stderr, status = Open3.capture3("pminfo statsd.pmda.metrics_tracked -f");
metrics_tracked = ""
if stderr.empty?
  lines = stdout.split("\n")
  if lines[2].include?("value 0")
    metrics_tracked << "- " + "✔".green + " statsd.pmda.metrics_tracked 'counter' is 0\n"
  else
    metrics_tracked << "- " + "✖".red + " statsd.pmda.metrics_tracked 'counter' is not 0\n"
    err_count = err_count + 1
  end
  if lines[3].include?("value 0")
    metrics_tracked << "- " + "✔".green + " statsd.pmda.metrics_tracked 'gauge' is 0\n"
  else
    metrics_tracked << "- " + "✖".red + " statsd.pmda.metrics_tracked 'gauge' is not 0\n"
    err_count = err_count + 1
  end
  if lines[4].include?("value 0")
    metrics_tracked << "- " + "✔".green + " statsd.pmda.metrics_tracked 'duration' is 0\n"
  else
    metrics_tracked << "- " + "✖".red + " statsd.pmda.metrics_tracked 'duration' is not 0\n"
    err_count = err_count + 1
  end
  if lines[5].include?("value 0")
    metrics_tracked << "- " + "✔".green + " statsd.pmda.metrics_tracked 'total' is 0\n"
  else
    metrics_tracked << "- " + "✖".red + " statsd.pmda.metrics_tracked 'total' is not 0\n"
    err_count = err_count + 1
  end
else
  metrics_tracked = "- " + "✖".red + "Unable to get statsd.pmda.metrics_tracked"
  err_count = err_count + 1
end

## Shows results 

print "\r"

$stdout.flush
if err_count == 0
  puts "✔".green + " Hardcoded stats are set to 0 on startup                  "
else 
  puts "✖".red + " Hardcoded stats are not set to 0 on startup: " + err_count.to_s
end

puts received_status
puts parsed_status
puts aggregated_status
puts dropped_status
puts time_spent_aggregating
puts time_spent_parsing
puts metrics_tracked

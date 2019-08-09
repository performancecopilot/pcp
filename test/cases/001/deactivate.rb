#!/usr/bin/env ruby
require 'open3'
require 'colorize'

# Test if agent deactivates
# - fails when stderr is not empty

print "⧖ Deactivating agent..."

stdout, stderr, status = Open3.capture3(". /etc/pcp.conf && cd $PCP_PMDAS_DIR/statsd && sudo make deactivate")

print "\r"

$stdout.flush
if stderr.empty?
  puts "✔".green + " Agent deactivated successfully"
else
  puts "✖".red + " Failed to deactivate agent"
  puts stderr
end

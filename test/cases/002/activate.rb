#!/usr/bin/env ruby
require 'open3'
require 'colorize'

# Test if agent activates
# - fails when stderr is not empty

print "⧖ Activating agent..."

stdout, stderr, status = Open3.capture3(". /etc/pcp.conf && cd $PCP_PMDAS_DIR/statsd && sudo make activate")

print "\r"

$stdout.flush
if stderr.empty?
  puts "✔".green + " Agent activated successfully"
else
  puts "✖".red + " Failed to activate agent"
  puts stderr
end

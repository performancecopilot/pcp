#!/usr/bin/env ruby
require 'socket'

N = 1000000
ds = UDPSocket.new
ds.connect('localhost', 8125)
ts = []

ts << Thread.new do
  N.times do
    ds.send("thread_1.counter:1|c", 0)
  end
  puts "thread_1 done"
end

ts << Thread.new do
  N.times do
    ds.send("thread_2.counter:2|c", 0)
  end
  puts "thread_2 done"
end

ts << Thread.new do
  N.times do
    ds.send("thread_3.gauge:#{Random.rand(500)}|g", 0)
  end
  puts "thread_3 done"
end

ts << Thread.new do
  time_spent = 0.001
  N.times do
    start = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    ds.send("thread_4.ms:#{sprintf('%8.8f', time_spent * 1000)}|ms", 0)
    finish = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    time_spent = finish - start
  end
  puts "thread_4 done"
end

ts.each(&:join)

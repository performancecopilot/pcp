from prometheus_client import start_http_server, Summary, \
	Counter, Gauge, Histogram
import time
import sys

requests = Counter('requests_total', 'Total requests', ['type', 'agent'])
requests.labels(type='fetch', agent='prometheus').inc()
requests.labels(type='store', agent='json').inc()
requests.labels(type='oneline', agent='docker').inc()

histogram = Histogram('request_latency_seconds', 'Sample histogram')
histogram.observe(4.7)

queue = Gauge('queue_size', 'Sample Gauge of queue size')
queue.set(100)
queue.dec(10)

summary = Summary('summary_requests', 'Sample summary of requests')
summary.observe(4.7)


if __name__ == '__main__':
	start_http_server(8000)
	try:
		time.sleep(1000)
	except:
		sys.exit(0)
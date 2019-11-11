#!/bin/bash
distribution=${1?usage: $0 distribution}
artifacts=$(mktemp -d)

maybe_start_shell() {
    if [ "$?" -ne 0 ]; then
        echo "Info: Artifacts are stored in /artifacts inside the container"
        echo "Previous command failed, starting shell:"
        docker exec -it pcp-qa bash
    fi
}

echo 'Build Performance Co-Pilot'
docker rm -f pcp-qa 2>/dev/null
docker build -f build/ci/containers/Dockerfile.${distribution} -t pcp-qa .
docker run -d --privileged \
      -v /lib/modules:/lib/modules:ro \
      -v /usr/src:/usr/src:ro \
      -v ${artifacts}:/artifacts \
      --name pcp-qa pcp-qa
docker exec pcp-qa bash -c 'touch /var/lib/pcp/pmdas/{simple,sample}/.NeedInstall'
docker exec pcp-qa bash -c 'systemctl restart pmcd || journalctl -xe'
docker exec pcp-qa bash -c 'cd /var/lib/pcp/testsuite && ./check 002 && rm check.time'
docker exec pcp-qa bash -c 'mkdir /artifacts/{test-logs,pcp-logs}'

echo 'Run tests: Sanity'
#docker exec pcp-qa bash -o pipefail -c 'cd /var/lib/pcp/testsuite && ./check -g sanity | tee /artifacts/test-logs/sanity.log >(/pcp/build/ci/gen_junit_report.py > /artifacts/test-logs/sanity.xml)'
maybe_start_shell

echo 'Run tests: BCC PMDA'
#docker exec pcp-qa bash -o pipefail -c 'cd /var/lib/pcp/testsuite && ./check -g pmda.bcc | tee /artifacts/test-logs/pmda.bcc.log >(/pcp/build/ci/gen_junit_report.py > /artifacts/test-logs/pmda.bcc.xml)'
maybe_start_shell

echo 'Run tests: bpftrace PMDA'
docker exec pcp-qa bash -o pipefail -c 'cd /var/lib/pcp/testsuite && ./check -g pmda.bpftrace | tee /artifacts/test-logs/pmda.bpftrace.log >(/pcp/build/ci/gen_junit_report.py > /artifacts/test-logs/pmda.bpftrace.xml)'
maybe_start_shell

echo 'Copy artifacts'
docker exec pcp-qa bash -c 'cp -r /var/log/pcp/* /artifacts/pcp-logs'
docker exec pcp-qa bash -c 'cp -r /packages /artifacts'

echo "Info: Artifacts are stored in ${artifacts} on the host"

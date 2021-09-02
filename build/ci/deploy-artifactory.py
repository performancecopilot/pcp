#!/usr/bin/env python3
import sys
import os
import argparse
from typing import Dict, List
import yaml
import re
import requests
import hashlib
import datetime


class TimeoutHTTPAdapter(requests.adapters.HTTPAdapter):
    """
    https://findwork.dev/blog/advanced-usage-python-requests-timeouts-retries-hooks/
    """

    def __init__(self, *args, **kwargs):
        if "timeout" in kwargs:
            self.timeout = kwargs["timeout"]
            del kwargs["timeout"]
        else:
            raise Exception("Please specify a timeout.")
        super().__init__(*args, **kwargs)

    def send(self, request, **kwargs):
        timeout = kwargs.get("timeout")
        if timeout is None:
            kwargs["timeout"] = self.timeout
        return super().send(request, **kwargs)


class ArtifactoryApi:
    def __init__(
        self,
        endpoint: str,
        user: str,
        password: str,
        gpg_passphrase: str,
        timeout=20 * 60,
    ):
        self.endpoint = endpoint
        self.user = user
        self.password = password
        self.gpg_passphrase = gpg_passphrase

        self.session = requests.Session()
        retries = requests.packages.urllib3.util.retry.Retry(
            total=3, backoff_factor=10, status_forcelist=[429, 500, 502, 503, 504]
        )
        self.session.mount(self.endpoint, TimeoutHTTPAdapter(timeout=timeout, max_retries=retries))

    @staticmethod
    def get_checksums(path: str):
        md5 = hashlib.md5()
        sha1 = hashlib.sha1()
        sha256 = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                md5.update(chunk)
                sha1.update(chunk)
                sha256.update(chunk)
        return [md5.hexdigest(), sha1.hexdigest(), sha256.hexdigest()]

    def deploy(
        self,
        repository: str,
        artifact_path: str,
        target_path="",
        params=None,
    ) -> Dict:
        # artifactory requires all checksums, otherwise a warning will be displayed
        md5, sha1, sha256 = self.get_checksums(artifact_path)
        file_name = os.path.basename(artifact_path)
        _, file_ext = os.path.splitext(file_name)
        artifact_info = {
            "type": file_ext[1:],
            "md5": md5,
            "sha1": sha1,
            "sha256": sha256,
            "name": file_name,
        }
        params_str = ";".join([f"{k}={v}" for k, v in (params or {}).items()])

        print(f"Uploading {file_name} to {self.endpoint}/{repository}{target_path}/{file_name}")
        with open(artifact_path, "rb") as f:
            r = self.session.put(
                f"{self.endpoint}/{repository}{target_path}/{file_name};{params_str}",
                auth=(self.user, self.password),
                headers={"X-Checksum-Sha1": sha1, "X-Checksum-Sha256": sha256},
                data=f,
            )
        print(r.text)
        r.raise_for_status()
        print()
        return artifact_info

    def calculate_yum_metadata(self, repository):
        print(f"Calculating and signing metadata of {repository}")
        r = self.session.post(
            f"{self.endpoint}/api/yum/{repository}?async=1",
            auth=(self.user, self.password),
            headers={"X-GPG-PASSPHRASE": self.gpg_passphrase},
        )
        print(r.text)
        r.raise_for_status()
        print()

    def calculate_deb_metadata(self, repository):
        print(f"Calculating and signing metadata of {repository}")
        r = self.session.post(
            f"{self.endpoint}/api/deb/reindex/{repository}?async=1",
            auth=(self.user, self.password),
            headers={"X-GPG-PASSPHRASE": self.gpg_passphrase},
        )
        print(r.text)
        r.raise_for_status()
        print()

    def upload_build_info(self, version: str, build_name: str, build_number: str, build_artifacts: List):
        print(f"Uploading build information of build {build_name} #{build_number}")
        r = self.session.put(
            f"{self.endpoint}/api/build",
            auth=(self.user, self.password),
            json={
                "name": build_name,
                "version": version,
                "number": build_number,
                "started": datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%S.000+00:00"),
                "modules": [{"id": "pcp", "artifacts": build_artifacts}],
            },
        )
        print(r.text)
        r.raise_for_status()
        print()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", default="https://performancecopilot.jfrog.io/artifactory")
    parser.add_argument("--user", default=os.environ.get("ARTIFACTORY_USER"))
    parser.add_argument("--token", default=os.environ.get("ARTIFACTORY_TOKEN"))
    parser.add_argument("--gpg_passphrase", default=os.environ.get("ARTIFACTORY_GPG_PASSPHRASE"))

    parser.add_argument("--maturity", default="snapshot")
    parser.add_argument("--version", required=True)
    parser.add_argument("--build_name", required=True)
    parser.add_argument("--build_number", required=True)
    parser.add_argument("--source")

    parser.add_argument("--exclude", action="append")
    parser.add_argument("artifact_dir", nargs="*")

    args = parser.parse_args()
    if not args.user or not args.token or not args.gpg_passphrase:
        parser.print_help()
        sys.exit(1)

    artifactory = ArtifactoryApi(args.endpoint, args.user, args.token, args.gpg_passphrase)

    if args.source:
        artifactory.deploy(f"pcp-source-{args.maturity}", args.source)

    updated_repositories = set()
    for artifact_dir in args.artifact_dir:
        # ex. build-fedora31-container
        artifact_type, platform_name = os.path.basename(artifact_dir).split("-", 1)
        if artifact_type != "build":
            continue

        platform_def_path = os.path.join(os.path.dirname(__file__), f"platforms/{platform_name}.yml")
        with open(platform_def_path, encoding="utf-8") as f:
            platform = yaml.safe_load(f)

        if "artifactory" not in platform:
            print(f"Skipping {platform_name}: artifactory is not configured in {platform_name}.yml")
            continue

        artifactory_params = platform["artifactory"]
        repository_key = f"pcp-{artifactory_params['package_type']}-{args.maturity}"
        deploy_path = artifactory_params["deploy_path"]
        build_name = f"{args.build_name} {platform_name}"  # e.g. "CI fedora33"

        artifact_params = artifactory_params.get("params", {})
        # required for linking artifacts to builds
        # https://jfrog.com/knowledge-base/how-artifactory-maps-published-artifacts-to-builds-and-why-sometimes-the-paths-to-them-dont-appear/
        artifact_params["build.name"] = build_name
        artifact_params["build.number"] = args.build_number

        build_artifacts = []
        for artifact_filename in os.listdir(artifact_dir):
            if any(re.match(pattern, artifact_filename) for pattern in args.exclude):
                print(f"Skipping {artifact_filename}\n")
                continue

            artifact_path = os.path.join(artifact_dir, artifact_filename)
            build_artifact = artifactory.deploy(
                repository_key,
                artifact_path,
                target_path=deploy_path,
                params=artifact_params,
            )
            build_artifacts.append(build_artifact)

        artifactory.upload_build_info(args.version, build_name, args.build_number, build_artifacts)
        updated_repositories.add(repository_key)

    # schedule recalculation and GPG signing of repository metadata
    for repository in updated_repositories:
        if "-rpm-" in repository:
            artifactory.calculate_yum_metadata(repository)
        elif "-deb-" in repository:
            artifactory.calculate_deb_metadata(repository)


if __name__ == "__main__":
    main()

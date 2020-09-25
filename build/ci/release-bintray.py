#!/usr/bin/env python3
# pylint: disable=too-many-arguments
import sys
import os
import argparse
import yaml
import requests

PACKAGE_LICENSES = ['GPL-2.0']
PACKAGE_VCS_URL = 'https://github.com/performancecopilot/pcp'


class TimeoutHTTPAdapter(requests.adapters.HTTPAdapter):
    """
    https://findwork.dev/blog/advanced-usage-python-requests-timeouts-retries-hooks/
    """

    def __init__(self, *args, **kwargs):
        if "timeout" in kwargs:
            self.timeout = kwargs["timeout"]
            del kwargs["timeout"]
        else:
            raise Exception('Please specify a timeout.')
        super().__init__(*args, **kwargs)

    def send(self, request, **kwargs):
        timeout = kwargs.get("timeout")
        if timeout is None:
            kwargs["timeout"] = self.timeout
        return super().send(request, **kwargs)


class BintrayApi:

    def __init__(self, subject: str, user: str, apikey: str, gpg_passphrase: str, endpoint='https://api.bintray.com',
                 timeout=10*60):
        self.subject = subject
        self.user = user
        self.apikey = apikey
        self.gpg_passphrase = gpg_passphrase
        self.endpoint = endpoint

        self.session = requests.Session()
        retries = requests.packages.urllib3.util.retry.Retry(
            total=3, backoff_factor=10, status_forcelist=[429, 500, 502, 503, 504])
        self.session.mount(self.endpoint, TimeoutHTTPAdapter(timeout=timeout, max_retries=retries))

    def setup_repository(self, repository, repository_type, repository_description):
        r = self.session.get(
            f"{self.endpoint}/repos/{self.subject}/{repository}",
            auth=(self.user, self.apikey),
        )
        if r.status_code == 404:
            print(f"Creating repository bintray.com/{self.subject}/{repository}")
            r = self.session.post(
                f"{self.endpoint}/repos/{self.subject}/{repository}",
                auth=(self.user, self.apikey),
                json={
                    'type': repository_type,
                    'desc': repository_description,
                    'gpg_use_owner_key': True,
                },
            )
            print(r.text)
            r.raise_for_status()
            print()

    def setup_package(self, repository, package):
        r = self.session.get(
            f"{self.endpoint}/packages/{self.subject}/{repository}/{package}",
            auth=(self.user, self.apikey),
        )
        if r.status_code == 404:
            print(f"Creating package bintray.com/{self.subject}/{repository}/{package}")
            r = self.session.post(
                f"{self.endpoint}/packages/{self.subject}/{repository}",
                auth=(self.user, self.apikey),
                json={
                    'name': package,
                    'licenses': PACKAGE_LICENSES,
                    'vcs_url': PACKAGE_VCS_URL
                },
            )
            print(r.text)
            r.raise_for_status()
            print()

    def upload(self, repository, package, version, params, path):
        file_name = os.path.basename(path)
        params = ';'.join([f"{k}={v}" for k, v in params.items()])

        print(f"Uploading {file_name} to bintray.com/{self.subject}/{repository}/{package}/{version}")
        with open(path, 'rb') as f:
            r = self.session.put(
                f"{self.endpoint}/content/{self.subject}/{repository}/{package}/{version}/{file_name};{params}",
                auth=(self.user, self.apikey),
                headers={'X-GPG-PASSPHRASE': self.gpg_passphrase},
                data=f,
            )
        print(r.text)
        if r.status_code not in [200, 409]:
            # ignore HTTP 409: An artifact with the path ... already exists [under another version]
            r.raise_for_status()
        print()

    def sign_version(self, repository, package, version):
        print(f"Signing version bintray.com/{self.subject}/{repository}/{package}/{version}")
        r = self.session.post(
            f"{self.endpoint}/gpg/{self.subject}/{repository}/{package}/versions/{version}",
            auth=(self.user, self.apikey),
            headers={'X-GPG-PASSPHRASE': self.gpg_passphrase},
        )
        print(r.text)
        r.raise_for_status()
        print()

    def sign_metadata(self, repository, package, version):
        print(f"Signing metadata of bintray.com/{self.subject}/{repository}")
        r = self.session.post(
            f"{self.endpoint}/calc_metadata/{self.subject}/{repository}",
            auth=(self.user, self.apikey),
            headers={'X-GPG-PASSPHRASE': self.gpg_passphrase},
        )
        print(r.text)
        r.raise_for_status()
        print()

    def publish(self, repository, package, version):
        print(f"Publish version bintray.com/{self.subject}/{repository}/{package}/{version}")
        r = self.session.post(
            f"{self.endpoint}/content/{self.subject}/{repository}/{package}/{version}/publish",
            auth=(self.user, self.apikey),
            headers={'X-GPG-PASSPHRASE': self.gpg_passphrase},
        )
        print(r.text)
        r.raise_for_status()
        print()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--subject', default=os.environ.get('BINTRAY_SUBJECT', 'pcp'))
    parser.add_argument('--package', default=os.environ.get('BINTRAY_PACKAGE', 'pcp'))
    parser.add_argument('--user', default=os.environ.get('BINTRAY_USER'))
    parser.add_argument('--apikey', default=os.environ.get('BINTRAY_APIKEY'))
    parser.add_argument('--gpg_passphrase', default=os.environ.get('BINTRAY_GPG_PASSPHRASE'))
    parser.add_argument('--version', required=True)
    parser.add_argument('--source')
    parser.add_argument('artifact', nargs='*')

    args = parser.parse_args()
    if not args.user or not args.apikey or not args.gpg_passphrase:
        parser.print_help()
        sys.exit(1)

    bintray = BintrayApi(args.subject, args.user, args.apikey, args.gpg_passphrase)
    repositories_to_publish = []

    if args.source:
        bintray.upload('source', args.package, args.version, {}, args.source)
        repositories_to_publish.append('source')

    for artifact_dir in args.artifact:
        # ex. build-fedora31-container
        artifact, platform_name, _runner = os.path.basename(artifact_dir).split('-')
        if artifact != 'build':
            continue

        platform_def_path = os.path.join(os.path.dirname(__file__), f"platforms/{platform_name}.yml")
        with open(platform_def_path) as f:
            platform = yaml.safe_load(f)

        if 'bintray' not in platform:
            print(f"Skipping {platform_name}: bintray is not configured in {platform_name}.yml")
            continue

        bintray_params = platform['bintray'].get('params', {})
        repository_params = platform['bintray']['repository']
        repository = repository_params['name']

        bintray.setup_repository(repository, repository_params['type'], repository_params['description'])
        bintray.setup_package(repository, args.package)

        for artifact_filename in os.listdir(artifact_dir):
            artifact_filepath = os.path.join(artifact_dir, artifact_filename)
            bintray.upload(repository, args.package, args.version, bintray_params, artifact_filepath)

        bintray.sign_version(repository, args.package, args.version)
        bintray.sign_metadata(repository, args.package, args.version)
        repositories_to_publish.append(repository)

    # publish new version for all distributions at the same time
    for repository in repositories_to_publish:
        bintray.publish(repository, args.package, args.version)


if __name__ == '__main__':
    main()

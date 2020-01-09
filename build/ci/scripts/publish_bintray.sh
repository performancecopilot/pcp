#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
artifacts="$2/*"
bintray_repository="${bintray_distribution}"

. ../../VERSION.pcp
version=${PACKAGE_MAJOR}.${PACKAGE_MINOR}.${PACKAGE_REVISION}

for file_path in ${artifacts}
do
  file=$(basename "${file_path}")

  echo "Uploading ${file} to bintray.com/${bintray_subject}/${bintray_repository}/${bintray_package}/${version}"
  curl --silent --show-error --fail --upload-file "${file_path}" --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
    -X PUT -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
    "https://api.bintray.com/content/${bintray_subject}/${bintray_repository}/${bintray_package}/${version}/${file};${bintray_params}"
  echo && echo
done

echo "Signing version bintray.com/${bintray_subject}/${bintray_repository}/${bintray_package}/${version}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/gpg/${bintray_subject}/${bintray_repository}/${bintray_package}/versions/${version};publish=1"
echo && echo

echo "Signing metadata of bintray.com/${bintray_subject}/${bintray_repository}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/calc_metadata/${bintray_subject}/${bintray_repository}"
echo && echo

echo "Publish all files of version bintray.com/${bintray_subject}/${bintray_repository}/${bintray_package}/${version}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/content/${bintray_subject}/${bintray_repository}/${bintray_package}/${version}/publish"
echo

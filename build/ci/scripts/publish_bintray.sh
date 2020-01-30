#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/env.build.sh
artifacts="$2/*"
BINTRAY_REPOSITORY="${BINTRAY_DISTRIBUTION}"
[ "${SNAPSHOT}" != "no" ] && BINTRAY_REPOSITORY="${BINTRAY_REPOSITORY}-nightly"

for file_path in ${artifacts}
do
    file=$(basename "${file_path}")

    echo "Uploading ${file} to bintray.com/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/${PCP_VERSION}"
    curl --silent --show-error --fail --upload-file "${file_path}" --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
      -X PUT -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
      "https://api.bintray.com/content/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/${PCP_VERSION}/${file};${BINTRAY_PARAMS}"
    echo && echo
done

echo "Signing version bintray.com/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/${PCP_VERSION}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/gpg/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/versions/${PCP_VERSION};publish=1"
echo && echo

echo "Signing metadata of bintray.com/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/calc_metadata/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}"
echo && echo

echo "Publish all files of version bintray.com/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/${PCP_VERSION}"
curl --silent --show-error --fail --user ${BINTRAY_USER}:${BINTRAY_APIKEY} \
  -X POST -H "X-GPG-PASSPHRASE: ${BINTRAY_GPG_PASSPHRASE}" \
  "https://api.bintray.com/content/${BINTRAY_SUBJECT}/${BINTRAY_REPOSITORY}/${BINTRAY_PACKAGE}/${PCP_VERSION}/publish"
echo

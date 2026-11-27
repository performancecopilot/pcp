# Signing environment for PCP release artifacts.
#
# Pinned so that RPM and DEB signatures are produced by the same tool
# versions on every release, independent of the ubuntu-latest runner
# image.  debsigs >= 0.2.2 is required for zstd-compressed Debian
# packages and for signature verification.
FROM debian:trixie@sha256:9c6e381421d0f48d1b8133e573f33c185ee16cbf505eda9c4a8a4791b25ea8a6

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	ca-certificates \
	gnupg \
	rpm \
	debsigs \
 && rm -rf /var/lib/apt/lists/*

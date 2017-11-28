ARG VERSION_TAG=latest

FROM swiftnav/buildroot-base:$VERSION_TAG

ADD docker/entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

################################################################################
#
# grpc
#
################################################################################

GRPC_VERSION = v1.12.0
GRPC_SITE = https://github.com/grpc/grpc.git
GRPC_SITE_METHOD = git
GRPC_LICENSE = BSD-3-Clause
GRPC_LICENSE_FILES = LICENSE
GRPC_DEPENDENCIES = gflags gtest c-ares openssl protobuf zlib
GRPC_CONF_OPTS = \
	-DgRPC_ZLIB_PROVIDER=package \
	-DgRPC_CARES_PROVIDER=package \
	-DgRPC_SSL_PROVIDER=package

$(eval $(cmake-package))

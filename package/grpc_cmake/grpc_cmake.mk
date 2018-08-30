################################################################################
#
# grpc
#
################################################################################

GRPC_CMAKE_VERSION = v1.12.0
GRPC_CMAKE_SITE = https://github.com/grpc/grpc.git
GRPC_CMAKE_SITE_METHOD = git
GRPC_CMAKE_LICENSE = BSD-3-Clause
GRPC_CMAKE_LICENSE_FILES = LICENSE
GRPC_CMAKE_DEPENDENCIES = gflags gtest c-ares openssl protobuf zlib
GRPC_CMAKE_CONF_OPTS = \
	-DgRPC_ZLIB_PROVIDER=package \
	-DgRPC_CARES_PROVIDER=package \
	-DgRPC_SSL_PROVIDER=package

$(eval $(cmake-package))

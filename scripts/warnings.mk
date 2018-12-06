# A standard set of warnings for all piksi_buildroot packages:
#
PBR_CC_WARNINGS = \
	-Wmissing-prototypes \
	-Wconversion \
	-Wimplicit \
	-Wshadow \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef -Wuninitialized \
	-Wpointer-arith \
	-Wstrict-prototypes \
	-Wcast-align \
	-Wformat=2 \
	-Wimplicit-function-declaration \
	-Wredundant-decls \
	-Wformat-security \
	-Wall \
	-Wextra \
	-Wno-strict-prototypes \
	-Werror

PBR_CXX_WARNINGS = \
	-Wconversion \
	-Wshadow \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef -Wuninitialized \
	-Wpointer-arith \
	-Wstrict-prototypes \
	-Wcast-align \
	-Wformat=2 \
	-Wredundant-decls \
	-Wformat-security \
	-Wall \
	-Wextra \
	-Werror

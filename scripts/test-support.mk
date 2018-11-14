pbr_proot_valgrind_test = \
	{ cd $(TARGET_DIR); \
	  export PROOT_NO_SECCOMP=1; \
		export _VALGRIND_LIB=$$PWD/../host/x86_64-buildroot-linux-gnu/sysroot/usr/lib/valgrind; \
		test -d $$_VALGRIND_LIB || exit 1; \
		PATH=/bin:/usr/bin:/sbin:/usr/sbin \
		  proot -b $$PWD/../build -b $$PWD/../../../scripts -b $$_VALGRIND_LIB:/usr/lib/valgrind -R . \
			valgrind \
				--suppressions="${BR2_EXTERNAL_piksi_buildroot_PATH}/scripts/valgrind_suppressions.txt" \
				--track-origins=yes \
				--leak-check=full \
				--error-exitcode=1 \
				/usr/bin/$(1); \
	}

pbr_proot_test = \
	{ cd $(TARGET_DIR); \
	  export PROOT_NO_SECCOMP=1; \
		PATH=/bin:/usr/bin:/sbin:/usr/sbin \
		  proot -R . \
				/usr/bin/$(1); \
	}


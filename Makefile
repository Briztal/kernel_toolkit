include /root/projects/mftk/mftk.mk

$(eval $(call mftk.utility.define,toolchain,target,x86_64))
$(eval $(call mftk.utility.execute,toolchain))

INC := -Iinclude/

CFLAGS += -std=c89 -Wall -pedantic -O3 -nostdinc -nostdlib -nodefaultlibs -nolibc

TS_BDIR = build/test

$(eval $(call mftk.node.define,nostd,0,build_dir,$(.wdir)/build/nostd))
$(eval $(call mftk.node.define,nostd,0,build_arch,x86_64))
$(eval $(call mftk.node.define,nostd,0,debug,1))

$(eval $(call mftk.node.define,kerneltk,0,nostd_inc_dir,$(.wdir)/build/nostd/include))
$(eval $(call mftk.node.define,kerneltk,0,build_dir,$(.wdir)/build/kerneltk))
$(eval $(call mftk.node.define,kerneltk,0,build_arch,x86_64))
$(eval $(call mftk.node.define,kerneltk,0,debug,1))

kerneltk.nostd.ar :
	rm -rf build/nostd
	$(call mftk.node.execute,nostd,0,nostd,)

kerneltk.ar :
	rm -rf build/kerneltk
	$(call mftk.node.execute,kerneltk,0,kerneltk,)

clean:
	rm -rf build

all: clean kerneltk.nostd.ar kerneltk.ar

# Top-level Makefile — builds every face under apps/ and demo/ and
# installs the embedded ones into firmware/main/faces/.
#
# Usage:
#   make           # build + install every face
#   make clean     # clean every face
#   make apps      # build + install just the apps
#   make demo      # build + install just the demos

APP_FACES  := $(patsubst %/Makefile,%,$(wildcard apps/*/face/Makefile))
DEMO_FACES := $(patsubst %/Makefile,%,$(wildcard demo/*/Makefile))

.PHONY: all apps demo clean
.PHONY: $(APP_FACES) $(DEMO_FACES)

all: apps demo

apps: $(APP_FACES)

demo: $(DEMO_FACES)

$(APP_FACES) $(DEMO_FACES):
	$(MAKE) -C $@ install

clean:
	@for d in $(APP_FACES) $(DEMO_FACES); do \
	  $(MAKE) -C $$d clean; \
	done

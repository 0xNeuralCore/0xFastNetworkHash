MOCK_CC ?= gcc
MOCK_CFLAGS ?= -std=c11 -Wall -Wextra
MOCK_OBJDIR ?= obj

.DEFAULT_GOAL := all
.PHONY: all clean

all:
	mkdir -p $(MOCK_OBJDIR)
	$(MAKE) -C src -f Makefile.txt libs \
		COMPILER=$(MOCK_CC) \
		'COMPILER_FLAGS=$(MOCK_CFLAGS) -c' \
		'DEPENDENCY_FLAGS=-MMD -MP' \
		RELEASEDIR=../$(MOCK_OBJDIR)/ \
		INCLUDEDIR=../mock/include/

clean:
	$(MAKE) -C src -f Makefile.txt clean \
		RELEASEDIR=../$(MOCK_OBJDIR)/ \
		INCLUDEDIR=../mock/include/

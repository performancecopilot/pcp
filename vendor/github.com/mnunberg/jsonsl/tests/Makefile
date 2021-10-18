TESTMODS= json_test api_test jpr_test unescape cxxtest

all: $(TESTMODS)
	./json_test ../share/*
	./api_test
	./jpr_test
	./unescape
	./json_test ../share/jsc/pass*.json
	JSONSL_FAIL_TESTS=1 ./json_test ../share/jsc/fail*.json
ifneq (,$(findstring JSONSL_PARSE_NAN,$(CFLAGS)))
	./json_test ../share/nan/*.json
else
	JSONSL_FAIL_TESTS=1 ./json_test ../share/nan/*.json
endif
	@echo "   Some tests were skipped."
	@echo "   See share/jsc/nyi_fail/README.skipped for details"
	@echo "All Tests OK"

%: %.c
	echo "LIBFLAGS ${LIBFLAGS}"
	echo "LDFLAGS ${LDFLAGS}"
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTMODS)

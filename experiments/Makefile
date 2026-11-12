SUBPROJECTS := abi-bridge syscall-user-dispatch ntsync-capability pe64-image sud-latency gs-teb-probe

.PHONY: test clean

test:
	@set -e; for project in $(SUBPROJECTS); do \
		echo "=== $$project ==="; \
		$(MAKE) -C "$$project" test; \
	done

clean:
	@set -e; for project in $(SUBPROJECTS); do \
		$(MAKE) -C "$$project" clean; \
	done

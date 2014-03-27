cmake_args=
ifdef VIRTUAL_ENV
	cmake_args=-DCMAKE_INSTALL_PREFIX=$(VIRTUAL_ENV)
	library_path=$(LIBRARY_PATH):$(VIRTUAL_ENV)/lib/
else
	cmake_args=-DCMAKE_INSTALL_PREFIX=..
	library_path=$(LIBRARY_PATH):`pwd`/lib/
endif

cy_deps=
ifdef PYDISTRIBUTIONS_USE_LIB
	install_cy_deps=install_cc
endif

all: test

src/test_headers.cc: FORCE
	find include \
	  | grep '\.hpp' \
	  | sort \
	  | sed 's/include\/\(.*\)/#include <\1>/g' \
	  > src/test_headers.cc
	echo 'int main () { return 0; }' >> src/test_headers.cc

configure_cc: src/test_headers.cc FORCE
	mkdir -p build lib
	cd build && cmake $(cmake_args) ..

build_cc: configure_cc FORCE
	cd build && $(MAKE)

install_cc: build_cc FORCE
	cd build && make install

install_cy: $(install_cy_deps) FORCE
	pip install -r requirements.txt
	LIBRARY_PATH=$(library_path) pip install -e .

install: install_cc install_cy FORCE

test_cc: install_cc FORCE
	cd build && ctest
	./test_cmake.sh
	@echo '----------------'
	@echo 'PASSED CC TESTS'

test_cy: install_cy FORCE
	pyflakes setup.py distributions derivations
	pep8 --repeat --ignore=E265 --exclude=*_pb2.py setup.py distributions derivations
	nosetests -v
	@echo '----------------'
	@echo 'PASSED CY TESTS'

test: test_cc test_cy FORCE
	@echo '----------------'
	@echo 'PASSED ALL TESTS'

protobuf: FORCE
	protoc --cpp_out=include/ --python_out=. distributions/schema.proto
	mv include/distributions/schema.pb.cc src/
	@pyflakes distributions/schema_pb2.py \
	  || (echo '...patching schema_pb2.py' \
	    ; sed -i '/descriptor_pb2/d' distributions/schema_pb2.py)  # HACK

profile: install_cc FORCE
	build/benchmarks/sample_from_scores
	build/benchmarks/sample_assignment_from_py
	build/benchmarks/special

clean: FORCE
	git clean -Xdf

FORCE:

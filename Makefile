.PHONY: tests clean all

APP = kaleidoscope
SOURCEs = $(wildcard *.cpp)
OBJECTs = $(addprefix build/,$(SOURCEs:.cpp=.o))

CXXFLAGS += -g -Wall -std=c++17

tests: $(APP)
	@echo "run tests"
	@./$(APP) 

all: $(APP)

include $(wildcard deps/*.dep)

build/%.o: %.cpp
	@echo "c++ $@"
	@mkdir -p build deps
	@clang $(CXXFLAGS) -c $(notdir $(@:.o=.cpp)) -o $@ -MMD -MF deps/$(notdir $(@:.o=.dep))

$(APP): $(OBJECTs)
	@echo "link $@"
	@clang $^ -o $@ -lLLVM-11 -lstdc++

clean:
	@echo "clean"
	@rm -Rf $(APP) build deps

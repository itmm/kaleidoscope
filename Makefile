.PHONY: all tests clean

APP = kaleidoscope
SOURCEs = $(wildcard *.cpp)
OBJECTs = $(addprefix build/,$(SOURCEs:.cpp=.o))

CXXFLAGS += `llvm-config --cxxflags`

tests: $(APP)
	@echo "run tests"
	@./$(APP) 

all: $(APP)

include $(wildcard deps/*.dep)

build/%.o: %.cpp
	@echo "c++ $@"
	@mkdir -p build deps
	$(CXX) $(CXXFLAGS) -c $(notdir $(@:.o=.cpp)) -o $@ -MMD -MF deps/$(notdir $(@:.o=.dep))

$(APP): $(OBJECTs)
	@echo "link $@"
	$(CXX) $^ -o $@ `llvm-config --libs`

clean:
	@echo "clean"
	@rm -Rf $(APP) build deps

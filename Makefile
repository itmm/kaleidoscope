.PHONY: all tests clean

APP = kaleidoscope
SOURCEs = $(filter-out toy.cpp,$(wildcard *.cpp))
OBJECTs = $(addprefix build/,$(SOURCEs:.cpp=.o))

CXXFLAGS += `llvm-config --cxxflags` -g

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

toy: toy.cpp
	$(CXX) $(CXXFLAGS) toy.cpp `llvm-config --libs` -o toy

clean:
	@echo "clean"
	@rm -Rf $(APP) build deps

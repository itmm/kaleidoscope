.PHONY: tests clean

APP = kaleidoscope
SOURCEs = $(wildcard *.cpp)
OBJECTs = $(addprefix build/,$(SOURCEs:.cpp=.o))

CXXFLAGS += -g -Wall -std=c++17

tests: $(APP)
	@echo "run tests"
	@./$(APP) 

include $(wildcard deps/*.dep)

build/%.o: %.cpp
	@echo "c++ $@"
	@mkdir -p build deps
	@$(CXX) $(CXXFLAGS) -c $(notdir $(@:.o=.cpp)) -o $@ -MMD -MF deps/$(notdir $(@:.o=.dep))

$(APP): $(OBJECTs)
	@echo "link $@"
	@$(CXX) $^ -o $@ -lLLVM-11

clean:
	@echo "clean"
	@rm -Rf $(APP) build deps

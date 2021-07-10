SRCDIR = src
OBJDIR = obj
BINDIR = bin
SRC += $(wildcard $(SRCDIR)/*.cpp)
OBJ += $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEP += $(OBJ:.o=.d)

CXX = g++
CXXFLAGS = -g --std=gnu++11 -MMD -Wall -Wpointer-arith -I./src
CXXFLAGS += -O3
LDFLAGS =
//CXXFLAGS += -fsanitize=address
//LDFLAGS += -fsanitize=address -lasan
dir_guard=@mkdir -p $(@D)

all: $(BINDIR)/pdp11-emu

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(dir_guard)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

$(BINDIR)/pdp11-emu: $(OBJ)
	$(dir_guard)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEP)

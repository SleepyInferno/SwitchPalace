CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../../include -DHOST_BUILD
SOURCES_PFS0 := test_pfs0.cpp ../../source/install/pfs0.cpp
SOURCES_HFS0 := test_hfs0.cpp ../../source/install/hfs0.cpp
TARGET_PFS0 := test_pfs0
TARGET_HFS0 := test_hfs0

.PHONY: all clean

all: $(TARGET_PFS0) $(TARGET_HFS0)
	./$(TARGET_PFS0)
	./$(TARGET_HFS0)
	@echo ""
	@echo "=== All parser tests PASSED ==="

$(TARGET_PFS0): $(SOURCES_PFS0)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_HFS0): $(SOURCES_HFS0)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET_PFS0) $(TARGET_HFS0) *.exe
